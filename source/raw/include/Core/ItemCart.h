#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

namespace PS::ItemCart {
using json=nlohmann::json;
struct Entry { std::string Path; json Row; int Quantity=1; };
class Model {
    std::vector<Entry> m_entries;
    std::uint64_t m_delivery=0;
    json m_submitted=json::array();
public:
    static constexpr std::size_t Limit=64;
    const std::vector<Entry>& Entries() const {return m_entries;}
    std::uint64_t Delivery() const {return m_delivery;}
    bool Add(const json& row) {
        if(m_delivery)return false;
        if(!row.is_object())return false;
        const auto it=row.find("Path");
        if(it==row.end() || !it->is_string())return false;
        const auto path=it->get<std::string>();
        if(path.empty() || path.front()!='/' || path.size()>=1024)return false;
        for(auto& entry:m_entries)if(entry.Path==path) {
            entry.Quantity=std::min(10000,entry.Quantity+1);return true;
        }
        if(m_entries.size()>=Limit)return false;
        m_entries.push_back({path,row,1});return true;
    }
    bool Remove(const std::string& path) {
        if(m_delivery)return false;
        return std::erase_if(m_entries,[&](const auto& entry){return entry.Path==path;})!=0;
    }
    void Clear(){if(!m_delivery)m_entries.clear();}
    void SetQuantity(const std::string& path,int quantity) {
        if(m_delivery)return;
        for(auto& entry:m_entries)if(entry.Path==path){entry.Quantity=std::clamp(quantity,1,10000);return;}
    }
    json Payload(int mode=0) const {
        json items=json::array();
        for(const auto& entry:m_entries) {
            int stack=1;
            const auto it=entry.Row.find("MaxStackSize");
            if(it!=entry.Row.end() && it->is_number_unsigned())stack=static_cast<int>(std::clamp<std::uint64_t>(it->get<std::uint64_t>(),1,10000));
            else if(it!=entry.Row.end() && it->is_number_integer())stack=static_cast<int>(std::clamp<std::int64_t>(it->get<std::int64_t>(),1,10000));
            const int quantity=mode==1?1:mode==2?std::max(1,stack/2):mode==3?stack:entry.Quantity;
            items.push_back({{"Item",entry.Path},{"Count",quantity}});
        }
        return items;
    }
    bool BeginDelivery(std::uint64_t request,int mode=0) {
        if(!request||m_delivery||m_entries.empty()||mode<0||mode>3)return false;
        m_submitted=Payload(mode);m_delivery=request;return true;
    }
    void CancelDelivery(){m_delivery=0;m_submitted=json::array();}
    // Only an exact, single native inventory-delta confirmation consumes a slot.
    // Editing is locked while this one-shot request is pending; failed rows stay staged.
    std::size_t CompleteDelivery(std::uint64_t request,const json& receipt) {
        if(!m_delivery||request!=m_delivery)return 0;
        std::size_t confirmed=0;
        const auto results=receipt.find("GrantResults");
        if(results!=receipt.end()&&results->is_array()&&results->size()<=Limit) {
            for(const auto& sent:m_submitted) {
                const auto& path=sent.at("Item").get_ref<const std::string&>();
                std::size_t matches=0;bool exact=false;
                for(const auto& row:*results) {
                    if(!row.is_object())continue;
                    const auto item=row.find("Item"),count=row.find("Count"),state=row.find("State");
                    if(item==row.end()||!item->is_string()||*item!=path)continue;
                    ++matches;
                    exact=count!=row.end()&&count->is_number_integer()&&*count==sent.at("Count")
                        &&state!=row.end()&&state->is_string()&&*state=="Confirmed";
                }
                if(matches==1&&exact)confirmed+=std::erase_if(m_entries,[&](const auto& entry){return entry.Path==path;});
            }
        }
        CancelDelivery();return confirmed;
    }
};

// Reserved portable kit contract. Parsing/staging does not grant items, load assets,
// run code, select a player, or bypass the existing explicit delivery permission.
inline bool KitPath(const std::string& path) {
    const auto slash=path.find_last_of('/'),dot=path.find_last_of('.');
    return path.size()>3&&path.size()<1024&&path.front()=='/'&&slash!=path.npos&&dot!=path.npos
        &&dot>slash+1&&dot+1<path.size()
        &&std::none_of(path.begin(),path.end(),[](unsigned char c){return c<=32||c==127||c=='\\'||c==':'||c=='"';});
}
inline json KitItems(const json& document) {
    if(!document.is_object()||!document.contains("SchemaVersion")||!document.at("SchemaVersion").is_number_integer()||document.at("SchemaVersion")!=1
        ||!document.contains("Type")||document.at("Type")!="RuneSchemaHelpyKit")
        throw std::invalid_argument("Unsupported Helpy kit schema");
    for(const auto& [key,value]:document.items())if(key!="SchemaVersion"&&key!="Type"&&key!="Name"&&key!="Items")
        throw std::invalid_argument("Kit metadata cannot contain actions or delivery instructions");
    if(document.contains("Name")&&(!document.at("Name").is_string()||document.at("Name").get_ref<const std::string&>().size()>128))
        throw std::invalid_argument("Kit name must be a string of at most 128 bytes");
    const auto& items=document.at("Items");
    if(!items.is_array()||items.empty()||items.size()>Model::Limit)throw std::invalid_argument("Kit requires 1..64 items");
    std::vector<std::string> paths;
    for(const auto& row:items) {
        if(!row.is_object()||row.size()!=2||!row.contains("Item")||!row.at("Item").is_string()
            ||!row.contains("Count")||!row.at("Count").is_number_integer())throw std::invalid_argument("Invalid kit item");
        const auto path=row.at("Item").get<std::string>();const auto& count=row.at("Count");
        if(!KitPath(path)||count<1||count>10000||std::find(paths.begin(),paths.end(),path)!=paths.end())
            throw std::invalid_argument("Kit items require unique canonical paths and counts 1..10000");
        paths.push_back(path);
    }
    return items;
}
inline json KitDocument(const Model& cart,const std::string& name={}) {
    json doc={{"SchemaVersion",1},{"Type","RuneSchemaHelpyKit"},{"Name",name},{"Items",cart.Payload()}};
    (void)KitItems(doc);return doc;
}
}
