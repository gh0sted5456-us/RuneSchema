#pragma once
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Loader/VendorOffers.h"
#include "Loader/HumanNpc.h"
#include "Loader/NpcMarkers.h"
#include "Loader/NpcVisualEffect.h"
#include "Loader/TimeOfDay.h"
#include "Loader/VendorCategoryGate.h"

namespace DragonWilds {
class NpcCatalog {
public:
    using Json = nlohmann::json;
    struct Entry { std::string Mod; Json Data; };
    struct Resolved { std::string Mod; Json Data; std::string StoreOwner; };
    inline static constexpr const char* DefaultHeaderImage =
        "/Game/Art/UI/Craft/T_DeathShop_Banner.T_DeathShop_Banner";
    static std::string HeaderImage(const Json& data) {
        if(!data.contains("VendorHeaderImage"))return DefaultHeaderImage;
        const auto path=data.at("VendorHeaderImage").get<std::string>();
        if(path.empty())return DefaultHeaderImage;
        if(path.size()<2 || path.size()>1024 || path.front()!='/' || path.find_first_of("\r\n\t")!=path.npos)
            throw std::runtime_error("VendorHeaderImage requires a nonempty Unreal asset path");
        return path;
    }
    static std::string Key(const std::string& mod, const std::string& id) {
        if (mod.empty() || id.empty() || id.size()>128 || id.find(':')!=id.npos)
            throw std::runtime_error("NPC/store Id must be nonempty, at most 128 characters, without ':'");
        return mod+":"+id;
    }
    static std::string Reference(const std::string& mod,const std::string& ref) {
        if(ref.empty() || ref.find_first_of("\r\n\t")!=ref.npos)
            throw std::runtime_error("NPC reference must be nonempty and contain no control characters");
        const auto colon=ref.find(':');
        return colon==ref.npos?Key(mod,ref):Key(ref.substr(0,colon),ref.substr(colon+1));
    }
    std::string ResolveNpcReference(const std::string& mod,const std::string& ref) const {
        const auto key=Reference(mod,ref);
        if(!npcs.contains(key))throw std::runtime_error("Missing NPC reference: "+key);
        return key;
    }
    bool ContainsNpcReference(const std::string& mod,const std::string& ref) const {
        try {return npcs.contains(Reference(mod,ref));}catch(...) {return false;}
    }
    void AddNpc(const std::string& mod, const Json& data) {
        auto normalized=data;
        // Pre-0.7.9 compatibility: the old Multiplayer flag no longer changes
        // behavior. One NPC definition now serves standalone and multiplayer.
        normalized.erase("Multiplayer");
        Require(normalized, {"Id","DisplayName","VisualSource","Mesh","Materials","IdleAnimation","HideMesh","NoInteract","HideName",
            "Location","Rotation","Scale","Enabled","EnableCollision","MeshCollision","Type","Appearance","Equipment","HideWeapon","Pose","DialoguePose","Ghost","VisualEffect","Map","OverheadIcon","VendorID","DialogueID","LoreID","LoreEntry","QuestID","TimeOfDay"});
        const auto& source=normalized;
        const auto key=Key(mod,source.at("Id").get<std::string>());
        HumanNpc::Validate(source);
        for(const auto* field:{"NoInteract","HideName"})
            if(source.contains(field) && !data[field].is_boolean())throw std::runtime_error(std::string(field)+" must be boolean");
        if(source.contains("HideMesh") && (!source["HideMesh"].is_boolean() || source.value("Type",std::string{})!="Prop"))
            throw std::runtime_error("HideMesh requires Type Prop and a boolean");
        if(source.contains("VisualEffect"))NpcVisualEffect::Validate(source.at("VisualEffect"));
        if(source.contains("LoreID") && source.contains("LoreEntry"))
            throw std::runtime_error("Use LoreID for a /lore definition or LoreEntry for a legacy/global entry, not both");
        if(source.contains("LoreID"))Reference(mod,source.at("LoreID").get<std::string>());
        if(source.contains("QuestID"))Reference(mod,source.at("QuestID").get<std::string>());
        if(source.contains("LoreEntry") || source.contains("LoreID")) {
            const auto& entry=source.contains("LoreEntry")?source.at("LoreEntry"):source.at("LoreID");
            if(!entry.is_string() || entry.get_ref<const std::string&>().empty() || entry.get_ref<const std::string&>().size()>1024
                || entry.get_ref<const std::string&>().find_first_of("\r\n\t")!=std::string::npos)
                throw std::runtime_error("LoreEntry requires a journal/lore entry ID or cooked asset path");
            if(source.contains("DialogueID") || source.contains("VendorID"))throw std::runtime_error("Direct LoreEntry cannot share the primary interaction with DialogueID or VendorID");
        }
        NpcMarkers::Validate(data);
        if(source.contains("TimeOfDay")) {
            if(!source["TimeOfDay"].is_string())throw std::runtime_error("TimeOfDay must be Any, Day, or Night");
            (void)TimeOfDay::Parse(source["TimeOfDay"].get<std::string>());
        }
        if(source.contains("EnableCollision") && !source["EnableCollision"].is_boolean())
            throw std::runtime_error("EnableCollision must be boolean");
        const auto collision=source.value("MeshCollision",std::string("Native"));
        if(collision!="Native" && collision!="Pawn" && collision!="None")
            throw std::runtime_error("MeshCollision must be Native, Pawn or None");
        if(HumanNpc::IsHuman(data) && collision=="Pawn")
            throw std::runtime_error("Human previews use actor capsule collision; MeshCollision Pawn is for AI visuals");
        if(source.contains("DialogueID")) {
            Reference(mod,source.at("DialogueID").get<std::string>());
        }
        if(source.contains("VendorID"))Reference(mod,source.at("VendorID").get<std::string>());
        if (!source.contains("Location") || (!HumanNpc::IsHuman(data) && !source.contains("VisualSource") && !source.contains("Mesh")))
            throw std::runtime_error("NPC requires Location and Mesh or VisualSource");
        if (!npcs.emplace(key,Entry{mod,source}).second)throw std::runtime_error("Duplicate NPC: "+key);
    }
    void AddStore(const std::string& mod, const Json& data) {
        if(data.contains("Npcs"))throw std::runtime_error("Move /vendors.Npcs bindings to VendorID on each /npc definition");
        Require(data,{"Id","Items","MerchantName","VendorHeaderImage","Repairable","Masterworkable","Enabled","DataTable","RowName","VendorProperties","CategoryRules"});
        const auto key=Key(mod,data.at("Id").get<std::string>());
        (void)HeaderImage(data);
        if(data.contains("Repairable") && !data.at("Repairable").is_boolean())
            throw std::runtime_error("Repairable must be boolean");
        if(data.contains("Masterworkable") && !data.at("Masterworkable").is_boolean())
            throw std::runtime_error("Masterworkable must be boolean");
        if(data.contains("CategoryRules"))(void)VendorCategoryGate::Parse(data.at("CategoryRules"));
        if (!data.contains("Items") || !data["Items"].is_array() || data["Items"].size()>128)
            throw std::runtime_error("Store Items requires an array of at most 128 offers");
        for(const auto& item:data["Items"])
            if(item.contains("_RecipeSlot"))throw std::runtime_error("_RecipeSlot is reserved for generated store recipes");
        if (!stores.emplace(key,Entry{mod,data}).second)throw std::runtime_error("Duplicate store: "+key);
    }
    struct StoreOffer { std::string Mod; std::string Id; Json Data; };
    static std::set<std::string> StoreTargets(const std::string& mod,const Json& data) {
        std::set<std::string> targets;
        if(data.contains("VendorID"))targets.insert(Reference(mod,data.at("VendorID").get<std::string>()));
        if(data.contains("RuneSchemaVendors")) {
            const auto& list=data.at("RuneSchemaVendors");
            if(!list.is_array() || list.size()>128)throw std::runtime_error("RuneSchemaVendors requires up to 128 store references");
            for(const auto& ref:list)targets.insert(Reference(mod,ref.get<std::string>()));
        }
        return targets;
    }
    static Json VanillaTargets(const Json& data) {
        Json result=Json::array();
        if(!data.contains("VanillaVendors"))return result;
        const auto& list=data.at("VanillaVendors");
        if(!list.is_array() || list.size()>128)throw std::runtime_error("VanillaVendors requires up to 128 Table/Row targets");
        std::set<std::pair<std::string,std::string>> seen;
        for(const auto& target:list) {
            Require(target,{"Table","Row"});
            const auto table=target.at("Table").get<std::string>();
            const auto row=target.at("Row").get<std::string>();
            if(table.empty() || table.size()>256 || table.find_first_of("/\\.\r\n\t")!=table.npos
                || row.empty() || row.size()>128 || row.find_first_of("\r\n\t")!=row.npos)
                throw std::runtime_error("VanillaVendors requires a data-table object name and nonempty row name");
            auto canonical=std::make_pair(table,row);
            for(auto* part:{&canonical.first,&canonical.second})
                for(char& c:*part)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
            if(seen.insert(canonical).second)result.push_back(target);
        }
        return result;
    }
    static StoreOffer ParseStoreOffer(const std::string& mod, const std::string& id, const Json& data) {
        Key(mod,id);
        Require(data,{"VendorID","RuneSchemaVendors","VanillaVendors","Item","Currency","Price","Count","Category","Order"});
        const auto stores=StoreTargets(mod,data);
        const auto vanilla=VanillaTargets(data);
        if(stores.empty() && vanilla.empty())throw std::runtime_error("Store recipe requires at least one merchant target");
        (void)VendorOffers::Properties(data);
        (void)VendorOffers::Category(data);
        (void)VendorOffers::Order(data,0);
        return {mod,id,data};
    }
    std::vector<Resolved> Resolve(const std::vector<StoreOffer>& offers = {}) const {
        auto expanded=stores;
        std::set<std::string> offerIds;
        for(const auto& offer:offers)ApplyOffer(expanded,offerIds,offer);
        std::vector<Resolved> result;
        for (const auto& [key,npc]:npcs)result.push_back(ResolveNpc(key,npc,expanded));
        return result;
    }
    template<class Error>
    std::vector<Resolved> ResolveIsolated(const std::vector<StoreOffer>& offers,Error onError) const {
        auto expanded=stores;std::set<std::string> offerIds;
        for(const auto& offer:offers)try {
            auto candidate=expanded;auto candidateIds=offerIds;
            ApplyOffer(candidate,candidateIds,offer);
            expanded=std::move(candidate);offerIds=std::move(candidateIds);
        }catch(const std::exception& error){onError("recipe:"+offer.Mod+":"+offer.Id,error.what());}
        catch(...){onError("recipe:"+offer.Mod+":"+offer.Id,"unknown store-offer error");}
        std::vector<Resolved> result;
        for(const auto& [key,npc]:npcs)try {result.push_back(ResolveNpc(key,npc,expanded));}
        catch(const std::exception& error){onError(key,error.what());}
        catch(...){onError(key,"unknown NPC reference error");}
        return result;
    }
private:
    static void ApplyOffer(std::map<std::string,Entry>& expanded,std::set<std::string>& offerIds,const StoreOffer& offer) {
        ParseStoreOffer(offer.Mod,offer.Id,offer.Data);
        const auto identity=Key(offer.Mod,offer.Id);
        if(!offerIds.insert(identity).second)throw std::runtime_error("Duplicate store recipe: "+identity);
        const auto targets=StoreTargets(offer.Mod,offer.Data);
        for(const auto& target:targets) {
            const auto store=expanded.find(target);
            if(store==expanded.end())throw std::runtime_error("Recipe "+identity+" references missing store "+target);
            if(store->second.Data.at("Items").size()>=128)throw std::runtime_error("Store exceeds 128 offers: "+target);
        }
        for(const auto& target:targets) {
            auto item=offer.Data;item.erase("VendorID");item.erase("RuneSchemaVendors");item.erase("VanillaVendors");
            item["_RecipeSlot"]="recipe:"+identity;expanded.at(target).Data["Items"].push_back(std::move(item));
        }
    }
    static Resolved ResolveNpc(const std::string& key,const Entry& npc,const std::map<std::string,Entry>& expanded) {
        auto data=npc.Data;
        if(data.contains("LoreID"))data["LoreEntry"]=Reference(npc.Mod,data.at("LoreID").get<std::string>());
        if(data.contains("QuestID"))data["QuestID"]=Reference(npc.Mod,data.at("QuestID").get<std::string>());
        if(data.value("NoInteract",false))
            for(const auto* field:{"VendorID","DialogueID","LoreID","LoreEntry"})data.erase(field);
        data["LoaderID"]=key;data["Stage"]="Visual";
        Json links=Json::object();
        if(data.contains("LoreEntry")) {
            data["Stage"]="Interaction";data["InteractionProperties"]={{"InteractionPrompt","Examine"}};
            links["Lore"]=data.at("LoreEntry");
        }
        if(data.contains("DialogueID")) {
            data["DialogueID"]=Reference(npc.Mod,data.at("DialogueID").get<std::string>());
            links["Dialogue"]=data.at("DialogueID");
            data["Stage"]="Interaction";data["InteractionProperties"]={{"InteractionPrompt","Talk"}};
        }
        if(data.contains("QuestID"))links["Quest"]=data.at("QuestID");
        const Entry* selected=nullptr;
        if(data.contains("VendorID")) {
            const auto target=Reference(npc.Mod,data.at("VendorID").get<std::string>());
            data["VendorID"]=target;links["Vendor"]=target;
            const auto found=expanded.find(target);
            if(found==expanded.end())throw std::runtime_error("NPC "+key+" references missing store "+target);
            if(found->second.Data.value("Enabled",true))selected=&found->second;
        }
        if(selected) {
            const auto& store=*selected;std::string owner="store:"+Key(store.Mod,store.Data.at("Id").get<std::string>());
            for(const auto* field:{"Items","MerchantName","VendorHeaderImage","Repairable","Masterworkable","DataTable","RowName","VendorProperties","CategoryRules"})
                if(store.Data.contains(field))data[field]=store.Data[field];
            if(!data.contains("MerchantName"))data["MerchantName"]=store.Data.at("Id");
            if(!data.contains("InteractionProperties"))data["InteractionProperties"]={{"InteractionPrompt","Trade"}};
            data["Stage"]="Merchant";data["References"]=std::move(links);return {npc.Mod,std::move(data),std::move(owner)};
        }
        data["References"]=std::move(links);return {npc.Mod,std::move(data),{}};
    }
    static void Require(const Json& data, std::initializer_list<const char*> fields) {
        if(!data.is_object())throw std::runtime_error("NPC/store definition must be an object");
        std::set<std::string> allowed(fields.begin(),fields.end());
        for(const auto& [key,value]:data.items())
            if(!allowed.contains(key))throw std::runtime_error("Unsupported NPC/store field: "+key);
        if(data.contains("Enabled") && !data["Enabled"].is_boolean())throw std::runtime_error("Enabled must be boolean");
    }
    std::map<std::string,Entry> npcs,stores;
};
}
