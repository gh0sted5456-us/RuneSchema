#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include <nlohmann/json.hpp>

namespace PS::ItemCart {
using json=nlohmann::json;
inline bool KitPath(const std::string& path);
inline json KitItems(const json& document);
struct Entry { std::string Path; json Row; int Quantity=1; };
struct Check {bool Valid=false;std::string Message;};
struct Outcome {Entry Item;std::string State,Message;};
class Model {
    std::vector<Entry> m_entries;
    std::uint64_t m_delivery=0;
    json m_submitted=json::array();
    std::map<std::string,Check> m_checks;
    std::vector<Outcome> m_outcomes;
    std::set<std::string> m_selected,m_selectedOutcomes;
public:
    static constexpr std::size_t Limit=64;
    const std::vector<Entry>& Entries() const {return m_entries;}
    std::uint64_t Delivery() const {return m_delivery;}
    const std::vector<Outcome>& Outcomes()const{return m_outcomes;}
    const std::set<std::string>& Selection()const{return m_selected;}
    const std::set<std::string>& OutcomeSelection()const{return m_selectedOutcomes;}
    bool Select(const std::string& path,bool additive=false){
        if(m_delivery||std::none_of(m_entries.begin(),m_entries.end(),[&](const auto& e){return e.Path==path;}))return false;
        if(!additive)m_selected.clear();
        if(additive&&m_selected.contains(path))m_selected.erase(path);else m_selected.insert(path);return true;
    }
    void SelectAll(bool selected=true){if(m_delivery)return;m_selected.clear();if(selected)for(const auto& e:m_entries)m_selected.insert(e.Path);}
    bool SelectOutcome(const std::string& path,bool additive=false){
        if(m_delivery||std::none_of(m_outcomes.begin(),m_outcomes.end(),[&](const auto& e){return e.Item.Path==path&&e.State=="Confirmed";}))return false;
        if(!additive)m_selectedOutcomes.clear();
        if(additive&&m_selectedOutcomes.contains(path))m_selectedOutcomes.erase(path);else m_selectedOutcomes.insert(path);return true;
    }
    void SelectAllOutcomes(bool selected=true){if(m_delivery)return;m_selectedOutcomes.clear();if(selected)for(const auto& e:m_outcomes)if(e.State=="Confirmed")m_selectedOutcomes.insert(e.Item.Path);}
    bool MoveSelected(int direction){
        if(m_delivery||(direction!=-1&&direction!=1))return false;bool moved=false;
        if(direction<0){for(size_t i=1;i<m_entries.size();++i)if(m_selected.contains(m_entries[i].Path)&&!m_selected.contains(m_entries[i-1].Path)){std::swap(m_entries[i],m_entries[i-1]);moved=true;}}
        else for(size_t i=m_entries.size();i>1;--i)if(m_selected.contains(m_entries[i-2].Path)&&!m_selected.contains(m_entries[i-1].Path)){std::swap(m_entries[i-2],m_entries[i-1]);moved=true;}
        return moved;
    }
    size_t RemoveSelected(){if(m_delivery)return 0;for(const auto& path:m_selected)m_checks.erase(path);const auto count=std::erase_if(m_entries,[&](const auto& e){return m_selected.contains(e.Path);});m_selected.clear();return count;}
    bool Reorder(const std::string& path={}) {
        return ReorderPaths(path.empty()?std::set<std::string>{}:std::set<std::string>{path});
    }
    bool ReorderSelected(){return !m_selectedOutcomes.empty()&&ReorderPaths(m_selectedOutcomes);}
private:
    bool ReorderPaths(const std::set<std::string>& paths) {
        if(m_delivery)return false;
        auto staged=*this;bool added=false;
        for(const auto& outcome:m_outcomes) {
            if(outcome.State!="Confirmed"||(!paths.empty()&&!paths.contains(outcome.Item.Path)))continue;
            const auto& item=outcome.Item;int quantity=item.Quantity;
            for(const auto& entry:staged.m_entries)if(entry.Path==item.Path)quantity+=entry.Quantity;
            if(quantity>10000||!staged.Add(item.Row))return false;
            staged.SetQuantity(item.Path,quantity);staged.m_checks.erase(item.Path);added=true;
        }
        if(added){m_entries=std::move(staged.m_entries);m_checks=std::move(staged.m_checks);}
        return added;
    }
public:
    const Check* Validation(const std::string& path)const{const auto i=m_checks.find(path);return i==m_checks.end()?nullptr:&i->second;}
    void ClearValidation(){m_checks.clear();}
    void ApplyValidation(const json& rows){
        if(!rows.is_array()||rows.size()>Limit)return;
        for(const auto& row:rows){if(!row.is_object()||!row.contains("Item")||!row["Item"].is_string()||!row.contains("Valid")||!row["Valid"].is_boolean())continue;
            const auto path=row["Item"].get<std::string>();
            if(std::none_of(m_entries.begin(),m_entries.end(),[&](const auto& entry){return entry.Path==path;}))continue;
            m_checks[path]={row["Valid"].get<bool>(),row.contains("Message")&&row["Message"].is_string()?row["Message"].get<std::string>():std::string{}};
        }
    }
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
        m_checks.erase(path);m_entries.push_back({path,row,1});return true;
    }
    bool Remove(const std::string& path) {
        if(m_delivery)return false;
        m_selected.erase(path);m_checks.erase(path);return std::erase_if(m_entries,[&](const auto& entry){return entry.Path==path;})!=0;
    }
    void Clear(){if(!m_delivery){m_entries.clear();m_checks.clear();m_outcomes.clear();m_selected.clear();m_selectedOutcomes.clear();}}
    void SetQuantity(const std::string& path,int quantity) {
        if(m_delivery)return;
        for(auto& entry:m_entries)if(entry.Path==path){entry.Quantity=std::clamp(quantity,1,10000);return;}
    }
    void RefreshMetadata(const json& rows) {
        if(!rows.is_array()||rows.size()>Limit)return;
        for(const auto& row:rows) {
            if(!row.is_object())continue;const auto path=row.find("Path");if(path==row.end()||!path->is_string())continue;
            for(auto& entry:m_entries)if(entry.Path==path->get_ref<const std::string&>()){entry.Row=row;break;}
        }
    }
    void ImportKit(const json& document,const std::function<json(const std::string&)>& resolve={}) {
        ImportKits({document},false,resolve);
    }
    void ImportKits(const std::vector<json>& documents,bool append=false,const std::function<json(const std::string&)>& resolve={}) {
        if(m_delivery)throw std::runtime_error("Wait for delivery before importing a kit");
        if(documents.empty()||documents.size()>256)throw std::invalid_argument("Select 1..256 presets");
        std::vector<Entry> staged=append?m_entries:std::vector<Entry>{};
        for(const auto& document:documents)for(const auto& item:KitItems(document)) {
            const auto path=item.at("Item").get<std::string>();const int quantity=item.at("Count").get<int>();
            const auto existing=std::find_if(staged.begin(),staged.end(),[&](const auto& e){return e.Path==path;});
            if(existing!=staged.end()){if(existing->Quantity>10000-quantity)throw std::invalid_argument("Combined preset quantity exceeds 10000");existing->Quantity+=quantity;continue;}
            if(staged.size()>=Limit)throw std::invalid_argument("Combined presets exceed 64 item types");
            auto row=resolve?resolve(path):json::object();
            if(!row.is_object())row=json::object();
            row["Path"]=path;if(!row.contains("Name")||!row.at("Name").is_string())row["Name"]=path.substr(path.find_last_of('.')+1);
            staged.push_back({path,std::move(row),quantity});
        }
        // Commit only after every row validates/resolves. Import never delivers.
        m_entries=std::move(staged);m_checks.clear();m_outcomes.clear();m_selected.clear();m_selectedOutcomes.clear();
    }
    json Payload(int mode=0,bool selectedOnly=false) const {
        json items=json::array();
        for(const auto& entry:m_entries) {
            if(selectedOnly&&!m_selected.contains(entry.Path))continue;
            int stack=1;
            const auto it=entry.Row.find("MaxStackSize");
            if(it!=entry.Row.end() && it->is_number_unsigned())stack=static_cast<int>(std::clamp<std::uint64_t>(it->get<std::uint64_t>(),1,10000));
            else if(it!=entry.Row.end() && it->is_number_integer())stack=static_cast<int>(std::clamp<std::int64_t>(it->get<std::int64_t>(),1,10000));
            const int quantity=mode==1?1:mode==2?std::max(1,stack/2):mode==3?stack:entry.Quantity;
            items.push_back({{"Item",entry.Path},{"Count",quantity}});
        }
        return items;
    }
    bool BeginDelivery(std::uint64_t request,int mode=0,bool selectedOnly=false) {
        if(!request||m_delivery||m_entries.empty()||(selectedOnly&&m_selected.empty())||mode<0||mode>3)return false;
        m_submitted=Payload(mode,selectedOnly);m_delivery=request;m_outcomes.clear();m_selectedOutcomes.clear();return true;
    }
    void CancelDelivery(){m_delivery=0;m_submitted=json::array();}
    // Only an exact, single native inventory-delta confirmation consumes a slot.
    // Definite pre-write rejections leave the active cart, but keep a red-X
    // outcome. Ambiguous results remain staged for explicit inventory review.
    std::size_t CompleteDelivery(std::uint64_t request,const json& receipt) {
        if(!m_delivery||request!=m_delivery)return 0;
        std::size_t confirmed=0;
        const auto results=receipt.find("GrantResults");
        if(results!=receipt.end()&&results->is_array()&&results->size()<=Limit) {
            for(const auto& sent:m_submitted) {
                const auto& path=sent.at("Item").get_ref<const std::string&>();
                std::size_t matches=0;bool exact=false,rejected=false;std::string message;
                for(const auto& row:*results) {
                    if(!row.is_object())continue;
                    const auto item=row.find("Item"),count=row.find("Count"),state=row.find("State");
                    if(item==row.end()||!item->is_string()||*item!=path)continue;
                    ++matches;
                    exact=count!=row.end()&&count->is_number_integer()&&*count==sent.at("Count")
                        &&state!=row.end()&&state->is_string()&&*state=="Confirmed";
                    rejected=count!=row.end()&&count->is_number_integer()&&*count==sent.at("Count")
                        &&state!=row.end()&&state->is_string()&&*state=="Rejected";
                    message=row.contains("Message")&&row["Message"].is_string()?row["Message"].get<std::string>():std::string{};
                }
                auto entry=std::find_if(m_entries.begin(),m_entries.end(),[&](const auto& item){return item.Path==path;});
                if(entry!=m_entries.end()){
                    auto delivered=*entry;delivered.Quantity=sent.at("Count").get<int>();
                    m_outcomes.push_back({std::move(delivered),matches==1&&exact?"Confirmed":matches==1&&rejected?"Rejected":"Unconfirmed",message});
                }
                if(matches==1&&(exact||rejected)){
                    const auto removed=std::erase_if(m_entries,[&](const auto& entry){return entry.Path==path;});
                    if(exact)confirmed+=removed;m_checks.erase(path);m_selected.erase(path);
                }
            }
        }
        CancelDelivery();return confirmed;
    }
};

