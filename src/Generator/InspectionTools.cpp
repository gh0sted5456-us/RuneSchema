#include "Generator/InspectionTools.h"
#include "Generator/DiagnosticPreset.h"
#include "Generator/FocusedCapture.h"
#include "Generator/AppearanceTrace.h"
#include "Generator/PlayerTrace.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Runtime/HostServices.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <atomic>

using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;
using nlohmann::json;

namespace PS::InspectionTools {
namespace {
    std::mutex mutex;
    enum class Action { None, Target, Player, Browse, Object, Preset, TraceStart, TraceStop, PlayerTraceStart, PlayerTraceStop, PlayerTraceMarker };
    json appearanceTraceStart;
    struct Request { Action action = Action::None; std::string text; float range = 10000;
        std::chrono::steady_clock::time_point due; std::string selectedObject; };
    Request pending;
    std::atomic<bool> hasRequest = false;
    bool busy = false;
    std::string status = "Enter a world, then capture a snapshot.";
    json snapshot;
    std::string snapshotText;
    bool snapshotTextDirty=true;
    std::vector<std::string> matches;
    std::string selectedPath;
    char search[256] = "Jump";
    ImGuiTextFilter propertyFilter;
    float rangeMeters = 100;
    bool delay = true;
    char presetEditor[16384] = "{\n  \"Name\": \"MyPlayer\",\n  \"Objects\": [],\n  \"ControllerProperties\": [],\n  \"IncludePlayer\": true,\n  \"IncludeControllerComponents\": true\n}";
    std::vector<json> presets;
    int selectedPreset = -1;
    bool presetsLoaded = false;
    std::atomic<bool> extendedCaptureDepth = false;
    std::vector<std::string> presetErrors;
    std::filesystem::path PresetFolder() {
        return std::filesystem::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "diagnostics" / "presets";
    }
    void ReloadPresets() {
        presets.clear(); presetErrors.clear(); selectedPreset = -1; presetsLoaded = true;
        std::filesystem::create_directories(PresetFolder());
        int invalid = 0;
        bool capacityReached = false;
        for (const auto& entry : std::filesystem::directory_iterator(PresetFolder())) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            if (presets.size() >= 64) { capacityReached = true; break; }
            try {
                if (entry.file_size() > 16383) throw std::runtime_error("Preset file too large.");
                std::ifstream file(entry.path()); json preset; file >> preset; ValidatePreset(preset, extendedCaptureDepth.load());
                presets.push_back(std::move(preset));
            } catch (const std::exception& error) {
                ++invalid;
                if (presetErrors.size() < 64) presetErrors.push_back(entry.path().filename().string() + ": " + error.what());
            }
        }
        std::sort(presets.begin(), presets.end(), [](const auto& a, const auto& b) { return a.at("Name") < b.at("Name"); });
        status = "Loaded " + std::to_string(presets.size()) + " presets; skipped " + std::to_string(invalid) + " invalid files.";
        if (capacityReached) status += " Preset capacity reached (64); remaining files were not read.";
    }

