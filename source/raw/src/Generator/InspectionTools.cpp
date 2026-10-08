#include "Generator/InspectionTools.h"
#include "Generator/NiagaraTest.h"
#include "Generator/NiagaraPreset.h"
#include "Generator/DiagnosticExport.h"
#include "Generator/TraceProfile.h"
#include "Generator/SaveViewer.h"
#include "Generator/AssetSearch.h"
#include "Generator/AssetTemplate.h"
#include "Generator/LoaderTemplate.h"
#include "Generator/StarterPreflight.h"
#include "Generator/LoaderSchemas.h"
#include "Core/ConfigFiles.h"
#include "Generator/ActivityRuleTemplate.h"
#include "Generator/DiagnosticPreset.h"
#include "Generator/DiagnosticLibrary.h"
#include "Generator/FocusedCapture.h"
#include "Generator/AppearanceTrace.h"
#include "Generator/PlayerTrace.h"
#include "Loader/PlayerGhost.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Runtime/HostServices.h"
#include "Runtime/NetworkContext.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <deque>
#include <cstdio>
#include <iomanip>
#include <random>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;
using nlohmann::json;

namespace PS::InspectionTools {
namespace {
    std::mutex mutex;
    enum class Action { None, Target, TraceTarget, TraceBrowse, TraceSelect, Player, Browse, Object, Preset, TraceStart, TraceStop, PlayerTraceStart, PlayerTraceStop, PlayerTraceMarker, SavedItems, SavedWorld, Clear, AssetSearch, AssetCapture, AssetReference, StarterCheck, NiagaraAttach, NiagaraRemove, NiagaraInspect, NiagaraArmorStart, NiagaraArmorStop };
    json appearanceTraceStart;
    struct Request { Action action = Action::None; std::string text; float range = 10000;
        std::chrono::steady_clock::time_point due; std::string selectedObject;
        int objectLimit = 250; };
    Request pending;
    std::atomic<bool> hasRequest = false;
    bool busy = false;
    std::mutex exportMutex;
    char exportName[97] = "";
    bool exportJsonc = false;
    struct HistoryEntry { std::string message; Section section; };
    std::deque<HistoryEntry> history;
    struct TraceControls {
        bool categories[5]{true,true,true,true,false};
        int seconds=30, maxEvents=1024;
        bool suppressTicks=true, captureFields=false, captureParameters=false, selectedObject=false;
        char filter[129]="", fields[2048]="[]";
        char includeTerms[2065]="",excludeTerms[2065]="";
        json Options() const {
            unsigned mask=0;
            for(unsigned i=0;i<5;++i) if(categories[i]) mask|=1u<<i;
            json options={{"Seconds",seconds},{"MaxEvents",maxEvents},{"Categories",mask},{"Filter",filter},
                {"SuppressTicks",suppressTicks},{"TraceSelectedObject",selectedObject},{"CaptureParameters",captureParameters}};
            if(captureFields) options["EventCaptures"]=json::parse(fields);
            options["IncludeAny"]=PlayerTrace::FilterLines(includeTerms);
            options["ExcludeAny"]=PlayerTrace::FilterLines(excludeTerms);
            PlayerTrace::ValidateOptions(options);
            return options;
        }
        void Load(const json& options) {
            seconds=options.at("Seconds");maxEvents=options.at("MaxEvents");
            const unsigned mask=options.at("Categories");
            for(unsigned i=0;i<5;++i) categories[i]=(mask&(1u<<i))!=0;
            suppressTicks=options.at("SuppressTicks");selectedObject=options.at("TraceSelectedObject");
            captureParameters=options.at("CaptureParameters");captureFields=options.contains("EventCaptures");
            std::snprintf(filter,sizeof(filter),"%s",options.at("Filter").get_ref<const std::string&>().c_str());
            const auto includes=PlayerTrace::FilterText(options.value("IncludeAny",json::array()));
            const auto excludes=PlayerTrace::FilterText(options.value("ExcludeAny",json::array()));
            std::snprintf(includeTerms,sizeof(includeTerms),"%s",includes.c_str());
            std::snprintf(excludeTerms,sizeof(excludeTerms),"%s",excludes.c_str());
            const auto paths=captureFields?options.at("EventCaptures").dump():"[]";
            std::snprintf(fields,sizeof(fields),"%s",paths.c_str());
        }
    } traceControls;
    std::string status = "Enter a world, then capture a snapshot.";
    std::string selectedPath;
    std::string traceTargetPath;
    std::vector<std::string> traceTargetMatches;
    json selectedTargetResolver;
    thread_local json captureOrigin={{"mode","unknown"}};
    json eventTraceOrigin={{"mode","unknown"}};
    void RememberStatus(Section section) {
        static std::string last;
        if (status.empty() || status == last) return;
        last = status;
        history.push_back({status, section});
        while (history.size() > 24) history.pop_front();
    }
    std::string ResolveProfileTarget(const json& target);
    void RenderDiagnosticGuide(const char* section) {
        if(!ImGui::CollapsingHeader("Diagnostic guide"))return;
        static json guide;
        static std::string error;
        static bool loaded=false;
        if(ImGui::Button("Reload guide"))loaded=false;
        if(!loaded) {
            loaded=true;guide=nullptr;error.clear();
            try {
                auto value=json::parse(ConfigFiles::Read(PS::HostServices::SearchesDirectory()/"diagnostic-guide.json",16384));
                if(!value.is_object() || value.value("Version",json{})!=1)throw std::runtime_error("Unsupported guide version");
                for(const auto* name:{"Presets","Traces","Libraries"}) {
                    if(!value.contains(name) || !value.at(name).is_array() || value.at(name).size()>32)throw std::runtime_error("Invalid guide section");
                    for(const auto& line:value.at(name))if(!line.is_string() || line.get_ref<const std::string&>().size()>2048)throw std::runtime_error("Invalid guide text");
                }
                guide=std::move(value);
            }catch(const std::exception& e){error=e.what();}
        }
        if(!error.empty())ImGui::TextWrapped("Guide unavailable: %s. Expected runtime/live/jobs/searches/diagnostic-guide.json. Diagnostic tools remain available.",error.c_str());
        else for(const auto* name:{section,"Libraries"})for(const auto& line:guide.at(name))ImGui::TextWrapped("%s",line.get_ref<const std::string&>().c_str());
    }
    void RenderRejectedDiagnostics(const char* kind,const std::vector<std::string>& errors,const std::filesystem::path& folder) {
        if(errors.empty())return;
        ImGui::PushID(kind);
        ImGui::TextWrapped("%s library: %zu issue(s). Rejected files are not available for selection.",kind,errors.size());
        if(ImGui::CollapsingHeader("Rejected files and reasons",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("Folder: %s",folder.string().c_str());
            for(const auto& error:errors)ImGui::TextWrapped("%s",error.c_str());
            ImGui::TextWrapped("Correct the reported setting in the file, then reload this library. A library limit means some files were not checked.");
        }
        ImGui::PopID();
    }
    void RenderTraceProfiles() {
        static std::vector<json> profiles;
        static std::vector<std::string> errors;
        static bool loaded=false;
        static int selected=-1;
        static char name[97]="MyTrace", description[1025]="";
        static ImGuiTextFilter profileFilter;
        const auto folder=PS::HostServices::TraceProfilesDirectory();
        if (!ImGui::CollapsingHeader("Profile: load configured controls", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::TextWrapped("Profiles configure the live controls below. Loading a profile does not start a trace.");
        RenderDiagnosticGuide("Traces");
        ImGui::SeparatorText("Select and load");
        if(ImGui::Button("Reload trace profiles"))loaded=false;
        if(!loaded) {
            loaded=true;profiles.clear();errors.clear();selected=-1;
            auto library=DiagnosticLibrary::Load(folder,{64,256,32768},[](const std::string& text){return ValidateTraceProfile(ParseTraceProfile(text));});
            profiles=std::move(library.Documents);errors=std::move(library.Errors);
        }
        RenderRejectedDiagnostics("Trace profile",errors,folder);
        profileFilter.Draw("Filter trace profiles",-1.0f);
        if(ImGui::BeginCombo("Trace profile",selected>=0?profiles[selected]["Name"].get_ref<const std::string&>().c_str():"Select profile")) {
            bool matched=false;
            for(int i=0;i<static_cast<int>(profiles.size());++i) {
                const auto searchable=profiles[i].at("Name").get<std::string>()+" "+profiles[i].value("Description",std::string{});
                if(!profileFilter.PassFilter(searchable.c_str()))continue;
                matched=true;
                ImGui::PushID(i);
                if(ImGui::Selectable(profiles[i]["Name"].get_ref<const std::string&>().c_str(),selected==i))selected=i;
                ImGui::PopID();
            }
            if(!matched)ImGui::TextDisabled("No matching trace profiles");
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(selected<0);
        if(ImGui::Button("Load trace profile")) {
            const auto& profile=profiles[selected];
            traceControls.Load(profile["Trace"]);
            std::snprintf(name,sizeof(name),"%s",profile["Name"].get_ref<const std::string&>().c_str());
            std::snprintf(description,sizeof(description),"%s",profile.value("Description",std::string{}).c_str());
            {std::lock_guard lock(exportMutex);
             std::snprintf(exportName,sizeof(exportName),"%s",profile["Export"].value("Name",std::string{}).c_str());
             exportJsonc=profile["Export"].value("Jsonc",false);}
            selectedTargetResolver=profile.value("Target",json{});
            traceTargetPath=TraceTargetAfterProfileLoad(profile,traceTargetPath,
                profile.contains("Target")?ResolveProfileTarget(profile["Target"]):std::string{});
            status=profile.contains("Target")
                ? (traceTargetPath.empty()?"Profile loaded, but its target is not currently live.":"Profile loaded and target resolved. Recording unchanged.")
                : "Profile loaded. Recording unchanged.";
        }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Profile metadata");
        if(selected>=0) ImGui::TextWrapped("%s",profiles[selected].value("Description",std::string{}).c_str());
        ImGui::InputText("Profile name",name,sizeof(name));
        ImGui::InputText("Profile instructions",description,sizeof(description));
        if(ImGui::Button("Save trace profile JSONC")) {
            try {
                json output;
                {std::lock_guard lock(exportMutex);output={{"Name",exportName},{"Jsonc",exportJsonc}};}
                json draft={{"Version",1},{"Name",name},{"Description",description},{"Trace",traceControls.Options()},{"Export",output}};
                if(!selectedTargetResolver.is_null() && !selectedTargetResolver.empty())draft["Target"]=selectedTargetResolver;
                else if(!traceTargetPath.empty())draft["Target"]={{"Mode","ExactPath"},{"Path",traceTargetPath}};
                const auto profile=ValidateTraceProfile(draft);
                const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                const auto path=folder/DiagnosticExportName(name,"Trace",std::to_string(stamp),true);
                if(std::filesystem::exists(path))throw std::runtime_error("Profile exists; retry.");
                ConfigFiles::Write(path,"// Load controls only; does not start recording or enable mods.\n"+profile.dump(2));
                status="Saved trace profile: "+path.string();loaded=false;
            }catch(const std::exception& error){status=error.what();}
        }
    }
    json snapshot;
    json assetMatches=json::array(),assetFields=json::object(),assetDraft;
    json assetRecord,assetHints=json::object();
    std::string assetLoader="assets";
    std::unordered_set<std::string> assetChosen;
    std::string assetSource,assetPreview;
    json starterBaseline;
    char starterEditor[65536]{};
    std::string assetSearchQuery,objectSearchQuery;
    bool assetSearchComplete=false,objectSearchComplete=false;
    json saveReports[2];
    unsigned saveRevision[2]{};
    json lastEventTrace;
    unsigned eventTraceRevision=0;
    std::string snapshotText;
    bool snapshotTextDirty=true;
    unsigned metadataBudget=16384;
    std::vector<std::string> matches;
    char search[256] = "Jump";
    int objectResultLimit = 250;
    int completedObjectResultLimit = 250;
    ImGuiTextFilter objectResultsFilter;
    ImGuiTextFilter propertyFilter;
    float rangeMeters = 100;
    bool delay = true;
    int playerCaptureDelay = 0;
    char presetEditor[16384] = "{\n  \"Name\": \"MyPlayer\",\n  \"Objects\": [],\n  \"ControllerProperties\": [],\n  \"IncludePlayer\": true,\n  \"IncludeControllerComponents\": true\n}";
    std::vector<json> presets;
    int selectedPreset = -1;
    bool presetsLoaded = false;
    ImGuiTextFilter presetFilter;
    std::atomic<bool> extendedCaptureDepth = false;
    std::vector<std::string> presetErrors;
    std::filesystem::path PresetFolder() {
        return PS::HostServices::PresetsDirectory();
    }
    void ReloadPresets() {
        presets.clear(); presetErrors.clear(); selectedPreset = -1; presetsLoaded = true;
        auto library=DiagnosticLibrary::Load(PresetFolder(),{256,1024,16383},[](const std::string& text){
            size_t nodes=0;
            auto preset=json::parse(text,[&](int depth,json::parse_event_t,json&){
                if(depth>32 || ++nodes>8192)throw std::runtime_error("Preset structure exceeds limits");return true;
            },true,true);
            ValidatePreset(preset,extendedCaptureDepth.load());return preset;
        });
        status="Loaded "+std::to_string(library.Documents.size())+" presets; rejected "+std::to_string(library.Rejected)+" files.";
        if(library.Truncated)status+=" Library limit reached; see details.";
        presets=std::move(library.Documents);presetErrors=std::move(library.Errors);
    }

    std::string Path(UObject* object) { return object ? to_string(object->GetPathName()) : ""; }
    std::string Lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }
    template<class T = UObject*> T Find(const CharType* path) {
        return UECustom::UObjectGlobals::StaticFindObject<T>(nullptr, nullptr, path);
    }
    bool IsTraceInstance(UObject* object) {
        return object && !object->IsA(UClass::StaticClass())
            && !object->IsA(UScriptStruct::StaticClass()) && !object->IsA(UFunction::StaticClass())
            && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject));
    }
    void Queue(Action action, std::string text = {}) {
        if (busy) return;
        int waitSeconds = action == Action::Player ? playerCaptureDelay
            : action == Action::TraceTarget || (action == Action::Target && delay) ? 3 : 0;
        if (action == Action::Preset) {
            playerCaptureDelay = json::parse(text).value("CaptureDelaySeconds", 0);
            waitSeconds = playerCaptureDelay;
        }
        auto inspectedPath = selectedPath;
        if (inspectedPath.empty() && snapshot.is_object() && snapshot.contains("object") && snapshot["object"].is_object())
            inspectedPath = snapshot["object"].value("path", std::string{});
        if(action==Action::PlayerTraceStart)inspectedPath=traceTargetPath;
        pending = { action, std::move(text), rangeMeters * 100,
            std::chrono::steady_clock::now() + std::chrono::seconds(waitSeconds), std::move(inspectedPath) };
        busy = true;
        if(action==Action::SavedItems || action==Action::SavedWorld) {
            const int index=action==Action::SavedWorld?1:0;
            saveReports[index]=nullptr;++saveRevision[index];
        }
        hasRequest = true;
        if(action == Action::Object)selectedPath = pending.text;
        status = waitSeconds > 0 ? "Capture scheduled: return to the game before the countdown ends." : "Capture queued...";
    }
    std::string ResolveProfileTarget(const json& target) {
        const auto mode=target.value("Mode",std::string("ExactPath"));
        if(mode=="ExactPath") {
            const auto path=target.value("Path",std::string{});
            auto* object=path.empty()?nullptr:Find(to_wstring(path).c_str());
            return object?Path(object):std::string{};
        }
        const auto classContains=Lower(target.value("ClassContains",std::string{}));
        const auto nameContains=Lower(target.value("NameContains",std::string{}));
        std::string result;
        UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
            if(!object || !object->GetClassPrivate()
                || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))return LoopAction::Continue;
            const auto classPath=Lower(Path(object->GetClassPrivate()));
            const auto objectName=Lower(to_string(object->GetName()));
            if(classPath.find(classContains)==classPath.npos || objectName.find(nameContains)==objectName.npos)return LoopAction::Continue;
            result=Path(object);return LoopAction::Break;
        });
        return result;
    }
    json Fields(UStruct* type, void* data = nullptr) {
        json fields = json::array();
        for (auto* p : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
            if(!metadataBudget)throw std::runtime_error("Metadata limit reached (16384 fields); use a focused preset.");
            --metadataBudget;
            json f = {{"name", to_string(p->GetName())}, {"type", PropertyHelper::GetPropertyTypeAsUTF8String(p)},
                {"offset", p->GetOffset_Internal()}, {"arrayDim", p->GetArrayDim()},
                {"elementSize", p->GetElementSize()}, {"flags", static_cast<uint64_t>(p->GetPropertyFlags())}};
            if (auto* s = CastField<FStructProperty>(p)) f["struct"] = Path(s->GetStruct().Get());
            if (auto* a = CastField<FArrayProperty>(p)) f["elementType"] = PropertyHelper::GetPropertyTypeAsUTF8String(a->GetInner());
            if (data && p->GetArrayDim() == 1 && p->GetOffset_Internal() >= 0) {
              try {
                auto* address = p->ContainerPtrToValuePtr<void>(data);
                if (auto* b = CastField<FBoolProperty>(p)) f["value"] = b->GetPropertyValue(address);
                else if (auto* n = CastField<FNumericProperty>(p)) {
                    if (n->IsInteger()) f["value"] = n->GetSignedIntPropertyValue(address);
                    else f["value"] = n->GetFloatingPointPropertyValue(address);
                } else if (auto* o = CastField<FObjectPropertyBase>(p)) {
                    f["value"] = Path(o->GetObjectPropertyValue(address));
                    f["objectReference"] = true;
                }
              } catch (const std::exception& error) { f["readError"] = error.what(); }
            }
            fields.push_back(std::move(f));
        }
        return fields;
    }
    json Describe(UObject* object) {
        if (!object || !object->GetClassPrivate()) throw std::runtime_error("Object is no longer available. Capture again.");
        json result = {{"path", Path(object)}, {"class", Path(object->GetClassPrivate())}, {"hierarchy", json::array()}};
        unsigned depth=0;
        for (UStruct* type = object->GetClassPrivate(); type; type = type->GetSuperStruct()) {
            if(++depth>32)throw std::runtime_error("Class hierarchy exceeds capture limit.");
            result["hierarchy"].push_back({{"class", Path(type)}, {"fields", Fields(type, object)}});
        }
        return result;
    }
    json CaptureGameplayEffects(UObject* component) {
        json report = {{"component", Path(component)}, {"arrays", json::object()}};
        auto* effectClass = Find<UClass*>(TEXT("/Script/Dominion.DominionGameplayEffect"));
        if (!effectClass) return {{"error", "Gameplay effect class unavailable"}};
        for (UStruct* type = component->GetClassPrivate(); type; type = type->GetSuperStruct()) {
            if (Path(type) != "/Script/Dominion.DominionGameplayEffectsComponent") continue;
            for (auto* property : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
                const auto name = to_string(property->GetName());
                if (name != "ReplicatedInstances" && name != "NonReplicatedInstances") continue;
                auto& output = report["arrays"][name];
                auto* arrayProperty = CastField<FArrayProperty>(property);
                auto* inner = arrayProperty ? CastField<FObjectProperty>(arrayProperty->GetInner()) : nullptr;
                if (!inner || property->GetArrayDim() != 1 || property->GetOffset_Internal() < 0
                    || inner->GetElementSize() != sizeof(UObject*)) {
                    output = {{"error", "Unsupported effect array layout"}};
                    continue;
                }
                auto* array = property->ContainerPtrToValuePtr<FScriptArray>(component);
                const auto count = array->Num();
                if (count < 0 || (count > 0 && !array->GetData())) {
                    output = {{"error", "Invalid effect array header"}};
                    continue;
                }
                output = {{"count", count}, {"truncated", count > 128}, {"entries", json::array()}};
                for (int32 index = 0; index < std::min<int32>(count, 128); ++index) {
                    auto* address = static_cast<uint8*>(array->GetData()) + index * sizeof(UObject*);
                    auto* effect = inner->GetObjectPropertyValue(address);
                    json entry = {{"index", index}};
                    if (!effect) entry["null"] = true;
                    else if (!effect->IsA(effectClass)) entry["error"] = "Unexpected effect class";
                    else {
                        entry["object"] = Describe(effect);
                        if (Path(effect->GetClassPrivate()) == "/Script/Dominion.ShadowVeilGameplayEffectInstance") {
                            auto* sourceField = CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(effect->GetClassPrivate(), TEXT("Source")));
                            if (sourceField && sourceField->GetArrayDim() == 1 && sourceField->GetElementSize() == sizeof(UObject*)) {
                                auto* source = sourceField->GetObjectPropertyValue(sourceField->ContainerPtrToValuePtr<void>(effect));
                                if (source) entry["sourceObject"] = Describe(source);
                            }
                        }
                    }
                    output["entries"].push_back(std::move(entry));
                }
            }
        }
        return report;
    }
    UObject* Controller() {
        auto* type = Find<UClass*>(TEXT("/Script/Dominion.DominionPlayerController"));
        if (!type) throw std::runtime_error("Player controller class unavailable. Enter a world first.");
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(type, controllers, true);
        for (auto* controller : controllers) {
            if (!controller || !controller->GetWorld() || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            ActorHelper::FunctionCall call(controller, TEXT("/Script/Engine.Controller:IsLocalController"));
            call.Invoke();
            if (call.Result<bool>()) return controller;
        }
        throw std::runtime_error("No local player found. Enter a world first; dedicated servers have no local camera.");
    }
    UObject* Pawn(UObject* controller) {
        ActorHelper::FunctionCall call(controller, TEXT("/Script/Engine.Controller:K2_GetPawn"));
        call.Invoke(); return call.Result<UObject*>();
    }
    json Capture(UObject* object) {
        json result = {{"object", Describe(object)}, {"components", json::array()}, {"functions", json::array()},
            {"liveGameplayEffects", json::array()},
            {"note", "On-demand snapshot. Scalar and resolved object-reference values; gameplay effect instance arrays capped at 128 entries each. Other arrays are metadata. No property editing or function invocation controls."}};
        auto* actorClass = Find<UClass*>(TEXT("/Script/Engine.Actor"));
        if (actorClass && object->IsA(actorClass)) {
            auto* componentClass = Find<UClass*>(TEXT("/Script/Engine.ActorComponent"));
            if (componentClass) {
                ActorHelper::FunctionCall call(object, TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
                call.Arg(TEXT("ComponentClass"), componentClass).Invoke();
                TArray<UObject*> components;
                call.MoveResult(&components, sizeof(components));
                int count = 0;
                auto* effectsClass = Find<UClass*>(TEXT("/Script/Dominion.DominionGameplayEffectsComponent"));
                for (auto* component : components) {
                    if (component && count++ < 128) {
                        result["components"].push_back(Describe(component));
                        if (effectsClass && component->IsA(effectsClass))
                            result["liveGameplayEffects"].push_back(CaptureGameplayEffects(component));
                    }
                }
                result["componentsTruncated"] = count > 128;
            }
        }
        std::vector<std::string> prefixes;
        for (const auto& level : result["object"]["hierarchy"]) prefixes.push_back(level["class"].get<std::string>() + ":");
        for (const auto& component : result["components"])
            for (const auto& level : component["hierarchy"]) prefixes.push_back(level["class"].get<std::string>() + ":");
        UObjectGlobals::ForEachUObject([&](UObject* candidate, int32_t, int32_t) -> LoopAction {
            if (candidate && candidate->IsA(UFunction::StaticClass())) {
                const auto path = Path(candidate);
                if (std::any_of(prefixes.begin(), prefixes.end(), [&](const auto& prefix) { return path.starts_with(prefix); })) {
                    if(result["functions"].size()>=1024) {result["functionsTruncated"]=true;return LoopAction::Break;}
                    auto* fn = static_cast<UFunction*>(candidate);
                    result["functions"].push_back({{"path", path}, {"parameters", Fields(fn)}, {"parameterSize", fn->GetParmsSize()}});
                }
            }
            return LoopAction::Continue;
        });
        return result;
    }
    json ConfigurationValue(FProperty* p, void* address, int depth = 0) {
        if(!metadataBudget)throw std::runtime_error("Configuration capture limit reached; use a focused preset.");
        --metadataBudget;
        if (depth > 5 || p->GetArrayDim() != 1) return {{"omitted", "depth or static array limit"}};
        if (auto* b = CastField<FBoolProperty>(p)) return b->GetPropertyValue(address);
        if (auto* n = CastField<FNumericProperty>(p)) {
            if (n->IsInteger()) return n->GetSignedIntPropertyValue(address);
            return n->GetFloatingPointPropertyValue(address);
        }
        if (CastField<FNameProperty>(p)) return to_string(static_cast<FName*>(address)->ToString());
        if (auto* o = CastField<FObjectPropertyBase>(p)) return Path(o->GetObjectPropertyValue(address));
        if (auto* s = CastField<FStructProperty>(p)) {
            json values = json::object();
            for (auto* child : TFieldRange<FProperty>(s->GetStruct().Get(), EFieldIterationFlags::None))
                values[to_string(child->GetName())] = ConfigurationValue(child, child->ContainerPtrToValuePtr<void>(address), depth + 1);
            return values;
        }
        if (auto* a = CastField<FArrayProperty>(p)) {
            json entries = json::array();
            auto* array = static_cast<FScriptArray*>(address);
            const auto count = array->Num();
            const auto stride = a->GetInner()->GetElementSize();
            if (count < 0 || stride <= 0 || stride > 16384 || (count && !array->GetData()))
                return {{"error", "invalid array header"}};
            for (int32 i = 0; i < std::min<int32>(count, 128); ++i)
                entries.push_back(ConfigurationValue(a->GetInner(), static_cast<uint8*>(array->GetData()) + static_cast<size_t>(i) * stride, depth + 1));
            return {{"entries", entries}, {"count", count}, {"truncated", count > 128}};
        }
        return {{"metadataOnly", PropertyHelper::GetPropertyTypeAsUTF8String(p)}};
    }
    json ResearchObject(UObject* object) {
        json report = Describe(object);
        report["configuration"] = json::object();
        for (UStruct* type = object->GetClassPrivate(); type; type = type->GetSuperStruct()) {
            for (auto* p : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
                const auto name = to_string(p->GetName()); const auto lower = Lower(name);
                if (lower.find("category") == std::string::npos && lower.find("filter") == std::string::npos
                    && lower.find("tag") == std::string::npos && lower.find("weight") == std::string::npos
                    && lower.find("stack") == std::string::npos && lower.find("rune") == std::string::npos
                    && lower.find("anima") == std::string::npos && lower.find("allowed") == std::string::npos) continue;
                try { report["configuration"][name] = ConfigurationValue(p, p->ContainerPtrToValuePtr<void>(object)); }
                catch (const std::exception& error) { report["configuration"][name] = {{"error", error.what()}}; }
            }
        }
        return report;
    }
    json TypeMetadata(UStruct* type) {
        json report={{"type",Path(type)},{"fields",Fields(type)}};
        if(type->IsA(UClass::StaticClass())) {
            std::vector<std::string> prefixes;
            unsigned depth=0;
            report["hierarchy"]=json::array();
            report["functions"]=json::array();
            for(auto* parent=type;parent && depth++<32;parent=parent->GetSuperStruct()) {
                prefixes.push_back(Path(parent)+":");
                report["hierarchy"].push_back({{"type",Path(parent)},{"fields",Fields(parent)}});
            }
            UObjectGlobals::ForEachUObject([&](UObject* candidate,int32_t,int32_t)->LoopAction {
                if(!candidate || !candidate->IsA(UFunction::StaticClass()))return LoopAction::Continue;
                const auto path=Path(candidate);
                if(!std::any_of(prefixes.begin(),prefixes.end(),[&](const auto& prefix){return path.starts_with(prefix);}))
                    return LoopAction::Continue;
                if(report["functions"].size()>=1024) {
                    report["functionsTruncated"]=true;
                    return LoopAction::Break;
                }
                report["functions"].push_back(TypeMetadata(static_cast<UFunction*>(candidate)));
                return LoopAction::Continue;
            });
        }
        if (type->IsA(UFunction::StaticClass())) {
            auto* function=static_cast<UFunction*>(type);
            report["parameterSize"]=function->GetParmsSize();
            report["functionFlags"]=static_cast<uint64_t>(function->GetFunctionFlags());
            report["referencedStructs"]=json::array();
            unsigned count=0;
            for (auto* field : TFieldRange<FProperty>(function,EFieldIterationFlags::None)) {
                if (count++>=32) {report["referencedStructsTruncated"]=true;break;}
                auto* nested=CastField<FStructProperty>(field);
                if (!nested) if (auto* array=CastField<FArrayProperty>(field))
                    nested=CastField<FStructProperty>(array->GetInner());
                if (nested && nested->GetStruct().Get())
                    report["referencedStructs"].push_back({{"type",Path(nested->GetStruct().Get())},{"fields",Fields(nested->GetStruct().Get())}});
            }
        }
        return report;
    }
    std::string Export(const json& report, const std::string& prefix, bool ruleTemplate = false, bool assetTemplate = false,int starterMode=0, bool niagaraPreset=false) {
        const bool diagnostic=!ruleTemplate && !assetTemplate && !niagaraPreset && !prefix.starts_with("Schema-");
        const auto document=diagnostic?WithDiagnosticOrigin(report,captureOrigin):report;
        const std::string relativeFolder=niagaraPreset?"jobs/searches/presets/loaders/niagara":prefix.starts_with("Schema-")?"jobs/exports/schema":"jobs/exports";
        auto folder=niagaraPreset?PS::HostServices::PresetsDirectory()/"loaders"/"niagara":
            prefix.starts_with("Schema-")?PS::HostServices::ExportsDirectory()/"schema":PS::HostServices::ExportsDirectory();
        std::string category=prefix;
        if(niagaraPreset || prefix=="NiagaraAttachment")category="niagara";
        else if(ruleTemplate)category="players";
        else if(assetTemplate)category=assetLoader;
        else if(report.contains("Loader") && report["Loader"].is_string() && !report["Loader"].get<std::string>().empty())category=report["Loader"].get<std::string>();
        else if(prefix.starts_with("Schema-"))category=prefix.substr(7);
        else if(prefix.starts_with("Preset-"))category="preset";
        std::filesystem::create_directories(folder);
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::string name;
        bool jsonc;
        { std::lock_guard settingsLock(exportMutex);
          jsonc = ruleTemplate || assetTemplate || exportJsonc;
          name = DiagnosticExportName(exportName, prefix, std::to_string(stamp), jsonc,category); }
        if(diagnostic)name=DiagnosticOriginLabel(document)+"_"+name;
        if (std::filesystem::exists(folder / name))
            throw std::runtime_error("Export already exists; retry to generate a new timestamp.");
        std::ofstream file(folder / name);
        if (ruleTemplate) file << "// Review before installing in a mod's players folder. Export does not activate this rule.\n"
            "// Numeric IDs are unresolved; verify across sessions, servers and game versions.\n"
            "// Merge into an existing nameplate rule if needed; a separate rule may replace its states.\n";
        else if (jsonc && !assetTemplate) file << "// RuneSchema diagnostic report.\n";
        if(assetTemplate)file<<LoaderTemplate::Jsonc(assetLoader,starterMode,report);
        else file << std::setw(2) << document;
        file.flush();
        if (!file) throw std::runtime_error("Could not write research snapshot.");
        return relativeFolder+"/"+name;
    }
    void RenderEventRuleEditor() {
        if (!ImGui::CollapsingHeader("Advanced: Event Rules")) return;
        ImGui::TextWrapped("Create a nameplate rule from the last stopped trace. Export saves a JSONC template only; it does not install or activate it.");
        if (!lastEventTrace.is_object() || !lastEventTrace.contains("events")) {
            ImGui::TextWrapped("Stop a trace with Capture supported event parameters enabled first.");return;
        }
        static unsigned revision=0;
        static int selected=-1,selectedParameter=0;
        static char conditions[4096]="[]",state[65]="CustomActivity";
        static char icon[2049]="",player[129]="REPLACE_WITH_PLAYER_NAME";
        static float timeout=5.0f;
        if (revision!=eventTraceRevision) {
            revision=eventTraceRevision;selected=-1;selectedParameter=0;
            std::snprintf(conditions,sizeof(conditions),"[]");
        }
        const auto& events=lastEventTrace["events"];
        if (events.empty()) {ImGui::TextWrapped("No parameters captured. Enable Capture supported event parameters before starting a new trace.");return;}
        const auto preview=selected<0?std::string("Select captured event"):"Event "+std::to_string(selected);
        if (ImGui::BeginCombo("Captured event",preview.c_str())) {
            for (size_t i=0;i<events.size();++i) {
                const auto& event=events[i];
                if (!event.contains("parameters") || ActivityParameterSuggestions(event["parameters"]).empty()) continue;
                const auto function=event.value("function",std::string{});
                const auto label=std::to_string(i)+" | "+function.substr(function.find_last_of(':')+1);
                if (ImGui::Selectable(label.c_str(),selected==static_cast<int>(i))) {
                    selected=static_cast<int>(i);selectedParameter=0;
                    const auto suggestions=ActivityParameterSuggestions(event["parameters"]);
                    const auto initial=json::array({suggestions.front()}).dump(2);
                    std::snprintf(conditions,sizeof(conditions),"%s",initial.c_str());
                }
            }
            ImGui::EndCombo();
        }
        if (selected<0 || static_cast<size_t>(selected)>=events.size()) return;
        const auto& event=events[selected];
        ImGui::TextWrapped("%s",event.value("function",std::string{}).c_str());
        ImGui::TextWrapped("Asset resolution: unavailable for this capture. Numeric IDs remain unverified outside the tested sessions; no asset lookup is guessed.");
        const auto suggestions=ActivityParameterSuggestions(event["parameters"]);
        if (selectedParameter>=static_cast<int>(suggestions.size()))selectedParameter=0;
        if (!suggestions.empty()) {
            const auto label=suggestions[selectedParameter].dump();
            if (ImGui::BeginCombo("Captured parameter",label.c_str())) {
                for(size_t i=0;i<suggestions.size();++i) {
                    const auto item=suggestions[i].dump();
                    if(ImGui::Selectable(item.c_str(),selectedParameter==static_cast<int>(i)))selectedParameter=static_cast<int>(i);
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Add parameter condition")) {
                try {
                    auto values=json::parse(conditions);
                    if (!values.is_array() || values.size()>=8)throw std::runtime_error("Use at most eight conditions");
                    values.push_back(suggestions[selectedParameter]);
                    const auto text=values.dump(2);
                    if(text.size()>=sizeof(conditions))throw std::runtime_error("Condition editor is full");
                    std::snprintf(conditions,sizeof(conditions),"%s",text.c_str());
                }catch(const std::exception& error){status=error.what();}
            }
        }
        ImGui::InputTextMultiline("Conditions (all must match)",conditions,sizeof(conditions),ImVec2(-1,90));
        ImGui::InputText("Activity name",state,sizeof(state));
        ImGui::InputText("Cooked icon path",icon,sizeof(icon));
        ImGui::InputText("Player name (* for everyone)",player,sizeof(player));
        ImGui::InputFloat("Inactivity seconds",&timeout,1.0f,5.0f,"%.1f");
        ImGui::TextWrapped("Template defaults: hidden base, self visible, other players hidden. Review and merge with any existing nameplate rule before installation. Only compatible native events can be exported.");
        if (ImGui::Button("Export JSONC rule template")) {
            try {
                const auto rule=BuildActivityRuleTemplate(event,json::parse(conditions),state,icon,timeout,player);
                status="Template exported to runtime/"+Export(rule,"EventRule",true)+". Not installed.";
            }catch(const std::exception& error){status=error.what();}
        }
    }
    void ShowObject(const json& object) {
        ImGui::TextWrapped("%s", object.at("path").get_ref<const std::string&>().c_str());
        if (ImGui::Button("Copy class path")) ImGui::SetClipboardText(object.at("class").get_ref<const std::string&>().c_str());

        if (ImGui::Button("Copy object path")) ImGui::SetClipboardText(object.at("path").get_ref<const std::string&>().c_str());
        for (const auto& level : object.at("hierarchy")) {
            ImGui::TextWrapped("%s", level.at("class").get_ref<const std::string&>().c_str());
            for (const auto& field : level.at("fields")) {
                const auto line = field.at("name").get<std::string>() + " : " + field.at("type").get<std::string>()
                    + (field.contains("value") ? " = " + field.at("value").dump() : " [metadata]");
                if (propertyFilter.PassFilter(line.c_str())) {
                    ImGui::TextWrapped("%s", line.c_str());
                    if (field.value("objectReference", false) && field.contains("value") && !field["value"].get<std::string>().empty()) {
                        ImGui::PushID(field.at("name").get_ref<const std::string&>().c_str());
                        ImGui::BeginDisabled(busy);
                        if (ImGui::SmallButton("Inspect referenced object")) Queue(Action::Object, field["value"].get<std::string>());
                        ImGui::EndDisabled(); ImGui::PopID();
                    }
                }
            }
        }
    }
}
void Reset() {
    NiagaraTest::Reset();
    PlayerGhost::ClearArmorTest();
    PlayerTrace::Cancel();AppearanceTrace::Cancel();
    std::lock_guard lock(mutex);
    pending={};hasRequest=false;
    assetSearchComplete=false;objectSearchComplete=false;
    std::string().swap(assetSearchQuery);std::string().swap(objectSearchQuery);
    snapshot=nullptr;lastEventTrace=nullptr;appearanceTraceStart=nullptr;++eventTraceRevision;
    assetMatches=json::array();assetFields=json::object();assetDraft=nullptr;
    assetRecord=nullptr;assetHints=json::object();assetChosen.clear();
    starterBaseline=nullptr;starterEditor[0]='\0';
    std::string().swap(assetSource);std::string().swap(assetPreview);
    std::string().swap(snapshotText);snapshotTextDirty=true;
    std::vector<std::string>().swap(matches);std::string().swap(selectedPath);selectedTargetResolver=nullptr;
    traceTargetPath.clear();traceTargetMatches.clear();
    eventTraceOrigin={{"mode","unknown"}};
    for(int i=0;i<2;++i) {saveReports[i]=nullptr;++saveRevision[i];}
    busy=false;status="Recordings stopped; cached results cleared. Exported files preserved.";
}

void RenderPlayerNameplateBuilder(bool nameplateDefinition) {
    std::lock_guard lock(mutex);
    RememberStatus(Section::AssetTemplates);
    static char playerId[97]="all_players",playerSelector[129]="*",definitionId[97]="adventurer";
    static char nameplateId[97]="adventurer",baseIcon[2049]="",stateId[65]="Active",stateIcon[2049]="";
    static char functionPath[513]="",advancedPlayer[8193]="{}",draft[32769]="";
    static int mode=0,action=0;
    static float scale=1.0f,distance=2500.0f,timeout=3.0f;
    static bool client=true,server=true,addState=true,addEvent=false;
    static bool setScale=false,setHealth=false,setStamina=false;
    static float playerScale=1.0f,health=100.0f,stamina=100.0f;
    const char* modes[]{"Name","Icon","Hidden"};
    const char* actions[]{"Pulse","Activate","Deactivate"};
    ImGui::PushID(nameplateDefinition?"FocusedNameplateBuilder":"FocusedPlayerBuilder");
    if(nameplateDefinition) {
        ImGui::SeparatorText("Create new reusable nameplate");
        ImGui::InputText("Definition Id",nameplateId,sizeof(nameplateId));
        if(ImGui::BeginCombo("Base mode",modes[mode])) {for(int i=0;i<3;++i)if(ImGui::Selectable(modes[i],mode==i))mode=i;ImGui::EndCombo();}
        ImGui::InputText("Base cooked icon",baseIcon,sizeof(baseIcon));
        ImGui::InputFloat("Base scale",&scale,.1f,.25f,"%.2f");
        ImGui::InputFloat("Visible distance",&distance,100.f,500.f,"%.0f");
        ImGui::InputFloat("Activity timeout",&timeout,.5f,1.f,"%.1f");
        ImGui::Checkbox("Show to this client",&client);ImGui::SameLine();ImGui::Checkbox("Show to server / other players",&server);
        ImGui::SeparatorText("Activity state and icon");
        ImGui::Checkbox("Add activity state",&addState);
        ImGui::BeginDisabled(!addState);
        ImGui::InputText("State Id",stateId,sizeof(stateId));
        ImGui::InputText("State cooked icon",stateIcon,sizeof(stateIcon));
        ImGui::Checkbox("Bind an exact event",&addEvent);
        ImGui::BeginDisabled(!addEvent);
        ImGui::InputText("Function /Class.Class_C:Function",functionPath,sizeof(functionPath));
        if(ImGui::BeginCombo("Event action",actions[action])) {for(int i=0;i<3;++i)if(ImGui::Selectable(actions[i],action==i))action=i;ImGui::EndCombo();}
        ImGui::EndDisabled();ImGui::EndDisabled();
        ImGui::TextWrapped("The state owns its icon. Use Traces > Advanced: Event Rules to capture parameter conditions. Displayed activity counters require a later runtime checkpoint and are intentionally omitted here.");
    } else {
        ImGui::SeparatorText("Create new player rule");
        ImGui::InputText("Rule Id",playerId,sizeof(playerId));
        ImGui::InputText("Player name (* for everyone)",playerSelector,sizeof(playerSelector));
        ImGui::InputText("Reusable Nameplate.Definition",definitionId,sizeof(definitionId));
        ImGui::Checkbox("Set scale",&setScale);ImGui::SameLine();ImGui::InputFloat("##PlayerScale",&playerScale,.1f,.25f,"%.2f");
        ImGui::Checkbox("Set maximum health",&setHealth);ImGui::SameLine();ImGui::InputFloat("##PlayerHealth",&health,10.f,100.f,"%.0f");
        ImGui::Checkbox("Set maximum stamina",&setStamina);ImGui::SameLine();ImGui::InputFloat("##PlayerStamina",&stamina,10.f,100.f,"%.0f");
        ImGui::TextWrapped("Advanced fields merge into this rule. Use them for implemented appearance, movement, capacity, attack, defense, named attributes, archetype, visual effect, and nameplate overrides. Identity and selector fields above remain authoritative.");
        ImGui::InputTextMultiline("Advanced player fields JSON object",advancedPlayer,sizeof(advancedPlayer),ImVec2(-1,110));
    }
    if(ImGui::Button("Build create-new preview")) {
        try {
            json document=json::array();
            if(nameplateDefinition) {
                if(!ActivityIdentifier(nameplateId))throw std::runtime_error("Definition Id must use letters, numbers or underscore");
                json body={{"Mode",modes[mode]},{"Scale",scale},{"Distance",distance},
                    {"ActivityTimeoutSeconds",timeout},{"Client",client?"Yes":"No"},{"Server",server?"Yes":"No"}};
                if(baseIcon[0])body["Icon"]=baseIcon;
                if(mode==1&&!baseIcon[0])throw std::runtime_error("Icon mode requires a base cooked icon");
                if(addState) {
                    if(!ActivityIdentifier(stateId)||!stateIcon[0])throw std::runtime_error("Activity state requires a valid Id and cooked icon");
                    body["States"]={{stateId,{{"Icon",stateIcon},{"Scale",.9}}}};
                    if(addEvent) {
                        json event={{"Function",functionPath},{"State",stateId},{"Action",actions[action]}};
                        body["Events"]=json::array({event});ValidateActivityEvents(body["Events"],body["States"]);
                    }
                }
                document.push_back({{"Id",nameplateId},{"Nameplate",body}});
            } else {
                if(!ActivityIdentifier(playerId))throw std::runtime_error("Rule Id must use letters, numbers or underscore");
                if(!playerSelector[0])throw std::runtime_error("Player selector is required");
                json body=json::parse(advancedPlayer);
                if(!body.is_object())throw std::runtime_error("Advanced player fields must be a JSON object");
                for(const auto* protectedField:{"Id","$Id","PlayerName","PlayerNames","PlayerGuid","PlayerGuids"})body.erase(protectedField);
                body["Id"]=playerId;body["PlayerName"]=playerSelector;
                if(definitionId[0])body["Nameplate"]["Definition"]=definitionId;
                if(setScale)body["Scale"]=playerScale;
                if(setHealth)body["MaxHealth"]=health;
                if(setStamina)body["MaxStamina"]=stamina;
                document.push_back(std::move(body));
            }
            const auto text=document.dump(2);if(text.size()>=sizeof(draft))throw std::runtime_error("Draft exceeds 32 KiB");
            std::snprintf(draft,sizeof(draft),"%s",text.c_str());status="Create-new preview ready. Review and export; no mod was installed.";
        }catch(const std::exception& error){status=error.what();}
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!draft[0]);
    if(ImGui::Button("Export JSONC draft")) {
        try {const auto document=json::parse(draft);status="Exported runtime/"+Export(document,nameplateDefinition?"Nameplate":"PlayerRule",!nameplateDefinition)+". Not installed.";}
        catch(const std::exception& error){status=error.what();}
    }
    ImGui::EndDisabled();
    if(draft[0]&&ImGui::CollapsingHeader("Editable create-new JSON",ImGuiTreeNodeFlags_DefaultOpen))
        ImGui::InputTextMultiline("##FocusedDraft",draft,sizeof(draft),ImVec2(-1,180));
    ImGui::TextWrapped("%s",status.c_str());
    ImGui::PopID();
}
void Tick() {
    PlayerTrace::Tick();
    AppearanceTrace::Tick();
    if (!hasRequest.load(std::memory_order_relaxed)) return;
    Request request;
    { std::lock_guard lock(mutex);
      if (pending.action == Action::None || std::chrono::steady_clock::now() < pending.due) return;
      request = std::move(pending); pending = {}; snapshot = nullptr; snapshotTextDirty=true; hasRequest = false; }
    try {
        metadataBudget=16384;
        captureOrigin={{"mode","unknown"},{"source","native world network-mode queries"}};
        struct OriginGuard { ~OriginGuard(){captureOrigin={{"mode","unknown"}};} } originGuard;
        try {
            const bool objectTrace=request.action==Action::PlayerTraceStart
                && json::parse(request.text).value("TraceSelectedObject",false);
            auto* preferred=!objectTrace || request.selectedObject.empty()?nullptr:Find(to_wstring(request.selectedObject).c_str());
            const auto network=PS::Network::Detect(preferred);
            captureOrigin["mode"]=std::string(PS::Network::Label(network.Mode));
            captureOrigin["world"]=Path(network.World);
        }catch(const std::exception& error){captureOrigin["error"]=error.what();}
        json result;
        if(request.action==Action::AssetReference) {
            const auto options=json::parse(request.text);
            auto report=AssetSearch::Reference(options.at("Entry"),options.at("Loader"));
            const auto file=Export(report,"Reference",false,true,4);
            std::lock_guard lock(mutex);snapshot=std::move(report);snapshotTextDirty=true;busy=false;
                status="Reference saved to runtime/"+file+". Not an installable mod; view in Tools > Results.";return;
        }
        if(request.action==Action::NiagaraInspect) {
            auto report=NiagaraTest::Inspect();
            const auto file=Export(report,"NiagaraAttachment");
            std::lock_guard lock(mutex);snapshot=std::move(report);snapshotTextDirty=true;
            status="Attachment snapshot saved to runtime/"+file+". View in Results.";busy=false;return;
        }
        if(request.action==Action::NiagaraArmorStart || request.action==Action::NiagaraArmorStop) {
            std::string message;
            if(request.action==Action::NiagaraArmorStart) {
                const json effect={{"Type","Niagara"},
                    {"System","/Game/Art/VFX/Library/Survival/Burning/NS_Fire_Small_NPC.NS_Fire_Small_NPC"},
                    {"Socket","None"},{"AutoActivate",true},
                    {"Parameters",{{"User.FireStrength",1.0},{"User.LightEnabled",true}}}};
                const auto slots=static_cast<uint8_t>(std::stoul(request.text));
                const auto count=PlayerGhost::ApplyArmorTest(Pawn(Controller()),effect,slots);
                message=count?"Armor fire + smoke test active on "+std::to_string(count)+" equipped armor mesh(es)."
                    :"Armor test is armed, but no equipped armor meshes were found. Equip armor, then press Start again.";
            } else {
                PlayerGhost::ClearArmorTest();message="Armor fire + smoke test removed.";
            }
            std::lock_guard lock(mutex);status=std::move(message);busy=false;return;
        }
        if(request.action==Action::NiagaraAttach || request.action==Action::NiagaraRemove) {
            std::string message;
            if(request.action!=Action::NiagaraRemove) {
                auto* controller=Controller();
                message=NiagaraTest::Attach(controller,Pawn(controller),NiagaraPreset::Validate(NiagaraPreset::Parse(request.text)));
            } else {NiagaraTest::Reset();message="Niagara test removed.";}
            std::lock_guard lock(mutex);status=std::move(message);busy=false;return;
        }
        if(request.action==Action::StarterCheck) {
            const auto options=json::parse(request.text);
            auto report=StarterPreflight::Check(options.at("Loader"), options.at("Draft"), options.at("Baseline"),
                [](const std::string& path){return Find<UObject*>(to_generic_string(path).c_str())!=nullptr;});
            const auto file=Export(report,"StarterPreflight");
            std::lock_guard lock(mutex);
            snapshot=std::move(report);snapshotTextDirty=true;
            status="Preflight saved to runtime/"+file+". View in Results. This is not runtime validation.";
            busy=false;return;
        }
        if(request.action==Action::AssetSearch || request.action==Action::AssetCapture) {
            const auto options=json::parse(request.text);const auto loader=options.at("Loader").get<std::string>();
            auto data=request.action==Action::AssetSearch?AssetSearch::Search(options.at("Query"),loader):AssetSearch::Capture(options.at("Entry"),loader,options.value("ReadValues",false));
            std::lock_guard lock(mutex);
            if(request.action==Action::AssetSearch) {
                assetMatches=std::move(data);assetSearchQuery=options.at("Query");assetSearchComplete=true;
                status=std::to_string(assetMatches.size())+" matches (limit 100). Inspect a match or export the list.";
            }
            else {assetFields=std::move(data["Values"]);assetHints=std::move(data["Available"]);assetRecord=options.at("Entry");assetSource=assetRecord.at("Key");
                assetChosen.clear();for(const auto& [key,value]:assetHints.items())assetChosen.insert(key);
                for(const auto& [key,value]:assetFields.items())assetChosen.insert(key);status="Select fields, then build a preview.";}
            assetDraft=nullptr;assetPreview.clear();busy=false;return;
        }
        if(request.action==Action::Clear) {
            Reset();
            return;
        }
        if(request.action==Action::SavedItems || request.action==Action::SavedWorld) {
            const int index=request.action==Action::SavedWorld?1:0;
            auto report=SaveViewer::Capture(Controller(),index==1);
            std::lock_guard lock(mutex);
            saveReports[index]=std::move(report);++saveRevision[index];busy=false;
            status="Save report refreshed. No save data was modified.";
            return;
        }
        if(request.action==Action::PlayerTraceStart || request.action==Action::PlayerTraceStop
            || request.action==Action::PlayerTraceMarker) {
            std::string message;
            if(request.action==Action::PlayerTraceStart) {
                const auto options=json::parse(request.text);
                auto targetPath=request.selectedObject;
                if(options.value("TraceSelectedObject",false) && !selectedTargetResolver.is_null()
                    && !selectedTargetResolver.empty()) {
                    const auto resolved=ResolveProfileTarget(selectedTargetResolver);
                    targetPath=resolved;
                }
                auto* target=options.value("TraceSelectedObject",false) && !targetPath.empty()
                    ? Find(to_wstring(targetPath).c_str()) : nullptr;
                if(options.value("TraceSelectedObject",false) && !IsTraceInstance(target)) {
                    std::lock_guard lock(mutex);traceTargetPath.clear();
                    throw std::runtime_error("Trace target is unavailable. Pick a live target in step 2, then start again.");
                }
                UObject* controller=nullptr;
                UObject* player=nullptr;
                if (!options.value("TraceSelectedObject",false)) {
                    controller=Controller();
                    player=Pawn(controller);
                }
                PlayerTrace::Start(player,controller,options,target);
                eventTraceOrigin=captureOrigin;
                message=options.value("TraceSelectedObject",false)
                    ? "Object Trace started. It automatically detaches at its limit; Stop and export saves the recording."
                    : "Player Trace started. It automatically detaches at its limit; Stop and export saves the recording.";
            } else if(request.action==Action::PlayerTraceStop) {
                result=PlayerTrace::Stop();
                result=WithDiagnosticOrigin(std::move(result),eventTraceOrigin);
                try { message="Exported "+Export(result,"PlayerTrace"); }
                catch(const std::exception& error) { message=std::string("Trace stopped; use Export JSON to retry: ")+error.what(); }
            } else if(request.action==Action::PlayerTraceMarker) { PlayerTrace::Marker(request.text);message="Marker added."; }
            std::lock_guard lock(mutex);
            if (request.action==Action::PlayerTraceStop) {
                lastEventTrace={{"events",json::array()}};
                for (const auto& event:result.at("events"))
                    if (event.contains("parameters") && lastEventTrace["events"].size()<16)
                        lastEventTrace["events"].push_back(event);
                ++eventTraceRevision;
            }
            snapshot=std::move(result);status=message;busy=false;return;
        }
        if (request.action == Action::TraceStart || request.action == Action::TraceStop) {
            const bool starting = request.action == Action::TraceStart;
            // Detach before reflection or export can throw.
            if (!starting) result = AppearanceTrace::Stop();
            auto capture = [&]() {
                auto* player = Pawn(Controller());
                if (!player) throw std::runtime_error("No local player is available.");
                json state={{"player",Path(player)},{"captures",json::array()},{"errors",json::array()}};
                CaptureBudget budget;
                budget.maxDepth=8; budget.maxEntries=256; budget.remaining=8192;
                for (const auto& path : std::vector<json>{
                    {"PlayerEquipmentComponent","*"}, {"PlayerCustomizationComponent","*"},
                    {"CPDManager","*"}, {"BodyMesh","OverrideMaterials"},
                    {"HeadMesh","OverrideMaterials"}, {"OutfitBodyMesh","OverrideMaterials"},
                    {"OutfitCapeMesh","OverrideMaterials"}, {"OutfitHeadMesh","OverrideMaterials"},
                    {"OutfitLegsMesh","OverrideMaterials"},
                    {"PlayerEquipmentComponent","HeldEquipmentActorRight","MeshComponent","OverrideMaterials"}}) {
                    try { state["captures"].push_back(CaptureProperty(player,path,budget)); }
                    catch (const std::exception& error) { state["errors"].push_back({{"path",path},{"error",error.what()}}); }
                }
                return state;
            };
            std::string message;
            if (starting) {
                auto* player=Pawn(Controller());
                if (!player) throw std::runtime_error("Enter a world before starting the trace.");
                auto state=capture();
                AppearanceTrace::Start(reinterpret_cast<uintptr_t>(ActorHelper::GetObjectRef(player,TEXT("PlayerEquipmentComponent"))),
                    reinterpret_cast<uintptr_t>(ActorHelper::GetObjectRef(player,TEXT("PlayerCustomizationComponent"))),
                    reinterpret_cast<uintptr_t>(ActorHelper::GetObjectRef(player,TEXT("CPDManager"))));
                appearanceTraceStart=std::move(state);
                message="Appearance trace started. Swap armor, change held items, equip/remove the cloak, then Stop and export. Recording is capped at 60 seconds / 256 events; Stop detaches the recorder.";
            } else {
                result["before"]=std::move(appearanceTraceStart);
                try { result["after"]=capture(); }
                catch (const std::exception& error) { result["afterError"]=error.what(); }
                try { message="Appearance trace detached and exported: "+Export(result,"AppearanceTrace"); }
                catch (const std::exception& error) { message=std::string("Trace detached; export failed, use Export JSON to retry: ")+error.what(); }
            }
            std::lock_guard lock(mutex); snapshot=std::move(result); status=message; busy=false; return;
        }
        if (request.action == Action::Preset) {
            const auto preset = json::parse(request.text); ValidatePreset(preset, extendedCaptureDepth.load());
            result = {{"preset", preset}, {"objects", json::array()}, {"errors", json::array()}};
            if (preset.contains("PropertyCaptures")) {
                result["propertyCaptures"] = json::array();
                CaptureBudget budget;
                const auto limits = preset.value("CaptureLimits", json::object());
                budget.remaining = limits.value("MaxNodes", 2048u);
                budget.maxDepth = limits.value("MaxDepth", DefaultCaptureDepth);
                budget.maxEntries = limits.value("MaxEntries", 64u);
                budget.maxSparseSlots = limits.value("MaxSparseSlots", 4096u);
                budget.followReferences = limits.value("FollowObjectReferences", false);
                result["propertyCaptureLimits"] = {{"MaxNodes", budget.remaining}, {"MaxDepth", budget.maxDepth},
                    {"MaxEntries", budget.maxEntries}, {"MaxSparseSlots", budget.maxSparseSlots},
                    {"FollowObjectReferences", budget.followReferences}};
                UObject* controller = nullptr;
                UObject* player = nullptr;
                for (const auto& target : preset["PropertyCaptures"]) {
                    try {
                        const auto root = target["Root"].get<std::string>();
                        UObject* object = nullptr;
                        if (root == "Player" || root == "Controller") {
                            if (!controller) controller = Controller();
                            if (root == "Controller") object = controller;
                            else { if (!player) player = Pawn(controller); object = player; }
                        } else if (root == "Selected") {
                            if (request.selectedObject.empty()) throw std::runtime_error("Inspect a target or select an object before running this preset");
                            object = Find(to_wstring(request.selectedObject).c_str());
                        } else object = ActorHelper::ResolveObject(to_wstring(root));
                        result["propertyCaptures"].push_back(CaptureProperty(object, target["Path"], budget));
                    } catch (const std::exception& error) {
                        if(target.value("Optional",false))result["skippedOptionalCaptures"].push_back({{"target",target},{"reason",error.what()}});
                        else result["errors"].push_back({{"target", target}, {"error", error.what()}});
                    }
                }
                result["propertyCaptureNodesRemaining"] = budget.remaining;
            }
            auto capture = [&](const std::string& target, auto resolve) {
                try {
                    auto* object = resolve();
                    if (!object) throw std::runtime_error("Target unavailable.");
                    if (object->IsA(UClass::StaticClass()) || object->IsA(UScriptStruct::StaticClass()) || object->IsA(UFunction::StaticClass())) {
                        auto* type = static_cast<UStruct*>(object);
                        result["objects"].push_back(TypeMetadata(type));
                    } else result["objects"].push_back(ResearchObject(object));
                } catch (const std::exception& error) { result["errors"].push_back({{"target", target}, {"error", error.what()}}); }
            };
            for (const auto& entry : preset.value("Objects", json::array())) {
                const auto path = entry.get<std::string>();
                capture(path, [&] { return ActorHelper::ResolveObject(to_wstring(path)); });
            }
            const bool needsPlayer = preset.value("IncludePlayer", false) || preset.value("IncludeControllerComponents", false)
                || !preset.value("ControllerProperties", json::array()).empty();
            if (needsPlayer) {
                try {
                    auto* controller = Controller();
                    for (const auto& entry : preset.value("ControllerProperties", json::array())) {
                        const auto name = entry.get<std::string>();
                        capture(name, [&] {
                            auto* property = CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(controller->GetClassPrivate(), to_wstring(name)));
                            if (!property) throw std::runtime_error("Controller property is not an object reference.");
                            return property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(controller));
                        });
                    }
                    if (preset.value("IncludePlayer", false)) result["player"] = Capture(Pawn(controller));
                    if (preset.value("IncludeControllerComponents", false)) {
                        result["controller"] = Capture(controller);
                        auto* componentClass = Find<UClass*>(TEXT("/Script/Engine.ActorComponent"));
                        if (!componentClass) throw std::runtime_error("ActorComponent class unavailable.");
                        ActorHelper::FunctionCall call(controller, TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
                        call.Arg(TEXT("ComponentClass"), componentClass).Invoke();
                        TArray<UObject*> components; call.MoveResult(&components, sizeof(components));
                        int count = 0;
                        for (auto* component : components) if (component && count++ < 128)
                            capture(Path(component), [&] { return component; });
                        result["controllerComponentsTruncated"] = count > 128;
                    }
                } catch (const std::exception& error) { result["errors"].push_back({{"target", "local player"}, {"error", error.what()}}); }
            }
            result=WithDiagnosticOrigin(std::move(result),captureOrigin);
            const auto file = Export(result, "Preset-" + preset.at("Name").get<std::string>());
            const auto message = "Saved runtime/" + file + " (" + std::to_string(result["errors"].size()) + " errors).";
            std::lock_guard lock(mutex); snapshot = std::move(result); status = message; busy = false; return;
        }
        if(request.action==Action::TraceSelect) {
            auto* object=Find(to_wstring(request.text).c_str());
            if(!IsTraceInstance(object))throw std::runtime_error("Target unloaded or is a class/template. Search again for a live instance.");
            std::lock_guard lock(mutex);
            traceTargetPath=Path(object);selectedTargetResolver=nullptr;
            status="Trace target selected. Return to step 3 to start recording.";busy=false;return;
        }
        if (request.action == Action::Browse || request.action == Action::TraceBrowse) {
            std::vector<std::string> found;
            const auto limit = static_cast<size_t>(std::clamp(request.objectLimit, 1, 500));
            const auto needle = Lower(request.text);
            UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                if (object && found.size() < limit && (request.action!=Action::TraceBrowse || IsTraceInstance(object))) {
                    auto path = Path(object);
                    if (Lower(path).find(needle) != std::string::npos) found.push_back(std::move(path));
                }
                return found.size()>=limit ? LoopAction::Break : LoopAction::Continue;
            });
            std::sort(found.begin(), found.end());
            std::lock_guard lock(mutex);
            if(request.action==Action::TraceBrowse) {
                traceTargetMatches=std::move(found);
                status=std::to_string(traceTargetMatches.size())+" instance matches. Choose the world actor or widget, not a data asset; narrow the search if capped.";
                busy=false;return;
            }
            matches = std::move(found);
            objectSearchQuery=request.text;objectSearchComplete=true;
            completedObjectResultLimit = static_cast<int>(limit);
            status = std::to_string(matches.size()) + " loaded matches (limit " + std::to_string(limit)
                + "). Select one to inspect; narrow the search if capped.";
            busy = false; return;
        }
        if (request.action == Action::Object) {
            auto wide = to_wstring(request.text);
            auto* object = Find(wide.c_str());
            if (!object) throw std::runtime_error("Object unloaded since search. Search again.");
            if(IsTraceInstance(object)) {
                std::lock_guard lock(mutex);traceTargetPath=Path(object);selectedTargetResolver=nullptr;
            }
            if (object->IsA(UClass::StaticClass()) || object->IsA(UScriptStruct::StaticClass()) || object->IsA(UFunction::StaticClass())) {
                auto* type = static_cast<UStruct*>(object);
                result = TypeMetadata(type);
                result["hierarchy"] = json::array();
                for (auto* parent = type->GetSuperStruct(); parent; parent = parent->GetSuperStruct())
                    result["hierarchy"].push_back({{"type", Path(parent)}, {"fields", Fields(parent)}});
            } else result = Capture(object);
        } else {
            auto* controller = Controller();
            auto* pawn = Pawn(controller);
            if (!pawn) throw std::runtime_error("Local pawn unavailable. Finish entering the world.");
            if (request.action == Action::Player) {
                result = Capture(pawn);
                result["controller"] = Describe(controller);
            } else {
                auto* camera = ActorHelper::GetObjectRef(controller, TEXT("PlayerCameraManager"));
                if (!camera) throw std::runtime_error("Local camera unavailable.");
                ActorHelper::FunctionCall location(camera, TEXT("/Script/Engine.PlayerCameraManager:GetCameraLocation"));
                ActorHelper::FunctionCall rotation(camera, TEXT("/Script/Engine.PlayerCameraManager:GetCameraRotation"));
                location.Invoke(); rotation.Invoke();
                const auto start = location.Result<FVector>();
                const auto rot = rotation.Result<FRotator>();
                constexpr double radians = 3.14159265358979323846 / 180;
                const double pitch = rot.GetPitch() * radians, yaw = rot.GetYaw() * radians;
                FVector end(start.X() + request.range * std::cos(pitch) * std::cos(yaw),
                    start.Y() + request.range * std::cos(pitch) * std::sin(yaw),
                    start.Z() + request.range * std::sin(pitch));
                FVector impact{}; std::string error; UObject* component = nullptr;
                if (!UECustom::UKismetSystemLibrary::LineTraceGround(pawn, start, end,
                        {static_cast<AActor*>(pawn)}, impact, error, &component))
                    throw std::runtime_error(error.empty() ? "No blocking visibility hit. Aim at a closer solid surface and capture again." : error);
                if (!component) throw std::runtime_error("Hit had no inspectable component.");
                ActorHelper::FunctionCall owner(component, TEXT("/Script/Engine.ActorComponent:GetOwner"));
                owner.Invoke();
                auto* hitActor=owner.Result<UObject*>();
                if(!IsTraceInstance(hitActor))throw std::runtime_error("Hit has no live actor. Aim at the object and try again.");
                if(request.action==Action::TraceTarget) {
                    std::lock_guard lock(mutex);traceTargetPath=Path(hitActor);selectedTargetResolver=nullptr;
                    status="Target selected from the crosshair. Check its path below before recording.";busy=false;return;
                }
                result = Capture(hitActor);
                {std::lock_guard lock(mutex);traceTargetPath=Path(hitActor);selectedTargetResolver=nullptr;}
                result["hit"] = {{"component", Path(component)}, {"impact", {impact.X(), impact.Y(), impact.Z()}}, {"rangeCm", request.range}};
            }
        }
        result=WithDiagnosticOrigin(std::move(result),captureOrigin);
        std::lock_guard lock(mutex); snapshot = std::move(result); busy = false;
        status = "Snapshot captured. Open Results to filter fields, copy paths, or export JSON.";
    } catch (const std::exception& error) {
        std::lock_guard lock(mutex); busy = false; snapshot = nullptr; status = error.what();
    }
}
void Render(bool available, Section section, const char* loader) {
    std::lock_guard lock(mutex);
    RememberStatus(section);
    if (hasRequest && pending.action != Action::None
        && std::chrono::steady_clock::now() < pending.due) {
        const auto remaining = std::chrono::duration<double>(pending.due - std::chrono::steady_clock::now()).count();
        ImGui::Text("Capture in %.1f seconds", std::max(0.0, remaining));
        ImGui::SameLine();
        if (ImGui::Button("Cancel pending capture")) {
            pending = {}; hasRequest = false; busy = false;
            status = "Pending capture cancelled.";
        }
    }
    // Keep the shared export controls scoped to the visible tool tab. A profile or
    // an explicit user name can still override this default, but switching tabs
    // never silently reuses a name from a different tool family.
    static Section lastSection = Section::Inspector;
    static std::string lastLoader;
    static std::string lastDefault;
    const std::string tabKey = loader ? std::string(loader) : std::string{};
    if (section != lastSection || tabKey != lastLoader) {
        std::string currentDefault;
        switch (section) {
            case Section::Inspector: currentDefault = "Inspector"; break;
            case Section::Presets: currentDefault = "Preset"; break;
            case Section::Traces: currentDefault = "PlayerTrace"; break;
            case Section::Results: currentDefault = "Results"; break;
            case Section::SavedItems: currentDefault = "SavedItems"; break;
            case Section::SavedWorld: currentDefault = "WorldRegistry"; break;
            case Section::AssetTemplates: currentDefault = tabKey.empty() ? "Starter" : tabKey + "-Starter"; break;
            case Section::Niagara: currentDefault = "Niagara"; break;
        }
        std::lock_guard settingsLock(exportMutex);
        if (exportName[0] == 0 || std::string(exportName) == lastDefault)
            std::snprintf(exportName, sizeof(exportName), "%s", currentDefault.c_str());
        lastDefault = currentDefault;
        lastSection = section;
        lastLoader = tabKey;
    }
    if(ImGui::CollapsingHeader("Session cleanup")) {
        ImGui::TextWrapped("Discards unexported recordings and cached reports; keeps settings and exported files.");
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Stop traces and clear results"))Queue(Action::Clear);
        ImGui::EndDisabled();
    }
    if(ImGui::CollapsingHeader("Export options")) {
    { std::lock_guard settingsLock(exportMutex);
      ImGui::InputText("Export name (optional, no extension)", exportName, sizeof(exportName));
      ImGui::Checkbox("Export reports as JSONC (otherwise JSON)", &exportJsonc);
      if(section==Section::AssetTemplates)ImGui::TextUnformatted("Mod starters always export as JSONC with comments."); }
    ImGui::TextWrapped("Names use name_loader_timestamp. Reports: runtime/live/jobs/exports. Existing files are kept.");
    }
    if(section==Section::Niagara) {
        static char draft[16385]="";
        static std::vector<std::string> files;
        static std::string builtInSelection="Full fire",sharedSelection;
        static bool draftEdited=false;
        static ImGuiTextFilter builtInPresetFilter,sharedPresetFilter;
        static double locationOffset[3]{};
        static double rotationOffset[3]{};
        const auto diagnostics=PS::HostServices::SearchesDirectory();
        const auto folder=diagnostics/"presets/loaders/niagara";
        auto edit=[&](const json& preset) {
            const auto validated=NiagaraPreset::Validate(preset);
            const auto& location=validated["LocationOffset"];
            const auto& rotation=validated["RotationOffset"];
            locationOffset[0]=location["X"];locationOffset[1]=location["Y"];locationOffset[2]=location["Z"];
            rotationOffset[0]=rotation["Pitch"];rotationOffset[1]=rotation["Yaw"];rotationOffset[2]=rotation["Roll"];
            const auto text=validated.dump(2);
            if(text.size()>=sizeof(draft))throw std::runtime_error("Preset exceeds editor capacity.");
            std::snprintf(draft,sizeof(draft),"%s",text.c_str());
        };
        if(!draft[0])edit(NiagaraPreset::Builtin(0));
        ImGui::TextWrapped("Manual local-player experiments, not a /niagara mod loader. Presets never run automatically. Each Attach replaces the current test component.");
        ImGui::BeginDisabled(busy);
        builtInPresetFilter.Draw("Filter built-in presets",-1.0f);
        if(ImGui::BeginCombo("Built-in preset",builtInSelection.empty()?"Choose a test":builtInSelection.c_str())) {
            bool matched=false;
            for(size_t i=0;i<4;++i) {
                auto preset=NiagaraPreset::Builtin(i);
                const auto name=preset["Name"].get<std::string>();
                if(!builtInPresetFilter.PassFilter(name.c_str()))continue;
                matched=true;
                if(ImGui::Selectable(name.c_str(),builtInSelection==name)) {
                    edit(preset);builtInSelection=name;sharedSelection.clear();draftEdited=false;
                    status="Preset selected; press Attach preset to run. Any current effect is unchanged.";
                }
            }
            if(!matched)ImGui::TextDisabled("No matching built-in presets");
            ImGui::EndCombo();
        }
        if(ImGui::Button("Refresh shared presets")) {
            files.clear();
            try {
                size_t checked=0;
                for(const auto& scanFolder:{folder})
                if(std::filesystem::exists(scanFolder))for(const auto& entry:std::filesystem::directory_iterator(scanFolder)) {
                    if(++checked>1024 || files.size()>=256)break;
                    if(entry.is_regular_file() && (entry.path().extension()==".json" || entry.path().extension()==".jsonc")
                        && entry.file_size()<=16384)files.push_back(entry.path().lexically_relative(diagnostics).generic_string());
                }
                std::sort(files.begin(),files.end());status="Shared preset list refreshed (256 files / 1024 directory entries maximum).";
            } catch(const std::exception& error){status=error.what();}
        }
        sharedPresetFilter.Draw("Filter shared presets",-1.0f);
        if(ImGui::BeginCombo("Shared preset",sharedSelection.empty()?"Choose a file":sharedSelection.c_str())) {
            bool matched=false;
            for(const auto& file:files) {
                if(!sharedPresetFilter.PassFilter(file.c_str()))continue;
                matched=true;
                if(ImGui::Selectable(file.c_str(),sharedSelection==file)) {
                try {edit(NiagaraPreset::Validate(NiagaraPreset::Parse(ConfigFiles::Read(diagnostics/file,16384))));sharedSelection=file;builtInSelection.clear();draftEdited=false;status="Preset loaded for review; press Attach to run.";}
                catch(const std::exception& error){status=error.what();}
            }
            }
            if(!matched)ImGui::TextDisabled("No matching shared presets");
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Presets: runtime/live/jobs/searches/presets/loaders/niagara. Attachment reports: runtime/live/jobs/exports. Loading a preset does not attach anything.");
        ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("Attachment transform",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("Relative to the selected component or socket. These move the whole Niagara system; individual emitters require exposed User.* parameters.");
            bool changed=false;
            changed|=ImGui::InputDouble("Location X",&locationOffset[0],1.0,10.0,"%.2f");
            changed|=ImGui::InputDouble("Location Y",&locationOffset[1],1.0,10.0,"%.2f");
            changed|=ImGui::InputDouble("Location Z",&locationOffset[2],1.0,10.0,"%.2f");
            changed|=ImGui::InputDouble("Rotation Pitch",&rotationOffset[0],1.0,10.0,"%.2f");
            changed|=ImGui::InputDouble("Rotation Yaw",&rotationOffset[1],1.0,10.0,"%.2f");
            changed|=ImGui::InputDouble("Rotation Roll",&rotationOffset[2],1.0,10.0,"%.2f");
            if(changed)try {
                auto preset=NiagaraPreset::Parse(draft);
                preset["LocationOffset"]={{"X",locationOffset[0]},{"Y",locationOffset[1]},{"Z",locationOffset[2]}};
                preset["RotationOffset"]={{"Pitch",rotationOffset[0]},{"Yaw",rotationOffset[1]},{"Roll",rotationOffset[2]}};
                edit(preset);draftEdited=true;status="Attachment transform updated; press Attach preset to apply.";
            } catch(const std::exception& error){status=error.what();}
        }
        ImGui::SeparatorText("Test and capture");
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Attach preset")) {
            try {Queue(Action::NiagaraAttach,NiagaraPreset::Validate(NiagaraPreset::Parse(draft)).dump());}
            catch(const std::exception& error){status=error.what();}
        }
        if(ImGui::Button("Capture attachment and export"))Queue(Action::NiagaraInspect);
        if(ImGui::Button("Remove test"))Queue(Action::NiagaraRemove);
        ImGui::EndDisabled();
        ImGui::SeparatorText("Player armor test mode");
        ImGui::TextWrapped("Applies the cooked full-fire system (fire and smoke) to each currently equipped head, body, legs and cape mesh. This session-only override does not edit items, save data or shared Niagara assets.");
        static int armorTarget=0;
        constexpr const char* armorTargets[]{"All equipped armor","Head","Body","Legs","Cape"};
        constexpr uint8_t armorMasks[]{0x0f,0x01,0x02,0x04,0x08};
        if(ImGui::BeginCombo("Armor target",armorTargets[armorTarget])) {
            for(int i=0;i<static_cast<int>(std::size(armorTargets));++i)
                if(ImGui::Selectable(armorTargets[i],armorTarget==i))armorTarget=i;
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Start armor fire + smoke"))
            Queue(Action::NiagaraArmorStart,std::to_string(armorMasks[armorTarget]));
        if(ImGui::Button("Stop armor fire + smoke"))Queue(Action::NiagaraArmorStop);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",status.c_str());
        ImGui::BeginDisabled(busy);
        if(ImGui::CollapsingHeader("Edit preset JSON / JSONC"))
            if(ImGui::InputTextMultiline("##NiagaraPreset",draft,sizeof(draft),ImVec2(-1,180)))draftEdited=true;
        if(draftEdited)ImGui::TextWrapped("Edited draft: dropdowns identify its source, not an exact match. Attach uses the edited JSON.");
        if(ImGui::Button("Export preset")) {
            try {auto preset=NiagaraPreset::Validate(NiagaraPreset::Parse(draft));status="Exported runtime/"+Export(preset,"NiagaraPreset",false,false,0,true);}
            catch(const std::exception& error){status=error.what();}
        }
        ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("Attachment help and limits"))
            ImGui::TextWrapped("Target: PlayerRoot or PlayerMesh. Named sockets require PlayerMesh. LocationOffset and RotationOffset are relative attachment transforms. Emitters omitted from the map retain defaults. Switches are requests, not confirmed isolation; old particles may take time to fade. Unsupported layouts stop the call. Cooked effects can still crash: use a disposable world.");
        ImGui::TextWrapped("Capture once, move, then capture again without reattaching. Reports use the active preset, not unsaved editor changes. Persistent effects for exact armor or held weapons belong in that item's /equipment JSON; /players affects the whole character; /spawns can target ActorRoot or ActorMesh.");
        ImGui::TextWrapped("%s",status.c_str());return;
    }
    if(section==Section::AssetTemplates) {
        static char query[97]="",mod[65]="MyMod",name[65]="MyItem",identity[23]="";
        static int mode=3,loaderIndex=0;
        ImGui::BeginDisabled(busy);
        bool loaderChanged=false;
        if(loader && assetLoader!=loader) {
            if(busy){ImGui::EndDisabled();ImGui::TextWrapped("Waiting for the current authoring request to finish.");return;}
            for(size_t i=0;i<LoaderCapabilities.size();++i)if(std::string_view(loader)==LoaderCapabilities[i].Name){loaderIndex=static_cast<int>(i);loaderChanged=true;break;}
        }
        if(!loader && ImGui::BeginCombo("Loader",LoaderCapabilities[loaderIndex].Name)) {
            for(size_t i=0;i<LoaderCapabilities.size();++i)
                if(ImGui::Selectable(LoaderCapabilities[i].Name,loaderIndex==static_cast<int>(i))) {loaderIndex=static_cast<int>(i);loaderChanged=true;}
            ImGui::EndCombo();
        }
        if(loaderChanged) {
            assetSearchComplete=false;
            assetLoader=LoaderCapabilities[loaderIndex].Name;assetMatches=json::array();assetFields=json::object();assetHints=json::object();assetRecord=nullptr;
            assetSource.clear();assetDraft=nullptr;assetPreview.clear();mode=3;
        }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Search configuration");
        ImGui::TextWrapped("%s",AuthoredStarters::Supports(assetLoader)?"Search authored files in installed mod folders, including inactive mods. Top-level JSON/JSONC files only.":"Search loaded names or paths. /raw includes row names; /strings includes source text. No objects are loaded or changed.");
        ImGui::InputText("Search text",query,sizeof(query));
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Search")) {assetMatches=json::array();assetFields=json::object();assetHints=json::object();assetSource.clear();assetDraft=nullptr;assetPreview.clear();
            assetSearchComplete=false;
            Queue(Action::AssetSearch,json{{"Loader",assetLoader},{"Query",query}}.dump());}
        ImGui::EndDisabled();
        ImGui::BeginDisabled(busy || !assetSearchComplete);
        if(ImGui::Button("Export search results")) {
            try {status="Exported runtime/"+Export(SearchReport(assetSearchQuery,
                AuthoredStarters::Supports(assetLoader)?"Installed authored files, including inactive mods":"Loaded game records",
                assetLoader,assetMatches,100),"LoaderSearch");}
            catch(const std::exception& error){status=error.what();}
        }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Search results");
        ImGui::TextWrapped("Search lists are research reports. Select a match below to build a supported JSONC starter.");
        ImGui::BeginDisabled(!available || busy);
        ImGui::BeginChild("Item matches",ImVec2(0,160),true);
        for(const auto& row:assetMatches) {
            const auto path=row["Key"].get<std::string>();
            const auto label=row["Name"].get<std::string>()+" | "+path;
            ImGui::PushID(path.c_str());
            if(ImGui::Selectable(label.c_str(),assetSource==path)) {assetSource.clear();assetFields=json::object();assetHints=json::object();assetDraft=nullptr;assetPreview.clear();
                Queue(Action::AssetCapture,json{{"Loader",assetLoader},{"Entry",row},{"ReadValues",mode!=3}}.dump());}
            ImGui::PopID();
        }
        ImGui::EndChild();ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("Structural schema")) {
        if(ImGui::Button("Export this loader's structural schema")) {
            try{status="Exported runtime/"+Export(JsonSchemaGenerator::LoaderSchemas().at(assetLoader),"Schema-"+assetLoader);}
            catch(const std::exception& error){status=error.what();}
        }
        }
        ImGui::SeparatorText("Starter configuration");
        bool changed=false;
        constexpr const char* formats[]{"Basic starter","$Patch","$Clone","Fields reference"};
        ImGui::BeginDisabled(busy);
        if(ImGui::BeginCombo("Format",formats[mode])) {
            for(int i=0;i<4;++i) {
                ImGui::BeginDisabled((i==1&&!LoaderTemplate::Patch(assetLoader))||(i==2&&!LoaderTemplate::Clone(assetLoader)));
                if(ImGui::Selectable(formats[i],mode==i)) {mode=i;changed=true;
                    if(!assetSource.empty() && !assetRecord.is_null())Queue(Action::AssetCapture,json{{"Loader",assetLoader},{"Entry",assetRecord},{"ReadValues",mode!=3}}.dump());}
                ImGui::EndDisabled();
            }ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if(mode!=3 && (mode==2 || assetLoader=="buildings" || assetLoader=="players")) {
            changed|=ImGui::InputText("Mod folder",mod,sizeof(mod));
            changed|=ImGui::InputText("New item name / rule key",name,sizeof(name));
        }
        if(mode==2 && assetLoader=="assets") {
            changed|=ImGui::InputText("PersistenceID",identity,sizeof(identity));
            if(ImGui::Button("Generate new ID")) {
                try {
                    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
                    std::random_device random;
                    for(int i=0;i<21;++i)identity[i]=alphabet[random()&63];
                    identity[21]=alphabet[(random()&3)<<4];identity[22]=0;changed=true;
                }catch(const std::exception& error){status=error.what();}
            }
            ImGui::TextWrapped("Use your actual mod folder name. Keep the generated ID unchanged after installation. A clone inherits omitted fields and behavior.");
        }
        if(!assetSource.empty())ImGui::TextWrapped("Source: %s",assetSource.c_str());
        ImGui::BeginDisabled(busy || assetRecord.is_null());
        if(ImGui::Button("Export editable reference JSONC"))
            Queue(Action::AssetReference,json{{"Loader",assetLoader},{"Entry",assetRecord}}.dump());
        ImGui::EndDisabled();
        ImGui::TextWrapped("Reference: available fields, captured values and full paths. Unknowns are null; inspectable does not mean editable. Uses this loader's current loaded/authored search scope, not live inventory. Does not install anything.");
        const auto& choices=mode==3?assetHints:assetFields;
        if(ImGui::CollapsingHeader("Select fields",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("StarterFields",ImVec2(0,160),true);
        for(const auto& [key,value]:choices.items()) {
            bool selected=assetChosen.contains(key);
            if(ImGui::Checkbox(key.c_str(),&selected)) {changed=true;if(selected)assetChosen.insert(key);else assetChosen.erase(key);}
        }
        ImGui::EndChild();
        }
        if(changed) {assetDraft=nullptr;assetPreview.clear();}
        ImGui::TextWrapped("%s",mode==3?"Field reference only; not an installable mod. Maximum 256 fields, with unsupported JSON types left open.":LoaderTemplate::Help(assetLoader).c_str());
        ImGui::BeginDisabled(busy || assetSource.empty());
        if(ImGui::Button("Build preview")) {
            try {
                json fields=json::object();
                for(const auto& [key,value]:choices.items())if(assetChosen.contains(key))fields[key]=value;
                assetDraft=LoaderTemplate::Build(assetLoader,assetRecord,std::move(fields),mode,mod,name,identity);
                const auto editable=assetDraft.dump(2);
                if(editable.size()>=sizeof(starterEditor))throw std::runtime_error("Starter exceeds editor capacity (64 KiB)");
                std::snprintf(starterEditor,sizeof(starterEditor),"%s",editable.c_str());
                starterBaseline=assetDraft;
                assetPreview=LoaderTemplate::Jsonc(assetLoader,mode,assetDraft);status="Preview ready. Review before installing.";
            }catch(const std::exception& error){status=error.what();}
        }
        ImGui::EndDisabled();
        if(!assetPreview.empty() && mode!=3 && ImGui::CollapsingHeader("Edit draft and check dependencies")) {
            ImGui::TextWrapped("Edit the draft, then check changes and loaded references. Comparison uses the captured starter, not fresh game values. No game writes or package loads.");
            ImGui::BeginDisabled(busy);
            if(ImGui::InputTextMultiline("Draft JSON / JSONC",starterEditor,sizeof(starterEditor),ImVec2(-1,180))) {
                try{assetDraft=StarterPreflight::Parse(starterEditor);assetPreview=LoaderTemplate::Jsonc(assetLoader,mode,assetDraft);status="Draft changed; run preflight again.";}
                catch(const std::exception& error){assetDraft=nullptr;status=error.what();}
            }
            ImGui::BeginDisabled(assetDraft.is_null());
            if(ImGui::Button("Check changes and dependencies"))
                Queue(Action::StarterCheck,json{{"Loader",assetLoader},{"Draft",assetDraft},{"Baseline",starterBaseline}}.dump());
            ImGui::EndDisabled();ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(assetDraft.is_null() || busy);
        if(ImGui::Button("Export JSONC")) {
            try {status="Exported runtime/"+Export(assetDraft,"Starter",false,true,mode);}
            catch(const std::exception& error){status=error.what();}
        }
        ImGui::EndDisabled();
        if(!assetPreview.empty() && ImGui::CollapsingHeader("Preview JSONC",ImGuiTreeNodeFlags_DefaultOpen)) {ImGui::BeginChild("Asset preview",ImVec2(0,180),true,ImGuiWindowFlags_HorizontalScrollbar);ImGui::TextUnformatted(assetPreview.c_str());ImGui::EndChild();}
        ImGui::TextWrapped("%s",status.c_str());return;
    }
    if(section==Section::SavedItems || section==Section::SavedWorld) {
        const int index=section==Section::SavedWorld?1:0;
        ImGui::SeparatorText("Capture and export");
        ImGui::TextWrapped(index?"Building registration history only; not a placed-object or container scan.":
            "Last saved inventory of the active character, matched by GUID. Names resolve from loaded assets only; pak ownership is not guaranteed.");
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Refresh save report"))Queue(index?Action::SavedWorld:Action::SavedItems);
        ImGui::EndDisabled();
        auto& report=saveReports[index];
        ImGui::BeginDisabled(report.is_null());
        if(ImGui::Button("Export save report")) {
            try {status="Exported runtime/"+Export(report,index?"WorldRegistry":"SavedItems");}
            catch(const std::exception& error){status=error.what();}
        }
        ImGui::EndDisabled();
        if(!report.is_null()) {
            ImGui::TextWrapped("%s",report.value("Coverage",std::string{}).c_str());
            if(report.contains("SaveFile"))ImGui::TextWrapped("Save: %s",report["SaveFile"].get_ref<const std::string&>().c_str());
            for(const auto& warning:report["Warnings"])ImGui::TextWrapped("%s",warning.get_ref<const std::string&>().c_str());
            static bool includeOther[2]{};
            static unsigned displayedRevision[2]{~0u,~0u};
            static std::vector<size_t> visible[2];
            const bool changed=!index && ImGui::Checkbox("Include other loaded assets",&includeOther[index]);
            if(changed || displayedRevision[index]!=saveRevision[index]) {
                visible[index].clear();
                for(size_t i=0;i<report["Rows"].size();++i) {
                    const auto origin=report["Rows"][i].value("Origin",std::string{});
                    if(index || includeOther[index] || !origin.starts_with("Other loaded"))visible[index].push_back(i);
                }
                displayedRevision[index]=saveRevision[index];
            }
            ImGui::Text("Showing %zu of %zu entries. Export includes all entries and origin labels.",visible[index].size(),report["Rows"].size());
            if(ImGui::BeginTable("Saved identities",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,ImVec2(0,300))) {
                for(const auto* label:{"Display name","Internal name","PersistenceID",index?"Owner / state":"Location","Origin"})ImGui::TableSetupColumn(label);
                ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
                ImGuiListClipper clipper;clipper.Begin(static_cast<int>(visible[index].size()));
                while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                    const auto& row=report["Rows"][visible[index][i]];
                    ImGui::TableNextRow();
                    int column=0;
                    for(const auto* key:{"DisplayName","InternalName","PersistenceID",index?"Owner":"Location","Origin"}) {
                        ImGui::TableSetColumnIndex(column++);
                        const auto it=row.find(key);
                        std::string value=it==row.end() || it->is_null()?"Unresolved":it->is_string()?it->get<std::string>():it->dump();
                        if(index && std::string_view(key)=="Owner" && row.contains("State"))value+=" / "+row["State"].dump();
                        ImGui::TextUnformatted(value.c_str());
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",value.c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::TextWrapped("%s",status.c_str());
        return;
    }
    if (section == Section::Presets) {
        static bool openPresetEditor=false;
        ImGui::SeparatorText("Saved diagnostic presets");
        if(ImGui::CollapsingHeader("Capture limits and help")) {
        ImGui::BeginDisabled(busy);
        bool extended = extendedCaptureDepth.load();
        if (ImGui::Checkbox("Allow deeper captures this session (up to 16)", &extended)) {
            extendedCaptureDepth.store(extended);
            presetsLoaded = false;
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Depth: default 7, standard maximum 10. Extended captures can stall the game.");
        RenderDiagnosticGuide("Presets");
        }
        try { if (!presetsLoaded) ReloadPresets(); }
        catch (const std::exception& error) { status = error.what(); }
        RenderRejectedDiagnostics("Preset",presetErrors,PresetFolder());
        ImGui::TextWrapped("Run captures the selected scope into a timestamped report. Review errors and omissions before drawing conclusions.");
        const auto selectedName = selectedPreset >= 0 && selectedPreset < static_cast<int>(presets.size())
            ? presets[selectedPreset].at("Name").get<std::string>() : std::string("Select preset");
        presetFilter.Draw("Filter presets",-1.0f);
        if (ImGui::BeginCombo("Preset", selectedName.c_str())) {
            bool matched=false;
            for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
                const auto searchable=presets[i].at("Name").get<std::string>()+" "+presets[i].value("Description",std::string{});
                if(!presetFilter.PassFilter(searchable.c_str()))continue;
                matched=true;
                ImGui::PushID(i);
                if (ImGui::Selectable(presets[i].at("Name").get_ref<const std::string&>().c_str(), selectedPreset == i)) selectedPreset = i;
                ImGui::PopID();
            }
            if(!matched)ImGui::TextDisabled("No matching presets");
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("Reload presets")) {
            try { ReloadPresets(); } catch (const std::exception& error) { status = error.what(); }
        }
        ImGui::BeginDisabled(selectedPreset < 0);

        if (ImGui::Button("Edit selected")) {
            openPresetEditor=true;
            const auto text = presets[selectedPreset].dump(2);
            if (text.size() < sizeof(presetEditor)) std::snprintf(presetEditor, sizeof(presetEditor), "%s", text.c_str());
            else status = "Preset is too large for the editor.";
        }
        ImGui::BeginDisabled(!available);
         if (ImGui::Button("Run selected preset")) Queue(Action::Preset, presets[selectedPreset].dump());
        ImGui::EndDisabled(); ImGui::EndDisabled();
        if(openPresetEditor){ImGui::SetNextItemOpen(true);openPresetEditor=false;}
        if(ImGui::CollapsingHeader("Edit and save preset")) {
        ImGui::InputTextMultiline("Preset JSON", presetEditor, sizeof(presetEditor), ImVec2(-1, 180));
        if (ImGui::Button("Save preset")) {
            try {
                const auto preset = json::parse(presetEditor,nullptr,true,true); ValidatePreset(preset, extendedCaptureDepth.load());
                std::filesystem::create_directories(PresetFolder());
                const auto name = preset.at("Name").get<std::string>();
                const auto fileName = "user-" + name + ".json";
                std::ofstream file(PresetFolder() / fileName); file << preset.dump(2); file.flush();
                if (!file) throw std::runtime_error("Could not save preset.");
                file.close(); ReloadPresets(); status = "Saved presets/" + fileName + ". Saving the same Name replaces that saved preset.";
            } catch (const std::exception& error) { status = error.what(); }
        }
        }
        ImGui::EndDisabled();
    }
    if (section == Section::Traces) {
    ImGui::SeparatorText("1. Choose and load a profile");
    RenderTraceProfiles();
    ImGui::SeparatorText("2. Choose the recording target");
    ImGui::Checkbox("Trace inspected object instead of player (NPC/resource/VFX)", &traceControls.selectedObject);
    if(traceControls.selectedObject) {
        ImGui::TextWrapped("Target: %s",traceTargetPath.empty()?"None selected":traceTargetPath.c_str());
        ImGui::TextWrapped("Select the book/NPC in this tab. A crosshair pick hits the first solid surface; verify the path is the object you intended. Target is checked again when recording starts.");
        ImGui::BeginDisabled(!available || busy);
        if(ImGui::Button("Pick crosshair target in 3 seconds"))Queue(Action::TraceTarget);
        ImGui::TextWrapped("Return to the game and aim at the book before the three seconds expire. Then reopen this tab.");
        ImGui::SliderFloat("Target pick distance (meters)",&rangeMeters,1,500,"%.0f");
        if(ImGui::Button("Clear trace target")){traceTargetPath.clear();selectedTargetResolver=nullptr;}
        static char targetSearch[256]="";
        static int targetLimit=100;
        ImGui::InputText("Instance path contains",targetSearch,sizeof(targetSearch));
        ImGui::SliderInt("Target search result limit",&targetLimit,1,500);
        if(ImGui::Button("Search target instances") && targetSearch[0]) {
            traceTargetMatches.clear();Queue(Action::TraceBrowse,targetSearch);pending.objectLimit=targetLimit;
        }
        if(!traceTargetMatches.empty()) {
            ImGui::BeginChild("TraceTargetResults",ImVec2(0,150),true);
            for(const auto& path:traceTargetMatches)
                if(ImGui::Selectable(path.c_str(),traceTargetPath==path))Queue(Action::TraceSelect,path);
            ImGui::EndChild();
        }
        ImGui::EndDisabled();
    } else ImGui::TextWrapped("Target: local player, controller and owned subobjects.");
    ImGui::SeparatorText("Event Trace (player or selected object)");
    ImGui::TextWrapped("Records reflected player/controller calls and owned subobjects. Direct native and external world-object calls can be absent. Categories are inferred from names. Limits stop and detach automatically; export afterward. Broad tracing can reduce performance.");
    static char marker[129]="";
    ImGui::TextWrapped("Filter: %s | Limit: %d seconds / %d events",traceControls.filter[0]?traceControls.filter:"All names in selected categories",traceControls.seconds,traceControls.maxEvents);
    if(traceControls.includeTerms[0] || traceControls.excludeTerms[0])ImGui::TextWrapped("Additional include/exclude terms are active; see trace options below.");
    if(ImGui::CollapsingHeader("Trace setup",ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& categories=traceControls.categories;
    constexpr const char* labels[]{"Equipment / appearance","Combat / magic","Interactions / inventory / crafting","Movement","Other reflected calls"};
    for(unsigned i=0;i<5;++i) ImGui::Checkbox(labels[i],&categories[i]);
    auto& seconds=traceControls.seconds;auto& maxEvents=traceControls.maxEvents;auto& suppressTicks=traceControls.suppressTicks;
    auto& eventFilter=traceControls.filter;
    ImGui::SliderInt("Recording seconds",&seconds,1,60);
    ImGui::SliderInt("Maximum events",&maxEvents,1,4096);
    ImGui::Checkbox("Suppress tick and animation-update calls",&suppressTicks);
    ImGui::InputText("Function path contains",eventFilter,sizeof(eventFilter));
    ImGui::InputTextMultiline("Include any (one term per line)",traceControls.includeTerms,sizeof(traceControls.includeTerms),ImVec2(-1,80));
    ImGui::InputTextMultiline("Exclude any (one term per line)",traceControls.excludeTerms,sizeof(traceControls.excludeTerms),ImVec2(-1,80));
    ImGui::TextWrapped("Case-insensitive function-path substrings, up to 16 terms each. Any include term may match; exclusions win. The single contains filter still applies as an additional requirement. Empty includes allow all names in scope.");
    auto& captureEventFields=traceControls.captureFields;
    auto& captureEventParameters=traceControls.captureParameters;
    auto& eventFields=traceControls.fields;
    ImGui::Checkbox("Capture fields when matching events fire", &captureEventFields);
    ImGui::Checkbox("Capture supported event parameters", &captureEventParameters);
    if (captureEventParameters)
        ImGui::TextWrapped("Requires an 8+ character contains filter, or include terms each 8+ characters. First 16 calls only; copies numbers, enums, native bools and direct object paths, including nested structs. References are not traversed and assets are not loaded.");
    if (captureEventFields) {
        ImGui::InputTextMultiline("Event field paths (JSON)", eventFields, sizeof(eventFields), ImVec2(-1, 100));
        ImGui::TextWrapped("Roots: Context, Player, Controller, Target. Requires an 8+ character contains filter, or include terms each 8+ characters. Captures only the first 16 matching calls, 512 nodes per call. Object fields are separate from event parameters. No function invocation.");
    }
    }
    ImGui::SeparatorText("3. Record the interaction and export");
    ImGui::BeginDisabled(!available || busy);
    const bool targetMissing=traceControls.selectedObject && traceTargetPath.empty();
    if(targetMissing)ImGui::TextWrapped("Select a target in step 2 to enable Start. Do not switch to player scope to bypass a missing object.");
    ImGui::BeginDisabled(targetMissing);
    if(ImGui::Button("Start trace")) {
        try {
            const auto options=traceControls.Options();
            Queue(Action::PlayerTraceStart,options.dump());
        } catch (const std::exception& error) { status=error.what(); }
    }
    ImGui::EndDisabled();
    if(ImGui::Button("Stop and export trace"))Queue(Action::PlayerTraceStop);
    ImGui::InputText("Action marker",marker,sizeof(marker));
    if(ImGui::Button("Add marker"))Queue(Action::PlayerTraceMarker,marker);
    ImGui::EndDisabled();
    RenderEventRuleEditor();
    if(ImGui::CollapsingHeader("Appearance event trace (experimental)")) {
    ImGui::TextWrapped("Manual, local-player trace for armor, held items, customization and shader ramps. Uses version-checked native hooks; records at most 256 events over 60 seconds. Stop and export detaches this recorder. Shared hooks may remain for configured ghost refresh. The recorder does not change materials.");
    ImGui::BeginDisabled(!available || busy);
    if (ImGui::Button("Start appearance trace")) Queue(Action::TraceStart);

    if (ImGui::Button("Stop and export appearance trace")) Queue(Action::TraceStop);
    ImGui::EndDisabled();
    }
    }
    if (section == Section::Inspector) {
    ImGui::SeparatorText("Capture target");
    ImGui::TextWrapped("Capture the first solid surface along the camera direction, or inspect your player and components. Captures run once; there is no live object polling.");
    ImGui::BeginDisabled(!available || busy);
    ImGui::SliderFloat("Trace distance (meters)", &rangeMeters, 1, 500, "%.0f");
    ImGui::Checkbox("Delay crosshair capture by 3 seconds", &delay);
    if (ImGui::Button("Inspect crosshair target")) Queue(Action::Target);
    ImGui::SliderInt("Player capture delay (seconds)", &playerCaptureDelay, 0, 30);
    ImGui::TextDisabled("0 captures immediately. Delayed captures cancel on world changes.");
    if (ImGui::Button("Inspect local player")) Queue(Action::Player);
    if (ImGui::Button("Save player capture preset")) {
        try {
            const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            json preset = {{"Name", "PlayerCapture"}, {"IncludePlayer", true}, {"CaptureDelaySeconds", playerCaptureDelay}};
            ValidatePreset(preset);
            std::filesystem::create_directories(PresetFolder());
            const auto file = PresetFolder() / ("PlayerCapture_" + std::to_string(stamp) + ".jsonc");
            ConfigFiles::Write(file, preset.dump(2));
            presetsLoaded = false;
            status = "Player capture preset saved. Run it from Presets to reuse the delay.";
        } catch (const std::exception& error) { status = error.what(); }
    }
    ImGui::SeparatorText("Find a loaded object");
    ImGui::InputText("Loaded object path contains", search, sizeof(search));
    ImGui::SliderInt("Maximum loaded object results", &objectResultLimit, 1, 500);
    objectResultLimit = std::clamp(objectResultLimit, 1, 500);
    ImGui::TextDisabled("Limit applies to the next search.");
    if (ImGui::Button("Search loaded objects") && search[0]) {
        objectSearchComplete=false;matches.clear();Queue(Action::Browse, search);
        pending.objectLimit = objectResultLimit;
    }
    ImGui::EndDisabled();
    }
    if (!available) ImGui::TextWrapped("Game-thread tools unavailable: callback registration failed.");
    ImGui::TextWrapped("%s", status.c_str());
    if (section != Section::Results && section != Section::Inspector) return;
    ImGui::BeginDisabled(busy || !objectSearchComplete);
    if(ImGui::Button("Export object search results")) {
        try {status="Exported runtime/"+Export(SearchReport(objectSearchQuery,
            "Loaded object paths (case-insensitive substring)","",json(matches),completedObjectResultLimit),"ObjectSearch");}
        catch(const std::exception& error){status=error.what();}
    }
    ImGui::EndDisabled();
    if(objectSearchComplete) {
        objectResultsFilter.Draw("Filter loaded object results", -1.0f);
        const auto visible = std::count_if(matches.begin(), matches.end(),
            [](const auto& path) { return objectResultsFilter.PassFilter(path.c_str()); });
        ImGui::TextWrapped("Query: %s | Showing %zu of %zu matches (search limit %d).",
            objectSearchQuery.c_str(), static_cast<size_t>(visible), matches.size(), completedObjectResultLimit);
        ImGui::TextWrapped("Filter narrows the captured list only. Export includes all captured matches. Use -WidgetTree to hide child-widget paths; clear the filter to show all.");
        if (!visible) ImGui::TextDisabled("No results match this filter.");
    }
    if (!selectedPath.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Selected object:");
        ImGui::TextWrapped("%s", selectedPath.c_str());
    }
    if(section==Section::Inspector)ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if (!matches.empty() && ImGui::TreeNode("Loaded object results")) {
        ImGui::BeginChild("LoadedObjectResultsScroll", ImVec2(0, 190), true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::BeginDisabled(busy || !available);
        for (const auto& path : matches) {
            if (!objectResultsFilter.PassFilter(path.c_str())) continue;
            const bool selected = selectedPath == path;
            const auto label = (selected ? "> " : "  ") + path + "###" + path;
            if (ImGui::Selectable(label.c_str(), selected)) Queue(Action::Object, path);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
        }
        ImGui::EndDisabled(); ImGui::EndChild(); ImGui::TreePop();
    }
    if (section != Section::Results) {
        if (!snapshot.is_null()) ImGui::TextWrapped("Open Results to view or export the captured snapshot.");
        return;
    }
    if (snapshot.is_null()) {ImGui::TextWrapped("No captured snapshot. Run an inspection, preset or trace first.");return;}
    ImGui::SeparatorText("Captured snapshot");
    if(snapshotTextDirty) { snapshotText=snapshot.dump(2);snapshotTextDirty=false; }
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Copy snapshot JSON")) ImGui::SetClipboardText(snapshotText.c_str());

    if (ImGui::Button("Export snapshot")) {
        try {
            const auto name = Export(snapshot, "Inspector");
            status = "Exported to Mods/RuneSchema/runtime/" + name;
        } catch (const std::exception& error) { status = error.what(); }
    }
    ImGui::EndDisabled();
    propertyFilter.Draw("Filter captured fields");
    ImGui::BeginChild("SnapshotFields",ImVec2(0,300),true,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (snapshot.contains("object")) {
        ShowObject(snapshot["object"]);
        for (const auto& component : snapshot["components"]) {
            const auto path = component.at("path").get<std::string>();
            if (ImGui::TreeNode(path.c_str())) { ShowObject(component); ImGui::TreePop(); }
        }
        if (snapshot.contains("controller") && ImGui::TreeNode("Controller fields")) { ShowObject(snapshot["controller"]); ImGui::TreePop(); }
        if (ImGui::TreeNode("Function signatures")) {
            for (const auto& fn : snapshot["functions"]) {
                const auto line = fn.dump();
                if (propertyFilter.PassFilter(line.c_str())) ImGui::TextWrapped("%s", line.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        const auto& text = snapshotText;
        size_t begin = 0;
        while (begin < text.size()) {
            const auto end = text.find('\n', begin);
            const auto line = text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
            if (propertyFilter.PassFilter(line.c_str())) ImGui::TextWrapped("%s", line.c_str());
            if (end == std::string::npos) break;
            begin = end + 1;
        }
    }
    ImGui::EndChild();

}

void RenderHistory() {
    std::lock_guard lock(mutex);
    ImGui::SeparatorText("Workflow history");
    ImGui::TextWrapped("Recent results and status statements. The colored label identifies the originating tool tab.");
    ImGui::BeginChild("WorkflowHistory", ImVec2(0, 112), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    const auto colorFor = [](Section section) {
        switch (section) {
            case Section::Inspector: return ImVec4(.22f, .43f, .64f, 1);
            case Section::Presets: return ImVec4(.55f, .37f, .15f, 1);
            case Section::Traces: return ImVec4(.43f, .32f, .59f, 1);
            case Section::Results: return ImVec4(.16f, .46f, .40f, 1);
            case Section::SavedItems: return ImVec4(.22f, .43f, .64f, 1);
            case Section::SavedWorld: return ImVec4(.55f, .37f, .15f, 1);
            case Section::AssetTemplates: return ImVec4(.22f, .43f, .64f, 1);
            case Section::Niagara: return ImVec4(.57f, .32f, .18f, 1);
        }
        return ImVec4(.5f, .5f, .5f, 1);
    };
    const auto labelFor = [](Section section) {
        switch (section) {
            case Section::Inspector: return "Inspector";
            case Section::Presets: return "Presets";
            case Section::Traces: return "Traces";
            case Section::Results: return "Results";
            case Section::SavedItems: return "Saved items";
            case Section::SavedWorld: return "Building registry";
            case Section::AssetTemplates: return "Search and author";
            case Section::Niagara: return "Niagara";
        }
        return "RuneSchema";
    };
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        ImGui::TextColored(colorFor(it->section), "[%s]", labelFor(it->section));
        ImGui::SameLine();
        ImGui::TextWrapped("%s", it->message.c_str());
    }
    ImGui::EndChild();
}
}
