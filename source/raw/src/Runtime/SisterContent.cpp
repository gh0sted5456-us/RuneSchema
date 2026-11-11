#include "Runtime/SisterContent.h"

#include "Core/ConfigFiles.h"
#include "Core/UE4SSPackages.h"
#include "Runtime/PluginCatalog.h"
#include "Utility/Logging.h"
#include "UE4SSProgram.hpp"
#include <lua.hpp>
#include <cstdlib>
#include <map>
#include <memory>

namespace PS::SisterContent {
namespace fs = std::filesystem;
namespace {
void* Allocate(void* context, void* pointer, size_t previous, size_t size)
{
    auto& used = *static_cast<size_t*>(context);
    const auto retained = used - (pointer ? previous : 0);
    if (!size) {
        std::free(pointer);
        used = retained;
        return nullptr;
    }
    if (size > 256 * 1024 - retained) return nullptr;
    auto* result = std::realloc(pointer, size);
    if (result) used = retained + size;
    return result;
}

bool RunRegistration(const fs::path& file)
{
    auto script = ConfigFiles::Read(file, 4096);
    if (script.starts_with("\xef\xbb\xbf")) script.erase(0, 3);
    size_t allocated = 0;
    std::unique_ptr<lua_State, decltype(&lua_close)> state(lua_newstate(Allocate, &allocated), lua_close);
    if (!state) throw std::runtime_error("Cannot allocate registration Lua state");
    auto* lua = state.get();
    bool registered = false;
    lua_newtable(lua);
    lua_pushlightuserdata(lua, &registered);
    lua_pushcclosure(lua, [](lua_State* call) {
        if (lua_gettop(call) != 0) return luaL_error(call, "RegisterContent takes no arguments");
        *static_cast<bool*>(lua_touserdata(call, lua_upvalueindex(1))) = true;
        return 0;
    }, 1);
    lua_setfield(lua, -2, "RegisterContent");
    lua_setglobal(lua, "RuneSchema");
    lua_sethook(lua, [](lua_State* call, lua_Debug*) {
        luaL_error(call, "Registration instruction budget exceeded");
    }, LUA_MASKCOUNT, 10000);
    if (luaL_loadbufferx(lua, script.data(), script.size(), "@runeschema/register.lua", "t") != LUA_OK
        || lua_pcall(lua, 0, 0, 0) != LUA_OK) {
        const auto* error = lua_tostring(lua, -1);
        throw std::runtime_error(error ? error : "Registration script failed");
    }
    return registered;
}
}

std::vector<Root> Discover(const fs::path& runeMods)
{
    using Program = RC::UE4SSProgram;
    auto& host = Program::get_program();
    const auto enabled = host.get_mods_txt_entries();
    const auto& directories = host.get_mods_directories();
    std::map<std::string, Root> accepted;
    for (auto directory = directories.rbegin(); directory != directories.rend(); ++directory) {
        std::error_code scanError;
        fs::directory_iterator entry(*directory, fs::directory_options::skip_permission_denied, scanError), end;
        while (!scanError && entry != end) {
            const auto folder = entry->path();
            entry.increment(scanError);
            const auto content = folder / "runeschema";
            const auto script = content / "register.lua";
            std::error_code error;
            if (!fs::is_directory(content, error) || UE4SSPackages::Linked(folder)
                || UE4SSPackages::Linked(content) || !fs::is_regular_file(script, error)) continue;
            const auto name = folder.stem().native();
            const auto id = RC::to_string(name);
            const auto key = ModFolderLayout::FoldAscii(id);
            if (!PluginCatalog::Token(id) || key == "runeschema" || key == "shared" || key == "fmodel"
                || accepted.contains(key)) continue;
            const auto setting = enabled.find(id);
            if ((setting == enabled.end() || !setting->second)
                && !fs::is_regular_file(folder / "enabled.txt", error)) continue;
            try {
                const auto* native = Program::find_mod_by_name<RC::CppMod>(name, Program::IsInstalled::Yes);
                const auto* lua = Program::find_mod_by_name<RC::LuaMod>(name, Program::IsInstalled::Yes);
                if ((!native || !fs::equivalent(native->get_path(), folder, error))
                    && (!lua || !fs::equivalent(lua->get_path(), folder, error))) continue;
                if (ModFolderLayout::LooksLikeRuneSchemaMod(runeMods / name))
                    throw std::runtime_error("A RuneSchema mod already owns this identity");
                for (const auto& child : fs::recursive_directory_iterator(content)) {
                    if (UE4SSPackages::Linked(child.path()))
                        throw std::runtime_error("Linked file or directory in sister content");
                }
                if (!RunRegistration(script))
                    throw std::runtime_error("Registration script must call RuneSchema.RegisterContent()");
                accepted.emplace(key, Root{name, content});
            } catch (const std::exception& failure) {
                Log<RC::LogLevel::Warning>(STR("[SISTER-CONTENT][SKIPPED] {}: {}.\n"), name, ToWideSafe(failure.what()));
            }
        }
    }
    std::vector<Root> roots;
    for (auto& [key, root] : accepted) {
        Log<RC::LogLevel::Normal>(STR("[SISTER-CONTENT][REGISTERED] {}: {}\n"), root.Name, root.Content.native());
        roots.push_back(std::move(root));
    }
    return roots;
}
}
