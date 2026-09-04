#pragma once

#include <string>
#include <unordered_map>

#include "nlohmann/json.hpp"
#include "Loader/DragonWildsModLoaderBase.h"

namespace DragonWilds {

    class DragonWildsStringModLoader : public DragonWildsModLoaderBase {
    public:
        DragonWildsStringModLoader();

        ~DragonWildsStringModLoader();

        void ApplyPending();

    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;

    private:
        struct Replacement {
            std::wstring To;
            bool Matched = false;
        };

        using ReplacementMap = std::unordered_map<std::wstring, Replacement>;

        void LoadStrings(const nlohmann::json& data);

        void AddEntry(ReplacementMap& target, const std::wstring& source, const std::wstring& replacement);

        void ReportMissing();

    private:
        ReplacementMap m_global;

        std::unordered_map<std::wstring, ReplacementMap> m_scoped;

        size_t m_conflictCount = 0;
        size_t m_passCount = 0;
        bool m_hooked = false;
        bool m_reportedMissing = false;
    };
}
