#include "Loader/DragonWildsRegistryLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include "Runtime/RegistryBridge.h"
#include "Utility/Config.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/FAssetData.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace {
using json=nlohmann::json;
constexpr std::size_t MaxEntriesPerDocument=256;
constexpr std::size_t MaxPresentationsPerEntry=64;
constexpr std::size_t MaxCookedRegistryBytes=256*1024;

bool Identifier(const std::string& value) {
    return !value.empty() && value.size()<=96
        && std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-'||c=='.';});
}

bool AssetPath(const std::string& value) {
    if(value.empty() || value.size()>1024 || value[0]!='/' || value.find("..")!=std::string::npos
        || value.find_first_of("\r\n\t")!=std::string::npos)return false;
    const auto mountEnd=value.find('/',1);if(mountEnd==std::string::npos||mountEnd==1)return false;
    const auto mount=value.substr(1,mountEnd-1);if(mount=="Script"||mount=="Engine")return false;
    return std::all_of(mount.begin(),mount.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-'||c=='.';});
}

void Fields(const json& value,std::initializer_list<const char*> allowed,const char* label) {
    if(!value.is_object())throw std::runtime_error(std::string(label)+" must be an object");
    for(const auto& [key,_]:value.items())if(std::none_of(allowed.begin(),allowed.end(),[&](const char* item){return key==item;}))
        throw std::runtime_error(std::string(label)+" contains unsupported field '"+key+"'");
}

json AuthorDocument(const json& source) {
    if(source.is_object()&&source.contains("RegistryJson")&&source["RegistryJson"].is_string())return json::parse(source["RegistryJson"].get<std::string>());
    if(source.is_object()&&source.contains("SchemaVersion"))return source;
    if(source.is_object()&&source.contains("kind")&&source.value("kind",std::string{})=="RuneSchemaRegistryBridgeManifest"&&source.contains("entries")) {
        json entries=json::array();for(const auto& row:source["entries"]) {
            json entry={{"Id",row.value("id",std::string{})},{"Kind",row.value("kind",std::string{})}};
            if(row.contains("spell"))entry["Spell"]=row["spell"];
            if(row.contains("presentation")){entry["Presentation"]=json::array();for(const auto& p:row["presentation"]){json out={{"Phase",p.value("module",std::string{})},{"Class",p.value("path",std::string{})},{"Classification",p.value("classification",std::string{})}};if(p.contains("socket"))out["Socket"]=p["socket"];if(p.contains("parameters"))out["Parameters"]=p["parameters"];entry["Presentation"].push_back(std::move(out));}}
            if(row.contains("authority")){const auto& a=row["authority"];entry["Authority"]={{"Action",a.value("action",std::string{})},{"GraphClass",a.value("graphClass",std::string{})},{"Function",a.value("function",std::string("Trigger"))},{"Bindings",a.value("bindings",json::object())}};if(a.contains("dataAsset"))entry["Authority"]["DataAsset"]=a["dataAsset"];}
            if(row.contains("metadata"))entry["Metadata"]=row["metadata"];entries.push_back(std::move(entry));
        }return {{"SchemaVersion",1},{"Entries",std::move(entries)}};
    }
    if(source.is_object()&&source.contains("Properties")&&source["Properties"].is_object())return AuthorDocument(source["Properties"]);
    if(source.is_array()) {
        bool fmodel=false;for(const auto& exportRow:source)if(exportRow.is_object()&&(exportRow.contains("Type")||exportRow.contains("Properties"))){fmodel=true;if(exportRow.contains("Properties"))try{return AuthorDocument(exportRow["Properties"]);}catch(...) {}}
        if(fmodel)throw std::runtime_error("FModel export detected, but no RuneSchema registry contract was found. Author a simple Entries document or cook a registry asset with RegistryJson; do not paste an arbitrary asset dump");
        return {{"SchemaVersion",1},{"Entries",source}};
    }
    throw std::runtime_error("unsupported registry root; use {SchemaVersion, Entries}, an Entries array, a merged RuneSchema manifest, or a cooked RegistryJson contract");
}
}

