#include "Core/PersistenceDiagnosticLedger.h"
#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/
        ("runeschema-persistence-ledger-"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto file=root/"mods"/"Example"/"recipes"/"recipe.json";
        PS::ConfigFiles::Write(file,R"({"Recipe":{"PersistenceID":"AAAAAAAAAAAAAAAAAAAAAA","InternalName":"REC_Test"}})");
        const auto output=root/"out"/"PersistenceLedger.json";
        PS::PersistenceDiagnostics::Write(root/"mods",{L"Example"},output);
        const auto document=nlohmann::json::parse(PS::ConfigFiles::Read(output));
        if(document.value("Kind",std::string{})!="RuneSchemaPersistenceDiagnosticLedger"
            || document.at("Records").size()!=1
            || document.at("Records").at(0).value("Owner",std::string{})!="Example"
            || document.at("Records").at(0).value("Kind",std::string{})!="Recipe"
            || document.at("Records").at(0).value("InternalName",std::string{})!="REC_Test"
            || document.value("Authority",std::string{}).find("never consulted")==std::string::npos)
            throw std::runtime_error("diagnostic persistence ledger contract failed");
        fs::remove_all(root);
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        std::error_code ignored;fs::remove_all(root,ignored);
        return 1;
    }
}
