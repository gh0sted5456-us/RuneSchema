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
    if (argc != 4) throw std::runtime_error("Header, implementation, and Blueprint docs are required");
    const auto header = Read(argv[1]);
    const auto source = Read(argv[2]);
    const auto docs = Read(argv[3]);

    Require(header, "m_runtimeWidgetActiveRules");
    Require(header, "m_runtimeWidgetCompletedRules");
    Require(header, "ApplyRuntimeWidgetCalls");
    Require(header, "RuntimeWidgetRuleMatchesEvent");
    Require(header, "RuntimeWidgetObservedTarget");
    Require(header, "FindRuntimeWidgetTarget");
    Require(header, "m_runtimeWidgetObservedTargets");

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

    Require(docs, "RuneSchema .8 test-bed runtime actions");
    Require(docs, "`$Call`");
    Require(docs, "`$When` and `$Once`");
    Require(docs, "`$Activate`");
    Require(docs, "`$RuntimeUI`");
    Require(docs, "## Scoped runtime discovery");
    Require(docs, "There is intentionally no general `FindAllOf(UserWidget)` authoring path.");

    std::cout << "RuneSchema .8 runtime-widget contract passed.\n";
}