namespace DragonWilds {
DragonWildsRegistryLoader::DragonWildsRegistryLoader(PS::Network::RegistryBridge& bridge)
    :DragonWildsModLoaderBase("registry"),m_bridge(bridge) {
    SetDisplayName(TEXT("Registry Loader"));
}

bool DragonWildsRegistryLoader::CanInitialize(const EEngineLifecyclePhase& phase) {
    return phase==EEngineLifecyclePhase::PostEngineInit;
}

bool DragonWildsRegistryLoader::OnInitialize() {
    m_modEntries=json::array();m_audit=json::array();m_keys.clear();
    return true;
}

nlohmann::json DragonWildsRegistryLoader::NormalizeEntry(const json& entry,const std::string& owner) {
    Fields(entry,{"Id","Kind","Spell","Presentation","Authority","Metadata"},"registry entry");
    const auto id=entry.value("Id",std::string{});
    const auto kind=entry.value("Kind",std::string{});
    if(!Identifier(id))throw std::runtime_error("registry Id must use letters, numbers, dot, dash or underscore");
    if(kind!="SpellPresentation" && kind!="PersistentEffect" && kind!="WeatherPresentation"
        && kind!="AudioPresentation" && kind!="CosmeticWrapper")
        throw std::runtime_error("registry Kind is unsupported");
    const auto key=owner+":"+id;
    if(m_keys.contains(key))throw std::runtime_error("duplicate registry key '"+key+"'");
    json result={{"key",key},{"owner",owner},{"id",id},{"kind",kind},{"presentation",json::array()}};
    if(entry.contains("Spell")) {
        if(!entry["Spell"].is_string() || !AssetPath(entry["Spell"].get<std::string>()))
            throw std::runtime_error("registry Spell must be a supported cooked asset path");
        result["spell"]=entry["Spell"];
    }
    if(!entry.contains("Presentation") && !entry.contains("Authority"))
        throw std::runtime_error("registry entry requires Presentation or Authority");
    if(entry.contains("Presentation") && (!entry["Presentation"].is_array()
        || entry["Presentation"].size()>MaxPresentationsPerEntry))
        throw std::runtime_error("registry Presentation must contain at most 64 entries");
    for(const auto& row:entry.value("Presentation",json::array())) {
        Fields(row,{"Phase","Class","Classification","Socket","Parameters"},"registry presentation");
        const auto phase=row.value("Phase",std::string{});
        const auto path=row.value("Class",std::string{});
        const auto classification=row.value("Classification",std::string{});
        if(!Identifier(phase))throw std::runtime_error("registry presentation Phase is invalid");
        if(!AssetPath(path))throw std::runtime_error("registry presentation Class is not a supported cooked path");
        if(classification!="PureVFX" && classification!="CosmeticWrapper"
            && classification!="NativeReplicated" && classification!="Unsupported")
            throw std::runtime_error("registry presentation Classification is unsupported");
        json normalized={{"module",phase},{"path",path},{"classification",classification}};
        if(row.contains("Socket")) {
            if(!row["Socket"].is_string() || row["Socket"].get<std::string>().size()>128)
                throw std::runtime_error("registry presentation Socket is invalid");
            normalized["socket"]=row["Socket"];
        }
        if(row.contains("Parameters")) {
            if(!row["Parameters"].is_object() || row["Parameters"].dump().size()>4096)
                throw std::runtime_error("registry presentation Parameters must be a bounded object");
            normalized["parameters"]=row["Parameters"];
        }
        result["presentation"].push_back(std::move(normalized));
    }
    if(entry.contains("Authority")) {
        const auto& authority=entry["Authority"];
        Fields(authority,{"Action","GraphClass","DataAsset","Function","Bindings"},"registry authority");
        const auto action=authority.value("Action",std::string{});
        if(action!="SpawnFollower" && action!="ExecuteGraph")
            throw std::runtime_error("registry Authority Action is unsupported");
        const auto graph=authority.value("GraphClass",std::string{});
        if(!AssetPath(graph) || !graph.ends_with("_C"))
            throw std::runtime_error("registry Authority GraphClass must be a cooked Blueprint class path");
        const auto function=authority.value("Function",std::string{"Trigger"});
        if(!Identifier(function))throw std::runtime_error("registry Authority Function is invalid");
        json bindings=json::object();
        if(authority.contains("Bindings")) {
            if(!authority["Bindings"].is_object() || authority["Bindings"].size()>32)
                throw std::runtime_error("registry Authority Bindings must be an object with at most 32 fields");
            for(const auto& [property,value]:authority["Bindings"].items()) {
                if(property.empty() || property.size()>128 || !value.is_string() || !AssetPath(value.get<std::string>()))
                    throw std::runtime_error("registry Authority binding must map a bounded property name to a cooked asset path");
                bindings[property]=value;
            }
        }
        std::string asset;
        if(authority.contains("DataAsset")) {
            if(!authority["DataAsset"].is_string() || !AssetPath(authority["DataAsset"].get<std::string>()))
                throw std::runtime_error("registry Authority DataAsset is invalid");
            asset=authority["DataAsset"].get<std::string>();
        }
        if(action=="SpawnFollower") {
            if(asset.empty())throw std::runtime_error("SpawnFollower authority requires DataAsset");
            if(bindings.contains("Follower Data Asset") && bindings["Follower Data Asset"]!=asset)
                throw std::runtime_error("SpawnFollower DataAsset conflicts with its named binding");
            bindings["Follower Data Asset"]=asset;
        } else if(bindings.empty()) {
            throw std::runtime_error("ExecuteGraph authority requires at least one named asset binding");
        }
        result["authority"]={{"action",action},{"graphClass",graph},{"function",function},{"bindings",bindings}};
        if(!asset.empty())result["authority"]["dataAsset"]=asset;
    }
    if(entry.contains("Metadata")) {
        if(!entry["Metadata"].is_object() || entry["Metadata"].dump().size()>4096)
            throw std::runtime_error("registry Metadata must be a bounded object");
        result["metadata"]=entry["Metadata"];
    }
    m_keys.insert(key);return result;
}

void DragonWildsRegistryLoader::LoadDocument(const json& input,const std::string& owner,const std::string& source) {
    const auto document=AuthorDocument(input);
    Fields(document,{"SchemaVersion","Entries"},"registry document");
    if(document.value("SchemaVersion",0)!=1 || !document.contains("Entries") || !document["Entries"].is_array()
        || document["Entries"].size()>MaxEntriesPerDocument)
        throw std::runtime_error("registry document requires SchemaVersion 1 and at most 256 Entries");
    std::size_t ordinal=0;for(const auto& entry:document["Entries"]) {++ordinal;
        const auto id=entry.is_object()?entry.value("Id",std::string("<missing Id>")):std::string("<non-object>");
        try {auto normalized=NormalizeEntry(entry,owner);
            m_audit.push_back({{"Key",normalized["key"]},{"Owner",owner},{"Source",source},{"Status","Accepted"},{"PresentationCount",normalized["presentation"].size()}});
            m_modEntries.push_back(std::move(normalized));
        }catch(const std::exception& error){m_audit.push_back({{"Key",owner+":"+id},{"Owner",owner},{"Source",source},{"Status","Rejected"},{"Reason",error.what()}});PS::Log<RC::LogLevel::Error>(STR("Registry '{}:{}' in '{}' (entry #{}) rejected: {}.\n"),PS::ToWideSafe(owner.c_str()),PS::ToWideSafe(id.c_str()),PS::ToWideSafe(source.c_str()),ordinal,PS::ToWideSafe(error.what()));}
    }
}

void DragonWildsRegistryLoader::OnLoad(const std::filesystem::path& path,const RC::StringType& mod,
    const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    const auto owner=RC::to_string(mod);
    if(!Identifier(owner) || owner=="FModel" || owner=="RuneSchema")
        throw std::runtime_error("registry owner is invalid or reserved");
    PS::JsonHelpers::ParseJsonFilesInPathWithSource(path,
        [&](const json& document,const std::filesystem::path& relative) {
            const auto source=relative.generic_string();
            try {LoadDocument(document,owner,source);}
            catch(const std::exception& error) {
                m_audit.push_back({{"Owner",owner},{"Source",source},{"Status","Rejected"},{"Reason",error.what()}});
                PS::Log<RC::LogLevel::Error>(STR("Registry file '{}' in mod '{}' rejected: {}.\n"),
                    PS::ToWideSafe(source.c_str()),mod,PS::ToWideSafe(error.what()));
            }
        });
}

void DragonWildsRegistryLoader::LoadCookedRegistries() {
    TArray<FAssetData> assets;auto interface=UAssetRegistryHelpers::GetAssetRegistry();auto* registry=static_cast<UAssetRegistry*>(interface.ObjectPointer);
    if(!registry||!registry->GetAllAssets(assets,true)||assets.Num()<0||assets.Num()>262144)throw std::runtime_error("mounted Asset Registry is unavailable or outside the safe bound");
    for(auto& asset:assets)try {
        const auto name=RC::to_string(asset.AssetName().ToString());if(!name.starts_with("DA_RuneSchemaRegistry")&&!name.starts_with("RSREG_"))continue;
        const auto package=RC::to_string(asset.PackageName().ToString()),path=package+"."+name;auto* object=ActorHelper::ResolveObject(PS::ToWideSafe(path.c_str()));
        if(!object)throw std::runtime_error("cooked registry asset could not be loaded");
        auto read=[&](const TCHAR* field)->std::string{auto* property=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),field));if(!property)return {};const auto& value=property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(object));const auto& chars=value.GetCharArray();if(chars.Num()<1||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0||chars.Num()>static_cast<int32_t>(MaxCookedRegistryBytes+1))throw std::runtime_error("cooked registry string is invalid or exceeds 256 KiB");return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));};
        auto document=read(TEXT("RuneSchemaRegistryJson"));if(document.empty())document=read(TEXT("RegistryJson"));if(document.empty())throw std::runtime_error("asset requires a RuneSchemaRegistryJson or RegistryJson string property");
        auto owner=read(TEXT("RegistryOwner"));if(owner.empty()){if(package.size()>2&&package[0]=='/'){const auto slash=package.find('/',1);owner=package.substr(1,slash==std::string::npos?slash:slash-1);}if(owner=="Game")throw std::runtime_error("assets under /Game require an explicit RegistryOwner string");}
        if(!Identifier(owner)||owner=="RuneSchema"||owner=="FModel")throw std::runtime_error("cooked RegistryOwner is invalid or reserved");
        LoadDocument(json::parse(document),owner,"pak:"+path);
        PS::Log<RC::LogLevel::Normal>(STR("Registry: discovered cooked registry asset '{}' owned by '{}'.\n"),PS::ToWideSafe(path.c_str()),PS::ToWideSafe(owner.c_str()));
    }catch(const std::exception& error){const auto name=RC::to_string(asset.AssetName().ToString());const auto package=RC::to_string(asset.PackageName().ToString());m_audit.push_back({{"Owner","<cooked>"},{"Source","pak:"+package+"."+name},{"Status","Rejected"},{"Reason",error.what()}});PS::Log<RC::LogLevel::Error>(STR("Cooked registry asset '{}' rejected: {}.\n"),PS::ToWideSafe((package+"."+name).c_str()),PS::ToWideSafe(error.what()));}
}

