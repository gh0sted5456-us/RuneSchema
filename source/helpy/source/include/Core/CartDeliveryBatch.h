#pragma once
#include "Core/ItemCart.h"
#include <set>

namespace PS::CartDeliveryBatch {
using Json=nlohmann::json;
struct Recipient {std::string Path,Name;};
inline bool MissingReceiptExpired(std::int64_t elapsedSeconds,bool runtimeBusy){return elapsedSeconds>30&&!runtimeBusy;}
class Plan {
    std::vector<Recipient> m_players;Json m_items=Json::array(),m_receipts=Json::array();
    size_t m_next=0;uint64_t m_pending=0;bool m_stopped=false,m_equip=false;
public:
    static constexpr size_t MaxPlayers=16;
    void Begin(const Json& items,const std::vector<Recipient>& players,bool equip=false){
        if(m_pending)throw std::runtime_error("A cart delivery is already pending");
        if(players.empty()||players.size()>MaxPlayers)throw std::runtime_error("Choose 1..16 cart recipients");
        (void)ItemCart::KitItems({{"SchemaVersion",1},{"Type","RuneSchemaHelpyKit"},{"Items",items}});
        std::set<std::string> unique;
        for(const auto& player:players){
            // The legacy remote bridge is self-targeted. Do not send a batch
            // of GUID targets through it and accidentally grant to self N times.
            if(player.Path.empty()||(players.size()>1&&player.Path.front()!='/')||player.Path.size()>1024||!unique.insert(player.Path).second)
                throw std::runtime_error("Multi-player delivery requires distinct authoritative host/server players; remote GUID targeting is not available yet");
        }
        m_players=players;m_items=items;m_receipts=Json::array();m_next=0;m_pending=0;m_stopped=false;m_equip=equip;
    }
    bool Pending()const{return m_pending!=0;}
    uint64_t PendingId()const{return m_pending;}
    bool Finished()const{return m_stopped||m_next>=m_players.size();}
    bool Stopped()const{return m_stopped;}
    size_t Completed()const{return m_next;}
    size_t Total()const{return m_players.size();}
    const Json& Receipts()const{return m_receipts;}
    Json Request(uint64_t id){
        if(!id||Pending()||Finished())throw std::runtime_error("No cart recipient is ready");m_pending=id;
        return {{"_RequestId",id},{"Action","GiveItems"},{"_Source","HelpyStandaloneCart"},{"Player",m_players[m_next].Path},{"Items",m_items},{"EquipOnReceipt",m_equip}};
    }
    static std::string Outcome(const Json& receipt,const Json& item){
        const auto rows=receipt.find("GrantResults");if(rows==receipt.end()||!rows->is_array()||rows->size()>ItemCart::Model::Limit)return "Unconfirmed";
        size_t matches=0;std::string outcome="Unconfirmed";
        for(const auto& row:*rows){if(!row.is_object())continue;const auto path=row.find("Item");if(path==row.end()||!path->is_string()||*path!=item.at("Item"))continue;
            ++matches;const auto count=row.find("Count"),state=row.find("State");
            if(count!=row.end()&&count->is_number_integer()&&*count==item.at("Count")&&state!=row.end()&&state->is_string()&&(*state=="Confirmed"||*state=="Rejected"))outcome=state->get<std::string>();}
        return matches==1?outcome:"Unconfirmed";
    }
    static bool Confirmed(const Json& receipt,const Json& item){return Outcome(receipt,item)=="Confirmed";}
    bool Accept(uint64_t id,const Json& receipt){
        if(!m_pending||id!=m_pending||Finished())return false;
        const bool complete=std::all_of(m_items.begin(),m_items.end(),[&](const auto& item){return Outcome(receipt,item)!="Unconfirmed";});
        m_receipts.push_back({{"Player",m_players[m_next].Path},{"Name",m_players[m_next].Name},{"Receipt",receipt}});
        m_pending=0;++m_next;if(!complete)m_stopped=true;return true;
    }
    void Stop(){m_pending=0;m_stopped=true;}
    Json Aggregate()const {
        Json rows=Json::array();for(const auto& item:m_items){bool all=m_receipts.size()==m_players.size(),rejected=false;
            for(const auto& delivered:m_receipts){const auto state=Outcome(delivered.at("Receipt"),item);all=all&&state!="Unconfirmed";rejected=rejected||state=="Rejected";}
            auto row=item;row["State"]=all?(rejected?"Rejected":"Confirmed"):"Unconfirmed";
            row["Message"]=all?(rejected?"One or more recipients rejected this item; see per-player results.":"Every recipient inventory delta confirmed."):"Delivery incomplete or uncertain; inspect inventories before retrying.";
            rows.push_back(std::move(row));}
        return {{"GrantResults",std::move(rows)}};
    }
};
}
