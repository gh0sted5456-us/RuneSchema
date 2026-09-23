#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "FAIL: cannot open " << path << '\n';
        std::exit(1);
    }
    std::ostringstream value;
    value << file.rdbuf();
    return value.str();
}

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    Check(argc == 7, "six documentation inputs supplied");
    const auto authoring = Read(argv[1]);
    const auto loaders = Read(argv[2]);
    const auto sourcePlugins = Read(argv[3]);
    const auto runtimePlugins = Read(argv[4]);
    const auto api = Read(argv[5]);
    const auto apiHeader = Read(argv[6]);

    constexpr std::array<const char*, 22> names{{
        "assets", "blueprints", "buildings", "courses", "dialogue", "effects",
        "enums", "equipment", "events", "journal", "lore", "nameplates",
        "niagara", "npc", "players", "quests", "raw", "recipes", "registry",
        "spawns", "strings", "vendors"
    }};
    for (const auto* name : names) {
        const auto heading = std::string("## `") + name + "`";
        Check(loaders.find(heading) != std::string::npos, "loader walkthrough missing");
    }

    for (const auto* section : {
        "## Runtime layout", "## Ordering", "## Cooked assets and `$declaration`",
        "## `/raw` and `/registry`", "## Storefront and mappings", "## Plugins",
        "## Multiplayer checklist", "## Failure isolation and log tags", "## Test sequence"
    }) Check(authoring.find(section) != std::string::npos, "authoring section missing");

    Check(sourcePlugins.find("Required plugins cannot be disabled") == std::string::npos,
        "source plugins.txt has stale required-plugin wording");
    Check(runtimePlugins.find("Required plugins cannot be disabled") == std::string::npos,
        "runtime plugins.txt has stale required-plugin wording");

    for (const auto* symbol : {
        "RUNESCHEMA_PLUGIN_API_VERSION", "RUNESCHEMA_PLUGIN_MAX_MESSAGE",
        "RuneSchemaPluginDescriptor", "RuneSchemaHostApi", "Log", "RegisterCapability",
        "RegisterService", "CallService", "FindObject", "ForEachObjectOfClass",
        "FindFieldClass", "IsFieldA", "ResolveBinding"
    }) {
        Check(api.find(symbol) != std::string::npos, "public API symbol is undocumented");
        Check(apiHeader.find(symbol) != std::string::npos, "documented ABI symbol is absent from header");
    }
    for (const auto* symbol : {
        "RuneSchemaPlugin_Query", "RuneSchemaPlugin_Initialize", "RuneSchemaPlugin_Shutdown",
        "RuneSchemaPlugin_OnUiInit", "RuneSchemaPlugin_OnUnrealInit",
        "runeschema.discovery", "runeschema.mapping", "runeschema.tools", "runeschema.bindings"
    }) Check(api.find(symbol) != std::string::npos, "export or core service is undocumented");
    for (const auto* result : {
        "RS_PLUGIN_OK", "RS_PLUGIN_INVALID_ARGUMENT", "RS_PLUGIN_INCOMPATIBLE_API",
        "RS_PLUGIN_DUPLICATE", "RS_PLUGIN_NOT_FOUND", "RS_PLUGIN_BUFFER_TOO_SMALL",
        "RS_PLUGIN_FAILED", "RS_LOG_INFO", "RS_LOG_WARNING", "RS_LOG_ERROR"
    }) Check(api.find(result) != std::string::npos, "API result or log level is undocumented");
    for (const auto* field : {
        "SchemaVersion", "Id", "Name", "Version", "ApiVersion", "BuiltForRuneSchema",
        "EntryPoint", "Enabled", "ConsoleMessage", "Capabilities", "Connections",
        "Dependencies", "Required", "plugins.txt"
    }) Check(api.find(field) != std::string::npos, "plugin manifest field is undocumented");
    std::cout << "Documentation contract passed for " << names.size() << " loaders.\n";
}