    std::string Path(UObject* object) { return object ? to_string(object->GetPathName()) : ""; }
    std::string Lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }
    template<class T = UObject*> T Find(const CharType* path) {
        return UECustom::UObjectGlobals::StaticFindObject<T>(nullptr, nullptr, path);
    }
    void Queue(Action action, std::string text = {}) {
        if (busy) return;
        auto inspectedPath = selectedPath;
        if (inspectedPath.empty() && snapshot.is_object() && snapshot.contains("object") && snapshot["object"].is_object())
            inspectedPath = snapshot["object"].value("path", std::string{});
        pending = { action, std::move(text), rangeMeters * 100,
            std::chrono::steady_clock::now() + std::chrono::seconds(action == Action::Target && delay ? 3 : 0), std::move(inspectedPath) };
        busy = true;
        hasRequest = true;
        selectedPath = action == Action::Object ? pending.text : "";
        status = action == Action::Target && delay ? "Capture queued: close the console and aim within 3 seconds." : "Capture queued...";
    }
    json Fields(UStruct* type, void* data = nullptr) {
        json fields = json::array();
        for (auto* p : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
            json f = {{"name", to_string(p->GetName())}, {"type", PropertyHelper::GetPropertyTypeAsUTF8String(p)},
                {"offset", p->GetOffset_Internal()}, {"arrayDim", p->GetArrayDim()}};
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
        for (UStruct* type = object->GetClassPrivate(); type; type = type->GetSuperStruct())
            result["hierarchy"].push_back({{"class", Path(type)}, {"fields", Fields(type, object)}});
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
                    auto* fn = static_cast<UFunction*>(candidate);
                    result["functions"].push_back({{"path", path}, {"parameters", Fields(fn)}, {"parameterSize", fn->GetParmsSize()}});
                }
            }
            return LoopAction::Continue;
        });
        return result;
    }
    json ConfigurationValue(FProperty* p, void* address, int depth = 0) {
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
    std::string Export(const json& report, const std::string& prefix) {
        const auto folder = std::filesystem::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "diagnostics";
        std::filesystem::create_directories(folder);
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto name = prefix + "-" + std::to_string(stamp) + ".json";
        std::ofstream file(folder / name); file << report.dump(2); file.flush();
        if (!file) throw std::runtime_error("Could not write research snapshot.");
        return name;
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
void Tick() {
    PlayerTrace::Tick();
    AppearanceTrace::Tick();
    if (!hasRequest.load(std::memory_order_relaxed)) return;
    Request request;
    { std::lock_guard lock(mutex);
      if (pending.action == Action::None || std::chrono::steady_clock::now() < pending.due) return;
      request = std::move(pending); pending = {}; snapshot = nullptr; snapshotTextDirty=true; hasRequest = false; }
    try {
        json result;
        if(request.action==Action::PlayerTraceStart || request.action==Action::PlayerTraceStop
            || request.action==Action::PlayerTraceMarker) {
            std::string message;
            if(request.action==Action::PlayerTraceStart) {
                auto* controller=Controller();PlayerTrace::Start(Pawn(controller),controller,json::parse(request.text));
                message="Player Trace started. It automatically detaches at its limit; Stop and export saves the recording.";
            } else if(request.action==Action::PlayerTraceStop) {
                result=PlayerTrace::Stop();
                try { message="Exported "+Export(result,"PlayerTrace"); }
                catch(const std::exception& error) { message=std::string("Trace stopped; use Export JSON to retry: ")+error.what(); }
            } else if(request.action==Action::PlayerTraceMarker) { PlayerTrace::Marker(request.text);message="Marker added."; }
            std::lock_guard lock(mutex);snapshot=std::move(result);status=message;busy=false;return;
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
                        result["errors"].push_back({{"target", target}, {"error", error.what()}});
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
                        result["objects"].push_back({{"type", Path(type)}, {"fields", Fields(type)}});
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
            const auto file = Export(result, "Preset-" + preset.at("Name").get<std::string>());
            const auto message = "Saved diagnostics/" + file + " (" + std::to_string(result["errors"].size()) + " errors).";
            std::lock_guard lock(mutex); snapshot = std::move(result); status = message; busy = false; return;
        }
        if (request.action == Action::Browse) {
            std::vector<std::string> found;
            const auto needle = Lower(request.text);
            UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                if (object && found.size() < 250) {
                    auto path = Path(object);
                    if (Lower(path).find(needle) != std::string::npos) found.push_back(std::move(path));
                }
                return found.size()>=250 ? LoopAction::Break : LoopAction::Continue;
            });
            std::sort(found.begin(), found.end());
            std::lock_guard lock(mutex); matches = std::move(found);
            status = std::to_string(matches.size()) + " loaded matches (limit 250). Select one to inspect; narrow the search if capped.";
            busy = false; return;
        }
        if (request.action == Action::Object) {
            auto wide = to_wstring(request.text);
            auto* object = Find(wide.c_str());
            if (!object) throw std::runtime_error("Object unloaded since search. Search again.");
            if (object->IsA(UClass::StaticClass()) || object->IsA(UScriptStruct::StaticClass()) || object->IsA(UFunction::StaticClass())) {
                auto* type = static_cast<UStruct*>(object);
                result = {{"type", Path(type)}, {"fields", Fields(type)}};
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
                result = Capture(owner.Result<UObject*>());
                result["hit"] = {{"component", Path(component)}, {"impact", {impact.X(), impact.Y(), impact.Z()}}, {"rangeCm", request.range}};
            }
        }
        std::lock_guard lock(mutex); snapshot = std::move(result); busy = false;
        status = "Snapshot captured. Open Results to filter fields, copy paths, or export JSON.";
    } catch (const std::exception& error) {
        std::lock_guard lock(mutex); busy = false; snapshot = nullptr; status = error.what();
    }
}
void Render(bool available, Section section) {
    std::lock_guard lock(mutex);
    if (section == Section::Presets) {
        ImGui::SeparatorText("Saved diagnostic presets");
        ImGui::BeginDisabled(busy);
        bool extended = extendedCaptureDepth.load();
        if (ImGui::Checkbox("Allow deeper captures this session (up to 16)", &extended)) {
            extendedCaptureDepth.store(extended);
            presetsLoaded = false;
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Default depth: 7. Standard maximum: 10. Deeper captures can stall the game; node and entry limits remain unchanged. Existing presets keep their explicit depth.");
        try { if (!presetsLoaded) ReloadPresets(); }
        catch (const std::exception& error) { status = error.what(); }
        if (!presetErrors.empty() && ImGui::TreeNode("Skipped preset details")) {
            for (const auto& error : presetErrors) ImGui::TextWrapped("%s", error.c_str());
            ImGui::TreePop();
        }
        ImGui::TextWrapped("Presets can inspect chosen property paths or all properties (*) of selected objects, including maps/arrays and optional object references. CaptureLimits bound scope and report omissions. Broad captures can stall the game. Run saves a timestamped report; presets contain data only.");
        const auto selectedName = selectedPreset >= 0 && selectedPreset < static_cast<int>(presets.size())
            ? presets[selectedPreset].at("Name").get<std::string>() : std::string("Select preset");
        if (ImGui::BeginCombo("Preset", selectedName.c_str())) {
            for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::Selectable(presets[i].at("Name").get_ref<const std::string&>().c_str(), selectedPreset == i)) selectedPreset = i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("Reload presets")) {
            try { ReloadPresets(); } catch (const std::exception& error) { status = error.what(); }
        }
        ImGui::BeginDisabled(selectedPreset < 0);

        if (ImGui::Button("Edit selected")) {
            const auto text = presets[selectedPreset].dump(2);
            if (text.size() < sizeof(presetEditor)) std::snprintf(presetEditor, sizeof(presetEditor), "%s", text.c_str());
            else status = "Preset is too large for the editor.";
        }
        ImGui::BeginDisabled(!available);
         if (ImGui::Button("Run selected preset")) Queue(Action::Preset, presets[selectedPreset].dump());
        ImGui::EndDisabled(); ImGui::EndDisabled();
        ImGui::InputTextMultiline("Preset JSON", presetEditor, sizeof(presetEditor), ImVec2(-1, 180));
        if (ImGui::Button("Save preset")) {
            try {
                const auto preset = json::parse(presetEditor); ValidatePreset(preset, extendedCaptureDepth.load());
                std::filesystem::create_directories(PresetFolder());
                const auto name = preset.at("Name").get<std::string>();
                const auto fileName = "user-" + name + ".json";
                std::ofstream file(PresetFolder() / fileName); file << preset.dump(2); file.flush();
                if (!file) throw std::runtime_error("Could not save preset.");
                file.close(); ReloadPresets(); status = "Saved presets/" + fileName + ". Saving the same Name replaces that saved preset.";
            } catch (const std::exception& error) { status = error.what(); }
        }
        ImGui::EndDisabled();
    }
    if (section == Section::Traces) {
    ImGui::SeparatorText("Player Trace");
    ImGui::TextWrapped("Records reflected player/controller calls and owned subobjects. Direct native and external world-object calls can be absent. Categories are inferred from names. Limits stop and detach automatically; export afterward. Broad tracing can reduce performance.");
    static bool categories[]{true,true,true,true,false};
    constexpr const char* labels[]{"Equipment / appearance","Combat / magic","Interactions / inventory / crafting","Movement","Other reflected calls"};
    unsigned categoryMask=0;
    for(unsigned i=0;i<5;++i) { ImGui::Checkbox(labels[i],&categories[i]);if(categories[i])categoryMask|=1u<<i; }
    static int seconds=30,maxEvents=1024;static bool suppressTicks=true;
    static char eventFilter[129]="",marker[129]="";
    ImGui::SliderInt("Recording seconds",&seconds,1,60);
    ImGui::SliderInt("Maximum events",&maxEvents,64,4096);
    ImGui::Checkbox("Suppress tick and animation-update calls",&suppressTicks);
    ImGui::InputText("Function path contains",eventFilter,sizeof(eventFilter));
    ImGui::BeginDisabled(!available || busy);
    if(ImGui::Button("Start Player Trace"))Queue(Action::PlayerTraceStart,json{{"Seconds",seconds},{"MaxEvents",maxEvents},{"Categories",categoryMask},{"Filter",eventFilter},{"SuppressTicks",suppressTicks}}.dump());
    if(ImGui::Button("Stop and export Player Trace"))Queue(Action::PlayerTraceStop);
    ImGui::InputText("Action marker",marker,sizeof(marker));
    if(ImGui::Button("Add marker"))Queue(Action::PlayerTraceMarker,marker);
    ImGui::EndDisabled();
    ImGui::SeparatorText("Appearance event trace (experimental)");
    ImGui::TextWrapped("Manual, local-player trace for armor, held items, customization and shader ramps. Uses version-checked native hooks; records at most 256 events over 60 seconds. Stop and export detaches this recorder. Shared hooks may remain for configured ghost refresh. The recorder does not change materials.");
    ImGui::BeginDisabled(!available || busy);
    if (ImGui::Button("Start appearance trace")) Queue(Action::TraceStart);

    if (ImGui::Button("Stop and export appearance trace")) Queue(Action::TraceStop);
    ImGui::EndDisabled();
    }
    if (section == Section::Inspector) {
    ImGui::SeparatorText("World and player inspector");
    ImGui::TextWrapped("Capture the first solid surface along the camera direction, or inspect your player and components. Captures run once; there is no live object polling.");
    ImGui::BeginDisabled(!available || busy);
    ImGui::SliderFloat("Trace distance (meters)", &rangeMeters, 1, 500, "%.0f");
    ImGui::Checkbox("Delay crosshair capture by 3 seconds", &delay);
    if (ImGui::Button("Inspect crosshair target")) Queue(Action::Target);
     if (ImGui::Button("Inspect local player")) Queue(Action::Player);
    ImGui::InputText("Loaded object path contains", search, sizeof(search));
    if (ImGui::Button("Search loaded objects") && search[0]) Queue(Action::Browse, search);
    ImGui::EndDisabled();
    }
    if (!available) ImGui::TextWrapped("Game-thread tools unavailable: callback registration failed.");
    ImGui::TextWrapped("%s", status.c_str());
    if (section != Section::Results && section != Section::Inspector) return;
    if (!selectedPath.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Selected object:");
        ImGui::TextWrapped("%s", selectedPath.c_str());
    }
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if (!matches.empty() && ImGui::TreeNode("Loaded object results")) {
        ImGui::BeginChild("LoadedObjectResultsScroll", ImVec2(0, 190), true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::BeginDisabled(busy || !available);
        for (const auto& path : matches) {
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
    if (snapshot.is_null()) return;
    if(snapshotTextDirty) { snapshotText=snapshot.dump(2);snapshotTextDirty=false; }
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Copy snapshot JSON")) ImGui::SetClipboardText(snapshotText.c_str());

    if (ImGui::Button("Export snapshot JSON")) {
        try {
            const auto folder = std::filesystem::path(PS::HostServices::WorkingDirectory()) / "Mods" / "RuneSchema" / "diagnostics";
            std::filesystem::create_directories(folder);
            const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto name = "Inspector-" + std::to_string(stamp) + ".json";
            std::ofstream file(folder / name); file << snapshotText; file.flush();
            if (!file) throw std::runtime_error("Could not write inspector snapshot.");
            status = "Exported to Mods/RuneSchema/diagnostics/" + name;
        } catch (const std::exception& error) { status = error.what(); }
    }
    ImGui::EndDisabled();
    propertyFilter.Draw("Filter captured fields");

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

}
}
