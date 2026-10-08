#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace PS::BPModLoaderIntegration {

inline std::string FoldAscii(std::string value) {
    for (auto& character : value)
        if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
    return value;
}

inline bool EnabledInModsTxt(const std::filesystem::path& path, std::string_view wanted) {
    std::ifstream input(path);
    if (!input) return false;
    bool enabled = false, disabled = false;
    std::string line;
    while (std::getline(input, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto name = line.substr(0, colon);
        auto value = line.substr(colon + 1);
        auto trim = [](std::string& text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) { text.clear(); return; }
            text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        };
        trim(name); trim(value);
        if (FoldAscii(name) != FoldAscii(std::string(wanted))) continue;
        if (value == "0") disabled = true;
        if (value == "1") enabled = true;
    }
    return enabled && !disabled;
}

inline constexpr std::string_view PatchBegin = "    -- RuneSchema LogicMods integration BEGIN";
inline constexpr std::string_view PatchEnd = "    -- RuneSchema LogicMods integration END";

inline std::optional<std::string> PatchSource(std::string source) {
    const auto begin = source.find(PatchBegin);
    const auto end = source.find(PatchEnd);
    if ((begin == std::string::npos) != (end == std::string::npos)) return std::nullopt;
    if (begin != std::string::npos) {
        if (end < begin || source.find(PatchBegin, end + PatchEnd.size()) != std::string::npos)
            return std::nullopt;
        auto after = end + PatchEnd.size();
        if (after < source.size() && source[after] == '\r') ++after;
        if (after < source.size() && source[after] == '\n') ++after;
        source.erase(begin, after - begin);
    }
    const auto function = source.find("local function LoadModConfigs()");
    if (function == std::string::npos) return std::nullopt;
    const auto functionEnd = source.find("\nend", function);
    if (functionEnd == std::string::npos) return std::nullopt;
    const auto call = source.find("\nLoadModConfigs()", functionEnd);
    if (call == std::string::npos) return std::nullopt;
    auto insertion = source.find('\n', call + 1);
    if (insertion == std::string::npos) return std::nullopt;
    ++insertion;
    static constexpr std::string_view block = R"LUA(    -- RuneSchema LogicMods integration BEGIN
    do
        local rsEnabled, rsDisabled = false, false
        local rsSource = debug.getinfo(1, "S").source or ""
        if rsSource:sub(1, 1) == "@" then rsSource = rsSource:sub(2) end
        rsSource = rsSource:gsub("\\", "/")
        local rsModsRoot = rsSource:match("^(.*)/BPModLoaderMod/[Ss]cripts/main%.lua$")
        if not rsModsRoot then
            print("[RuneSchema][LOGIC-PAK][PARTIAL] BPModLoaderMod script path could not be resolved.\n")
        else
            local rsOrder = io.open(rsModsRoot .. "/mods.txt", "r")
            if not rsOrder then
                print("[RuneSchema][LOGIC-PAK][PARTIAL] UE4SS mods.txt could not be read.\n")
            else
                for rsLine in rsOrder:lines() do
                    local rsValue = rsLine:lower():match("^%s*runeschema%s*:%s*([01])%s*$")
                    if rsValue == "1" then rsEnabled = true end
                    if rsValue == "0" then rsDisabled = true end
                end
                rsOrder:close()
                if rsEnabled and not rsDisabled then
                    local rsRuneRoot = rsModsRoot .. "/RuneSchema"
                    local rsOk, rsRegister = pcall(dofile, rsRuneRoot .. "/scripts/logicmods-register.lua")
                    if rsOk and type(rsRegister) == "function" then
                        local rsRegistered, rsError = pcall(rsRegister, Mods, OrderedMods, DefaultModConfig, UEHelpers, rsRuneRoot)
                        if not rsRegistered then
                            print("[RuneSchema][LOGIC-PAK][PARTIAL] Registration failed: " .. tostring(rsError))
                        end
                    elseif not rsOk then
                        print("[RuneSchema][LOGIC-PAK][PARTIAL] BPModLoader integration: " .. tostring(rsRegister))
                    end
                end
            end
        end
    end
    -- RuneSchema LogicMods integration END
)LUA";
    source.insert(insertion, block);
    return source;
}
}
