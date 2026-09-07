#include "SDK/DragonWildsSignatures.h"
#include "Signatures.hpp"
#include "SigScanner/SinglePassSigScanner.hpp"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include "ASMHelper/ASMHelper.hpp"
#include <utility>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    void SignatureManager::Initialize()
    {
        std::vector<SignatureContainer> SigContainerBox;
        SigContainerBox.reserve(Signatures.size() + SignaturesCallResolve.size());
        SinglePassScanner::SignatureContainerMap SigContainerMap;

        for (auto& [ClassAndName, Signature] : Signatures)
        {
            SignatureContainer SigContainer = [=]() -> SignatureContainer {
                return {
                    {{Signature}},
                    [=](SignatureContainer& self) {
                        void* FunctionPointer = static_cast<void*>(self.get_match_address());

                        SignatureMap.emplace(ClassAndName, FunctionPointer);
                        PS::Log<LogLevel::Verbose>(STR("Found {}: {}\n"), RC::to_generic_string(ClassAndName), FunctionPointer);

                        self.get_did_succeed() = true;

                        return true;
                    },
                    [=](const SignatureContainer& self) {
                        if (!self.get_did_succeed())
                        {
                            PS::Log<RC::LogLevel::Error>(STR("Failed to find signature for {}.\n"), RC::to_generic_string(ClassAndName));
                        }
                    }
                };
            }();
            SigContainerBox.emplace_back(std::move(SigContainer));
        }

        for (auto& [ClassAndName, Signature] : SignaturesCallResolve)
        {
            SignatureContainer SigContainer = [=]() -> SignatureContainer {
                return {
                    {{Signature}},
                    [=](SignatureContainer& self) {
                        void* FunctionPointer = static_cast<void*>(self.get_match_address());
                        void* FinalAddress = ASM::resolve_call(FunctionPointer);

                        SignatureMap.emplace(ClassAndName, FinalAddress);
                        PS::Log<LogLevel::Verbose>(STR("Found {}: {}\n"), RC::to_generic_string(ClassAndName), FinalAddress);

                        self.get_did_succeed() = true;

                        return true;
                    },
                    [=](const SignatureContainer& self) {
                        if (!self.get_did_succeed())
                        {
                            PS::Log<RC::LogLevel::Error>(STR("Failed to find signature for {}.\n"), RC::to_generic_string(ClassAndName));
                        }
                    }
                };
            }();
            SigContainerBox.emplace_back(std::move(SigContainer));
        }

        SigContainerMap.emplace(ScanTarget::MainExe, std::move(SigContainerBox));
        SinglePassScanner::start_scan(SigContainerMap);
    }

    void* SignatureManager::GetSignature(const std::string& ClassAndFunction)
    {
        auto It = SignatureMap.find(ClassAndFunction);
        if (It != SignatureMap.end())
        {
            return It->second;
        }

        return nullptr;
    }
}
