#include <vector>

#include <Windows.h>

#include "Unreal/FString.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/Hooks.hpp"
#include "Loader/DragonWildsStringModLoader.h"
#include "Loader/StringReplacementRules.h"
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
        return Utf8ToWide(DragonWilds::StringReplacementRules::Read(value));
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
            LoadStrings(data, modName);
        });
    }

    void DragonWildsStringModLoader::LoadStrings(const nlohmann::json& data, const RC::StringType& modName)
    {
        StringReplacementRules::Validate(data);

        for (auto& [Key, Value] : data.items())
        {
            if (Value.is_object())
            {
                auto& Scope = m_scoped[Utf8ToWide(Key)];
                for (auto& [Source, Replacement] : Value.items())
                {
                    AddEntry(Scope, Utf8ToWide(Source), ReadReplacement(Replacement), modName, Utf8ToWide(Key));
                }
            }
            else
            {
                AddEntry(m_global, Utf8ToWide(Key), ReadReplacement(Value), modName, L"global");
            }
        }
    }

    void DragonWildsStringModLoader::AddEntry(ReplacementMap& target, const std::wstring& source, const std::wstring& replacement,
        const RC::StringType& modName, const std::wstring& scope)
    {
        if (source.empty() || replacement.empty())
        {
            return;
        }

        auto Existing = target.find(source);
        if (Existing != target.end() && Existing->second.To != replacement)
        {
            PS::Log<LogLevel::Warning>(STR("Strings [{}/{}]: '{}' overrides '{}'; last loaded wins.\n"),
                scope, source, modName, Existing->second.Owner);
        }

        StringReplacementRules::Replace(target[source], replacement, modName);
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
