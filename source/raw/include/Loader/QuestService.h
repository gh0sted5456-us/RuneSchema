#pragma once
#include "Loader/QuestNativeAsset.h"
#include "Loader/QuestNativeRegistry.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Loader/QuestNativeAdapter.h"
#include "Loader/QuestLocationActor.h"
#include "Loader/DialogueSaveIdentity.h"
#include "Loader/QuestStageRuntime.h"
#include "Loader/EventDefinition.h"
#include "Loader/OwnedContentLedger.h"
#include "Runtime/HostServices.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include <map>
#include <memory>
#include "Utility/Logging.h"

namespace DragonWilds::Quests {
class Service {
    Catalog catalog;
    std::map<std::string,Json> definitions;
    std::map<std::string,std::unique_ptr<NativeAsset>> assets;
    std::map<std::string,std::unique_ptr<LocationActor>> locations;
    std::map<std::string,std::unique_ptr<NativeAsset>> eventAreaAssets;
    std::map<std::string,std::unique_ptr<LocationActor>> eventAreas;
    std::set<std::string> failedLocations;
    bool hasLocations=false;
    bool reconcilingLocations=false;
    std::map<std::string,std::string> reportedMarkerStates;
    Json networkManifest=Json::array();
    std::set<std::string> hidden;
    std::map<std::string,OwnedContent::Record> declarations;
    std::map<std::string,RC::Unreal::UObject*> declaredAssets;
public:
    bool QuestOwnsEventArea(RC::Unreal::UObject* controller,const std::string& eventKey) const {
        if(!controller || eventKey.empty())return false;
        for(const auto& [key,document]:definitions)try {
            const auto& definition=catalog.Find("_",key);
            const QuestNative::Adapter native(controller,Asset(key));
            auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");if(colon!=state.npos)state=state.substr(colon+2);
            if(state!="Given")continue;
            if(definition.Marker && definition.Kill && definition.Kill->EventKey==eventKey)return true;
            if(definition.Stages.empty())continue;
            const int run=native.GetInt(RC::Unreal::FName(TEXT("RuneSchema.Run"),RC::Unreal::FNAME_Add));
            if(run<=0 || !StageReceiptActive(native,document))continue;
            const auto progress=StageProgress(native,definition,run);const auto stage=progress.ActiveStage();
            if(stage>=definition.Stages.size())continue;
            for(size_t i=0;i<definition.Stages[stage].second.size();++i) {
                const auto& objective=definition.Stages[stage].second[i];
                if(!objective.Hidden && objective.Marker && objective.Kill && objective.Kill->EventKey==eventKey
                    && progress.Count(stage,i)<objective.Required.Count)return true;
            }
        }catch(...) {}
        return false;
    }
    bool SetEventArea(RC::Unreal::UObject* controller,const Events::Definition& event,bool show) {
        const auto found=eventAreas.find(event.Key);
        if(!show) {
            if(found!=eventAreas.end()){found->second->Remove();eventAreas.erase(found);}return false;
        }
        if(!event.EventArea || event.EventArea->Visibility==Events::AreaVisibility::Never)return false;
        ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
        if(!local.Result<bool>())return false;
        if(event.EventArea->Visibility==Events::AreaVisibility::Auto && QuestOwnsEventArea(controller,event.Key))return false;
        if(found!=eventAreas.end() && found->second->Get())return true;
        Quests::Definition marker{};
        marker.Key="RuneSchema.EventArea:"+event.Key;
        marker.PersistenceId=DialogueSave::PersistenceIdForSeed("EventArea/"+event.Key);
        marker.Title=event.Key;marker.Description="RuneSchema event area";marker.ObjectiveId="area";marker.ObjectiveText="Event area";
        marker.Required={"",1};marker.Reward={"",1};
        auto asset=std::make_unique<NativeAsset>(marker,true);
        auto area=std::make_unique<LocationActor>();
        const Quests::Location location{"RuneSchema_EventArea_"+marker.PersistenceId,event.EventArea->Position,event.EventArea->RadiusMeters};
        area->Create(controller->GetWorld(),asset->Get(),location);
        eventAreaAssets[event.Key]=std::move(asset);eventAreas[event.Key]=std::move(area);return true;
    }
    void EnsureDialogueState(const std::string& mod) {
        const auto key=DialogueSave::Key(mod);
        if(hidden.contains(key))return;
        const Json document={{"Id","__runeschema_dialogue_state"},{"PersistenceID",DialogueSave::PersistenceId(mod)},
            {"Title","RuneSchema dialogue progress"},{"Description","Internal player save record"},
            {"Objective",{{"Id","state"},{"Text","Internal"},{"Item","/Game/Gameplay/Items/Resources/Currency/ITEM_Currency_SoulFragment.ITEM_Currency_SoulFragment"},{"Count",1}}},
            {"Reward",{{"Item","/Game/Gameplay/Items/Resources/Currency/ITEM_Currency_SoulFragment.ITEM_Currency_SoulFragment"},{"Count",1}}}};
        Load(mod,document);hidden.insert(key);
    }
    bool HasAsset(const std::string& key) const{return assets.contains(key);}
    template<class Visit>void ForEachVisible(Visit visit)const {
        for(const auto& [key,document]:definitions)if(!hidden.contains(key))visit(key,catalog.Find("_",key),document);
    }
    template<class Visit>void ForEachDialogueMod(Visit visit)const {
        for(const auto& key:hidden)visit(key.substr(0,key.find(':')),key);
    }
    bool HasLocations() const {return hasLocations;}
    template<class Report> void ClearLocations(Report report) {
        for(auto& [key,location]:locations)try {location->Remove();}catch(const std::exception& e){report(key,e.what());}
        for(auto& [key,location]:eventAreas)try {location->Remove();}catch(const std::exception& e){report(key,e.what());}
        eventAreas.clear();eventAreaAssets.clear();
        locations.clear();failedLocations.clear();reportedMarkerStates.clear();
    }
    template<class Report> void ReconcileLocations(RC::Unreal::UObject* controller,Report report) {
        if(!controller || !controller->GetWorld() || reconcilingLocations)return;
        ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
        if(!local.Result<bool>())return;
        ActorHelper::FunctionCall authority(controller,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        const bool canMutate=authority.Result<bool>();
        reconcilingLocations=true;
        struct Guard {bool& Active;~Guard(){Active=false;}} guard{reconcilingLocations};
        for(const auto& [key,document]:definitions) {
            const auto& definition=catalog.Find("_",key);
            if(!definition.Marker && definition.Stages.empty())continue;
            try {
                const QuestNative::Adapter native(controller,Asset(key));
                auto state=RC::to_string(native.StateName());const auto colon=state.rfind("::");
                if(colon!=state.npos)state=state.substr(colon+2);
                if((definition.Marker || !definition.Stages.empty()) && reportedMarkerStates[key]!=state) {
                    reportedMarkerStates[key]=state;
                    PS::Log<RC::LogLevel::Verbose>(STR("Quest marker state changed: quest='{}', state='{}', authority={}, world='{}'.\n"),RC::to_generic_string(key),RC::to_generic_string(state),canMutate,controller->GetWorld()->GetPathName());
                }
                const auto reconcile=[&](const std::string& markerKey,const Location& marker,bool active) {
                auto found=locations.find(markerKey);
                if(found!=locations.end() && !found->second->Get()){locations.erase(found);found=locations.end();}
                if(!active) {
                    if(found!=locations.end()){found->second->Remove();locations.erase(found);}
                } else if(found==locations.end() && !failedLocations.contains(markerKey)) {
                    auto owned=std::make_unique<LocationActor>();
                    try {owned->Create(controller->GetWorld(),Asset(key),marker);}
                    catch(...) {
                        // Suppress only an attempted actor creation. Readiness
                        // failures before this point must not poison the session.
                        failedLocations.insert(markerKey);
                        throw;
                    }
                    PS::Log<RC::LogLevel::Verbose>(STR("Quest marker registered: '{}' at {}, {}, {}; radius={}m.\n"),RC::to_generic_string(markerKey),marker.Position[0],marker.Position[1],marker.Position[2],marker.RadiusMeters.value_or(0));
                    locations.emplace(markerKey,std::move(owned));
                }
                };
                if(definition.Marker) {
                    bool active=state=="Given";
                    if(active && (definition.Kill || definition.Acquire)) {
                        const auto count=native.GetInt(RC::Unreal::FName(RC::to_generic_string(definition.ObjectiveId).c_str(),RC::Unreal::FNAME_Add));
                        if(count<0 || count>definition.Required.Count)throw std::runtime_error("Invalid objective count for quest marker");
                        active=count<definition.Required.Count;
                    }
                    reconcile(key,*definition.Marker,active);
                }
                if(!definition.Stages.empty()) {
                    const int run=native.GetInt(RC::Unreal::FName(TEXT("RuneSchema.Run"),RC::Unreal::FNAME_Add));
                    size_t active=definition.Stages.size();
                    std::optional<Stages::Progress> progress;
                    if(state=="Given" && run>0 && StageReceiptActive(native,document)) {
                        // A connected client can receive and safely read the
                        // replicated stage receipt before IsQuestInitialized
                        // recognizes its locally constructed quest asset. AOR
                        // presentation is read-only there; authority paths keep
                        // the strict initialization requirement.
                        progress.emplace(StageProgress(native,definition,run,canMutate));active=progress->ActiveStage();
                        if(canMutate)SetStageText(native,definition,active);
                    }
                    for(size_t s=0;s<definition.Stages.size();++s)for(size_t o=0;o<definition.Stages[s].second.size();++o) {
                        const auto& objective=definition.Stages[s].second[o];
                        if(objective.Marker)reconcile(objective.Key,*objective.Marker,!objective.Hidden && s==active && progress && progress->Count(s,o)<objective.Required.Count);
                    }
                }
            }catch(const std::exception& e){report(key,e.what());}
        }
    }
    void Load(const std::string& mod,const Json& data) {
        if(data.is_object() && data.contains("$declaration")) {
            auto pendingDeclarations=declarations;
            for(const auto& record:OwnedContent::Declarations(data,mod,"Quest")) {
                const auto found=pendingDeclarations.find(record.PersistenceID);
                if(found!=pendingDeclarations.end() && found->second.Owner!=record.Owner)
                    throw std::runtime_error("Quest declaration ownership transfer refused");
                pendingDeclarations[record.PersistenceID]=record;
            }
            declarations=std::move(pendingDeclarations);
            if(data.size()==1)return;
            throw std::runtime_error("A quest $declaration document may contain only declaration metadata");
        }
        auto pending=catalog;auto documents=definitions;
        const auto add=[&](const Json& entry) {
            if(documents.size()>=128)throw std::runtime_error("Quest definition limit reached");
            pending.Add(mod,entry);documents.emplace(Parse(mod,entry).Key,entry);
        };
        if(data.is_array())for(const auto& entry:data)add(entry);else add(data);
        catalog=std::move(pending);definitions=std::move(documents);
        for(const auto& [key,document]:definitions){const auto& q=catalog.Find("_",key);if(q.Marker)hasLocations=true;for(const auto& [id,entries]:q.Stages)for(const auto& entry:entries)if(entry.Marker)hasLocations=true;}
    }
    bool Empty() const {return definitions.empty() && declarations.empty();}
    bool HasKills() const {for(const auto& [key,document]:definitions){const auto& q=catalog.Find("_",key);if(q.Kill)return true;for(const auto& [id,entries]:q.Stages)for(const auto& e:entries)if(e.Kill)return true;}return false;}
    template<class Visit> void ForEachKill(Visit visit) const {for(const auto& [key,document]:definitions){const auto& def=catalog.Find("_",key);if(def.Kill || !def.Stages.empty())visit(key,def,document);}}
    const Json& Document(const std::string& key) const {return definitions.at(key);}
    const Definition& Find(const std::string& mod,const Json& key) const {
        const auto& result=catalog.Find(mod,key);
        if(hidden.contains(result.Key))throw std::runtime_error("Internal save records are not playable quests");
        return result;
    }
    RC::Unreal::UObject* Asset(const std::string& key) const {
        const auto found=assets.find(key);
        if(found==assets.end())throw std::runtime_error("Quest asset is not prepared for this session");
        return found->second->Get();
    }
    void Prepare(RC::Unreal::UObject* context) {
        using namespace RC::Unreal;
        if(Empty())return;
        std::set<std::string> visiting,visited;
        const auto validateChain=[&](auto&& self,const std::string& key)->void {
            if(visited.contains(key))return;
            if(!visiting.insert(key).second)throw std::runtime_error("Quest prerequisite cycle: "+key);
            for(const auto& parent:Find("_",key).Prerequisites)self(self,parent);
            visiting.erase(key);visited.insert(key);
        };
        for(const auto& [key,document]:definitions)if(!hidden.contains(key))validateChain(validateChain,key);
        if(!context || !context->GetWorld())throw std::runtime_error("Quest world unavailable");
        auto* instance=SpawnRuntime::CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),TEXT("/Script/Engine.Default__GameplayStatics"),context);
        auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestDataSubsystem"));
        if(!instance || !type)throw std::runtime_error("Quest game-instance subsystem unavailable");
        TArray<UObject*> candidates;UECustom::UObjectGlobals::GetObjectsOfClass(type,candidates,true);
        UObject* subsystem=nullptr;
        for(auto* candidate:candidates)if(candidate && candidate->GetOuterPrivate()==instance
            && !candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
            if(subsystem)throw std::runtime_error("Quest subsystem is ambiguous for this game instance");
            subsystem=candidate;
        }
        if(!subsystem)throw std::runtime_error("Quest subsystem has not initialized");
        Json manifest=Json::array();
        std::vector<OwnedContent::Record> verifiedDeclarations;
        auto* questType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.QuestData"));
        if(!declarations.empty() && !questType)throw std::runtime_error("QuestData class unavailable for declarations");
        for(const auto& [id,record]:declarations) {
            auto* object=UECustom::UObjectGlobals::StaticFindObject<RC::Unreal::UObject*>(nullptr,nullptr,
                RC::to_generic_string(record.Source).c_str(),false);
            if(!object) {
                UECustom::TSoftObjectPtr<RC::Unreal::UObject> soft{UECustom::FSoftObjectPath(RC::to_generic_string(record.Source))};
                object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }
            if(!object || !object->IsA(questType))throw std::runtime_error("Cooked quest declaration did not resolve to QuestData: "+record.Source);
            const auto identity=[&](const TCHAR* name) {
                auto* field=RC::Unreal::CastField<RC::Unreal::FStrProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name));
                return field?RC::to_string(*field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(object))):std::string{};
            };
            const auto actualName=identity(TEXT("InternalName"));
            if(identity(TEXT("PersistenceID"))!=record.PersistenceID || actualName.empty()
                || (record.InternalNameAsserted && actualName!=record.InternalName))
                throw std::runtime_error("Cooked quest declaration identity mismatch: "+record.Source);
            object->SetRootSet();declaredAssets[id]=object;
            const auto netId=QuestRegistry::NativeRegistry::Register(subsystem,instance,object);
            manifest.push_back({{"Key",record.InternalName},{"NetId",netId},{"Declaration",record.Source}});
            auto verified=record;verified.InternalName=actualName;verifiedDeclarations.push_back(std::move(verified));
        }
        if(!verifiedDeclarations.empty())
            OwnedContent::Merge(OwnedContent::LedgerPath(PS::HostServices::StateDirectory()),verifiedDeclarations);
        for(const auto& [key,document]:definitions) {
            if(!assets.contains(key))assets.emplace(key,std::make_unique<NativeAsset>(catalog.Find("_",key),hidden.contains(key)));
            assets.at(key)->EnsureIdentity(catalog.Find("_",key));
            const auto netId=QuestRegistry::NativeRegistry::Register(subsystem,instance,Asset(key));
            manifest.push_back({{"Key",key},{"NetId",netId},{"Definition",document}});
            assets.at(key)->MarkRegistered();
        }
        networkManifest=std::move(manifest);
    }
    const Json& NetworkManifest() const {return networkManifest;}
};
}