// Portable kit contract. Parsing/staging does not grant items, load assets,
// run code, select a player, or bypass the existing explicit delivery permission.
inline bool KitPath(const std::string& path) {
    const auto slash=path.find_last_of('/'),dot=path.find_last_of('.');
    return path.size()>3&&path.size()<1024&&path.front()=='/'&&slash!=path.npos&&dot!=path.npos
        &&dot>slash+1&&dot+1<path.size()&&path.find("..") == path.npos&&path.find("//")==path.npos
        &&std::none_of(path.begin(),path.end(),[](unsigned char c){return c<=32||c==127||c=='\\'||c==':'||c=='"';});
}
inline json KitItems(const json& document) {
    if(!document.is_object()||!document.contains("SchemaVersion")||!document.at("SchemaVersion").is_number_integer()||document.at("SchemaVersion")!=1
        ||!document.contains("Type")||document.at("Type")!="RuneSchemaHelpyKit")
        throw std::invalid_argument("Unsupported RSMK kit schema");
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
inline json KitDocument(const Model& cart,const std::string& name={},bool selectedOnly=false) {
    json doc={{"SchemaVersion",1},{"Type","RuneSchemaHelpyKit"},{"Name",name},{"Items",cart.Payload(0,selectedOnly)}};
    (void)KitItems(doc);return doc;
}
}
