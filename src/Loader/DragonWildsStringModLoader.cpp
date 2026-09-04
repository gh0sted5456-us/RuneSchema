#include <vector>

#include <Windows.h>

#include "Unreal/FString.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/Hooks.hpp"
#include "Loader/DragonWildsStringModLoader.h"
#include "SDK/Helper/StringTableHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int needed = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (needed <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<size_t>(needed), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), needed);
        return wide;
    }

    std::wstring ReadReplacement(const nlohmann::json& value)
    {
        if (value.is_string())
        {
            return Utf8ToWide(value.get<std::string>());
        }

        if (!value.is_array())
        {
            return {};
        }

        std::wstring joined;
        for (const auto& line : value)
        {
            if (!line.is_string())
            {
                continue;
            }

            if (!joined.empty())
            {
                joined += L"\r\n";
            }

            joined += Utf8ToWide(line.get<std::string>());
        }

        return joined;
    }
}

namespace DragonWilds {
    DragonWildsStringModLoader::DragonWildsStringModLoader() : DragonWildsModLoaderBase("strings")
    {
        SetDisplayName(TEXT("String Mod Loader"));
    }

    DragonWildsStringModLoader::~DragonWildsStringModLoader() {}

    bool DragonWildsStringModLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit;
    }

    bool DragonWildsStringModLoader::OnInitialize()
    {
        return true;
    }

    void DragonWildsStringModLoader::OnLoad(const fs::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::GameInstanceInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            LoadStrings(data);
        });
    }

    void DragonWildsStringModLoader::LoadStrings(const nlohmann::json& data)
    {
        if (!data.is_object())
        {
            throw std::runtime_error("A strings file must contain a JSON object at the top level");
        }

        for (auto& [Key, Value] : data.items())
        {
            if (Value.is_object())
            {
                auto& Scope = m_scoped[Utf8ToWide(Key)];
                for (auto& [Source, Replacement] : Value.items())
                {
                    AddEntry(Scope, Utf8ToWide(Source), ReadReplacement(Replacement));
                }
            }
            else
            {
                AddEntry(m_global, Utf8ToWide(Key), ReadReplacement(Value));
            }
        }
    }

    void DragonWildsStringModLoader::AddEntry(ReplacementMap& target, const std::wstring& source, const std::wstring& replacement)
    {
        if (source.empty() || replacement.empty() || source == replacement)
        {
            return;
        }

        auto Existing = target.find(source);
        if (Existing != target.end() && Existing->second.To != replacement)
        {
            ++m_conflictCount;
            return;
        }

        target[source].To = replacement;
    }

    void DragonWildsStringModLoader::ApplyPending()
    {
        if (m_global.empty() && m_scoped.empty())
        {
            return;
        }

        if (!m_hooked)
        {
            m_hooked = true;

            if (m_conflictCount > 0)
            {
                PS::Log<LogLevel::Warning>(STR("{} string replacement(s) target text another mod already replaced, the first one won.\n"),
                    m_conflictCount);
            }

            Hook::FCallbackOptions options{};
            options.OwnerModName = TEXT("RuneSchema");
            options.HookName = TEXT("StringModLoaderInitGameState");
            Hook::RegisterInitGameStatePostCallback(
                [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                    ApplyPending();
                }, options);
        }

        std::unordered_map<UObject*, size_t> edits;

        UObject* lastTable = nullptr;
        ReplacementMap* scope = nullptr;

        StringTableHelper::ForEachEntry([&](UObject* table, FString& sourceString) {
            if (table != lastTable)
            {
                lastTable = table;
                auto Found = m_scoped.find(table->GetName());
                scope = Found == m_scoped.end() ? nullptr : &Found->second;
            }

            const int32 Length = sourceString.Len();
            if (Length <= 0)
            {
                return;
            }

            std::wstring Current(*sourceString, static_cast<size_t>(Length));

            Replacement* Match = nullptr;

            if (scope)
            {
                auto Scoped = scope->find(Current);
                if (Scoped != scope->end())
                {
                    Match = &Scoped->second;
                }
            }

            if (!Match)
            {
                auto Global = m_global.find(Current);
                if (Global != m_global.end())
                {
                    Match = &Global->second;
                }
            }

            if (!Match)
            {
                return;
            }

            sourceString = FString(Match->To);

            Match->Matched = true;
            ++edits[table];
        });

        for (const auto& [Table, Count] : edits)
        {
            PS::Log<RC::LogLevel::Normal>(STR("Edited {} entr{} in '{}'.\n"),
                Count, Count == 1 ? STR("y") : STR("ies"), Table->GetName());
        }

        ++m_passCount;
        ReportMissing();
    }

    void DragonWildsStringModLoader::ReportMissing()
    {
        if (m_passCount < 2 || m_reportedMissing)
        {
            return;
        }

        std::vector<RC::StringType> missing;

        for (const auto& [Source, Entry] : m_global)
        {
            if (!Entry.Matched)
            {
                missing.push_back(Source);
            }
        }

        for (const auto& [Table, Entries] : m_scoped)
        {
            for (const auto& [Source, Entry] : Entries)
            {
                if (!Entry.Matched)
                {
                    missing.push_back(std::format(STR("{} (in '{}')"), Source, Table));
                }
            }
        }

        if (missing.empty())
        {
            return;
        }

        m_reportedMissing = true;

        PS::Log<LogLevel::Warning>(STR("{} string(s) were never found, check for typos or stray spaces:\n"), missing.size());

        for (const auto& Entry : missing)
        {
            PS::Log<LogLevel::Warning>(STR("  '{}'\n"), Entry);
        }
    }
}
