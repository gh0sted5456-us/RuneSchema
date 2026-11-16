#pragma once
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace PS::ItemDelivery {
inline constexpr auto FileName="item-delivery.json";
inline constexpr auto Once="once";
inline constexpr auto Always="always";
inline constexpr auto Off="off";
inline constexpr std::size_t MaxItems=256;

inline std::string Fold(std::string_view value){
    std::string out(value);
    for(auto& c:out)if(c>='A'&&c<='Z')c+=32;
    return out;
}
inline bool Id(std::string_view value){
    return !value.empty()&&value.size()<=128&&value.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==value.npos;
}
inline bool AssetPath(std::string_view path){
    const auto slash=path.rfind('/'),dot=path.find('.');
    return path.size()<=512&&path.starts_with('/')&&slash>0&&dot!=path.npos
        &&dot>slash+1&&dot+1<path.size()&&path.find("//")==path.npos
        &&path.find('.',dot+1)==path.npos&&path.find('/',dot+1)==path.npos
        &&path.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-/.",
            1)==path.npos;
}
struct Item {
    std::string Owner,Delivery,Path;
    std::string Key()const{return Fold(Owner)+"/"+Fold(Delivery)+":"+Fold(Path);}
};
inline std::vector<Item> Read(std::string_view owner,const nlohmann::json& definitions){
    if(owner.empty()||owner.size()>128||std::any_of(owner.begin(),owner.end(),[](unsigned char c){
            return c<32||c==127||c=='/'||c==92||c==':';})
        ||!definitions.is_object()||definitions.size()>MaxItems)
        throw std::runtime_error("Expected an object mapping delivery IDs to ItemData path lists");
    std::vector<Item> items;
    std::set<std::string> ids;
    for(const auto& [id,paths]:definitions.items()){
        if(!Id(id)||!ids.insert(Fold(id)).second||!paths.is_array()||paths.size()>MaxItems)
            throw std::runtime_error("Delivery IDs must be unique and each delivery must contain a path list");
        std::set<std::string> unique;
        for(const auto& path:paths){
            if(!path.is_string()||!AssetPath(path.get_ref<const std::string&>()))
                throw std::runtime_error("Each entry must be a full ItemData object path");
            const auto& asset=path.get_ref<const std::string&>();
            if(unique.insert(Fold(asset)).second)items.push_back({std::string(owner),id,asset});
            if(items.size()>MaxItems)throw std::runtime_error("Delivery definitions exceed the item limit");
        }
    }
    return items;
}

class Receipts {
    nlohmann::json m_rows;
public:
    explicit Receipts(nlohmann::json document){
        if(!document.is_object()||document.value("Version",0)!=1||!document.at("Items").is_object())
            throw std::runtime_error("Invalid item delivery receipt file");
        m_rows=std::move(document["Items"]);
        for(const auto& [key,state]:m_rows.items())
            if(state!="held"&&state!="delivered")throw std::runtime_error("Invalid item delivery receipt state");
    }
    static nlohmann::json Empty(){return {{"Version",1},{"Items",nlohmann::json::object()}};}
    bool CanAttempt(const std::string& key,bool always)const{
        const auto at=m_rows.find(key);
        return at==m_rows.end()||(always&&*at=="delivered");
    }
    bool Hold(const std::string& key,bool always){
        if(!CanAttempt(key,always))return false;
        m_rows[key]="held";return true;
    }
    bool Commit(const std::string& key){
        const auto at=m_rows.find(key);
        if(at==m_rows.end()||*at!="held")return false;
        *at="delivered";return true;
    }
    auto Document()const{return nlohmann::json{{"Version",1},{"Items",m_rows}};}
};

struct SaveTicket {
    int64_t Handle{};
    std::string Slot;
    bool operator==(const SaveTicket&)const=default;
};
struct SaveReceipt {
    std::optional<SaveTicket> Ticket;
    std::string OpaqueSlot;
    bool Dispatched=false,Ambiguous=false;
    std::optional<bool> Result;
    void Dispatch(SaveTicket ticket){
        if(Dispatched)Ambiguous=true;
        else {Ticket=std::move(ticket);Dispatched=true;}
    }
    void DispatchOpaque(bool exclusive){
        if(!exclusive||Dispatched||OpaqueSlot.empty())Ambiguous=true;
        Dispatched=true;
    }
    void Complete(const SaveTicket& ticket,int64_t code){
        if(!Dispatched||Ambiguous||Result)return;
        const bool matches=Ticket?*Ticket==ticket:OpaqueSlot==ticket.Slot;
        if(matches)Result=code==1;
    }
    bool Confirmed()const{return !Ambiguous&&Result.value_or(false);}
};
}