void DragonWildsRegistryLoader::WriteMerged() {
    json entries=m_modEntries;
    std::size_t pure=0,native=0,wrappers=0,unsupported=0;
    for(const auto& entry:entries)for(const auto& row:entry.value("presentation",json::array())) {
        const auto value=row.value("classification",std::string{});
        if(value=="PureVFX")++pure;else if(value=="NativeReplicated")++native;
        else if(value=="CosmeticWrapper")++wrappers;else ++unsupported;
    }
    const json merged={{"kind","RuneSchemaRegistryBridgeManifest"},{"schemaVersion",1},{"protocolVersion",1},
        {"counts",{{"entries",entries.size()},{"modEntries",m_modEntries.size()},
            {"classifications",{{"PureVFX",pure},{"CosmeticWrapper",wrappers},{"NativeReplicated",native},{"Unsupported",unsupported}}}}},
        {"entries",std::move(entries)}};
    m_bridge.SetRegistrySnapshot(merged.dump());
    if(PS::PSConfig::Get()->GetSettings().advancedRuntime)
        PS::ConfigFiles::Write(PS::HostServices::ExportsDirectory()/"RegistryManifestAudit.json",
            json{{"Build","0.7.5.24"},{"Accepted",m_modEntries.size()},{"Entries",m_audit}}.dump(2)+"\n");
    PS::Log<RC::LogLevel::Normal>(TEXT("Registry: merged {} mod-owned entries.\n"),m_modEntries.size());
}

void DragonWildsRegistryLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase) {
    if(phase==EEngineLifecyclePhase::PostEngineInit){try{LoadCookedRegistries();}catch(const std::exception& error){PS::Log<RC::LogLevel::Error>(STR("Cooked registry discovery failed; JSON registries remain active: {}.\n"),PS::ToWideSafe(error.what()));}WriteMerged();}
}

void DragonWildsRegistryLoader::OnAutoReload(const RC::StringType& mod,const std::filesystem::path&) {
    PS::Log<RC::LogLevel::Warning>(TEXT("Registry changes in {} require a restart.\n"),mod);
}
}
