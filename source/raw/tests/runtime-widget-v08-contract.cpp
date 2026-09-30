#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Runtime-widget .8 contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("RuneSchema .8 runtime-widget contract is missing: ") + text);
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Header, implementation, Blueprint docs, and Restore Appearance example are required");
    const auto header = Read(argv[1]);
    const auto source = Read(argv[2]);
    const auto docs = Read(argv[3]);
    const auto restoreAppearance = Read(argv[4]);

    Require(header, "m_runtimeWidgetActiveRules");
    Require(header, "m_runtimeWidgetCompletedRules");
    Require(header, "ApplyRuntimeWidgetCalls");
    Require(header, "RuntimeWidgetRuleMatchesEvent");
    Require(header, "RuntimeWidgetObservedTarget");
    Require(header, "FindRuntimeWidgetTarget");
    Require(header, "m_runtimeWidgetObservedTargets");
    Require(header, "RuntimeUiRule");
    Require(header, "RuntimeUiInstance");
    Require(header, "m_runtimeUiInstances");
    Require(header, "BuildRuntimeUiNode");

    Require(source, "ActorHelper::FunctionCall");
    Require(source, "properties.erase(\"$Call\")");
    Require(source, "properties.erase(\"$When\")");
    Require(source, "properties.erase(\"$Once\")");
    Require(source, "properties.erase(\"$Activate\")");
    Require(source, "Blueprint $RuntimeWidget $Call Args must be an object");
    Require(source, "Blueprint $RuntimeWidget $Call does not support return-valued functions yet");
    Require(source, "Blueprint $RuntimeWidget $Call does not support output parameters yet");
    Require(source, "Blueprint $RuntimeWidget $Activate target is not a CommonUI activatable widget");
    Require(source, "m_runtimeWidgetActiveRules.emplace(ruleKey)");
    Require(source, "ClearRuntimeWidgetState();");
    Require(source, "properties.erase(\"$Find\")");
    Require(source, "WidgetTree discovery requires Name");
    Require(source, "HUDWidgetRefs");
    Require(source, "/Script/CommonUI.CommonActivatableWidgetContainerBase");
    Require(source, "WidgetList");
    Require(source, "RF_ClassDefaultObject | RF_ArchetypeObject");
    Require(source, "Blueprint $RuntimeWidget $Find matched more than one live object");
    Require(source, "m_runtimeWidgetObservedTargets[discovered]");
    Require(source, "RC::Unreal::UObjectGlobals::ForEachUObject");
    Require(source, "SpawnRuntime::CallWorldContextGetter");
    Require(source, "/Script/Engine.GameplayStatics:GetGameInstance");
    if (source.find("UECustom::UObjectGlobals::ForEachUObject") != std::string::npos)
        throw std::runtime_error("RuneSchema .8 must use RC::Unreal::UObjectGlobals::ForEachUObject");
    if (source.find("OwningGameInstance") != std::string::npos)
        throw std::runtime_error("RuneSchema .8 RuntimeUI must use the world-context GameInstance helper");
    Require(source, "Blueprint $RuntimeUI block exceeds the 16-widget safety limit");
    Require(source, "Blueprint $RuntimeUI tree exceeds the 64-node safety limit");
    Require(source, "/Script/UMG.UserWidget");
    Require(source, "/Script/UMG.WidgetTree");
    Require(source, "/Script/UMG.CanvasPanel");
    Require(source, "/Script/UMG.Border");
    Require(source, "/Script/UMG.TextBlock");
    Require(source, "/Script/UMG.Image");
    Require(source, "/Script/UMG.Button");
    Require(source, "AddToViewport");
    Require(source, "RemoveFromParent");
    Require(source, "m_runtimeUiActiveRules.emplace(key)");
    Require(source, "propertyName == \"$RuntimeWidget\" || propertyName == \"$RuntimeUI\"");

    Require(docs, "RuneSchema .8 test-bed runtime actions");
    Require(docs, "`$Call`");
    Require(docs, "`$When` and `$Once`");
    Require(docs, "`$Activate`");
    Require(docs, "`$RuntimeUI`");
    Require(docs, "## Scoped runtime discovery");
    Require(docs, "There is intentionally no general `FindAllOf(UserWidget)` authoring path.");
    Require(docs, "## RuneSchema-owned transient UI");
    Require(docs, "64 nodes");
    Require(docs, "does **not** create a Blueprint class");
    Require(restoreAppearance, "\"$RuntimeUI\"");
    Require(restoreAppearance, "\"$Storefront\": \"GamePass\"");
    Require(restoreAppearance, "\"Type\": \"Button\"");
    Require(restoreAppearance, "\"Event\": \"OnClicked\"");
    Require(restoreAppearance, "\"Target\": \"Character\"");
    Require(restoreAppearance, "\"Function\": \"GoToEditAppearance\"");
    if (restoreAppearance.find("Character.EditAppearanceButton") != std::string::npos)
        throw std::runtime_error("Restore Appearance must not depend on the missing Gamepass cooked button");

    std::cout << "RuneSchema .8 runtime-widget contract passed.\n";
}
