#include "Core/JsonDocument.h"
#include "Utility/StructuredDiagnosticB3.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using PS::StructuredDiagnostic::Record;
static std::vector<Record> reports;
namespace PS::StructuredDiagnostics {
bool Emit(Record value) noexcept {
    value.EventId="e1";value.SessionId="s1";value.Timestamp="2026-10-21T08:00:00Z";
    if(!StructuredDiagnostic::Validate(value))return false;
    reports.push_back(std::move(value));return true;
}
}
static void Receiver(const char* code,const std::filesystem::path& path,
        const std::string& field,const std::string& detail) noexcept {
    auto site=PS::StructuredDiagnosticB3::FromPath(path);
    if(std::string_view(code)=="RS-JSON-005")site.File.clear();
    const bool fieldFailure=std::string_view(code)=="RS-JSON-002"||std::string_view(code)=="RS-JSON-003";
    assert(PS::StructuredDiagnosticB3::Report(code,"Error",fieldFailure?"Blocked":"Failed",
        "JsonParser","JSON definition problem",detail,site,fieldFailure?"Validation":"Parsing",field));
}
int main(){
    namespace fs=std::filesystem;
    PS::JsonHelpers::SetDiagnosticReporter(&Receiver);
    const auto root=fs::temp_directory_path()/"rs_b3_json_test";
    fs::remove_all(root);
    const auto directory=root/"mods"/"ModTest"/"raw";
    fs::create_directories(directory);
    const auto bad=directory/"bad.jsonc";
    {std::ofstream out(bad);out<<"{ \"broken\": [ 1,  }";}
    bool failed=false;
    try{PS::JsonHelpers::ParseJsonFileInPath(bad,[](const auto&){});}catch(const std::exception&){failed=true;}
    assert(failed&&reports.size()==1&&reports.back().Code=="RS-JSON-001");
    assert(reports.back().Context["ModId"]=="ModTest"&&reports.back().Context["File"]=="raw/bad.jsonc");
    reports.clear();
    const auto valid=directory/"valid.jsonc";
    {std::ofstream out(valid);out<<"{\"value\":\"wrong\"}";}
    failed=false;
    try{PS::JsonHelpers::ParseJsonFileInPath(valid,[](const auto& data){
        PS::JsonHelpers::ValidateFieldExists(data,"missing");
    });}catch(const std::exception&){failed=true;}
    assert(failed&&reports.size()==1&&reports[0].Code=="RS-JSON-002"&&reports[0].Context["Property"]=="missing");
    reports.clear();
    try{PS::JsonHelpers::ParseJsonFileInPath(valid,[](const auto& data){
        int number=0;PS::JsonHelpers::ParseInteger(data,"value",number);
    });}catch(const std::exception&){failed=true;}
    assert(reports.size()==1&&reports[0].Code=="RS-JSON-003");
    reports.clear();
    const auto huge=directory/"huge.json";
    {std::ofstream out(huge,std::ios::binary);out<<std::string(2*1024*1024+1,'x');}
    failed=false;
    try{PS::JsonHelpers::ParseJsonFileInPath(huge,[](const auto&){});}catch(const std::exception&){failed=true;}
    assert(failed&&reports.size()==1&&reports[0].Code=="RS-JSON-004");
    reports.clear();
    const auto& origin=PS::StructuredDiagnosticB3::FromPath(directory/"nested"/"patch.jsonc");
    assert(origin.ModId=="ModTest"&&origin.File=="raw/nested/patch.jsonc");
    const auto windowsCase=PS::StructuredDiagnosticB3::FromPath(fs::path("C:/Game/MODS/CaseMod/ASSETS/FX/Glow.jsonc"));
    assert(windowsCase.ModId=="CaseMod"&&windowsCase.File=="ASSETS/FX/Glow.jsonc");
    const auto fallback=PS::StructuredDiagnosticB3::FromPath(fs::path("C:/NotMods/MyMod/RECIPES/New.jsonc"),"recipes");
    assert(fallback.ModId.empty()&&fallback.File=="New.jsonc");
    const PS::StructuredDiagnosticB2::Provenance raw{"ModTest","raw/test.jsonc","/Game/Test:NoSuchRow","raw-1"};
    assert(PS::StructuredDiagnosticB3::Report("RS-RAW-001","Error","Failed", "RawTableLoader",
        "Missing row","The row was not found",raw,"Application"));
    assert(reports.back().Context["Target"]=="/Game/Test:NoSuchRow");
    assert(PS::StructuredDiagnosticB3::Report("RS-REC-002","Error","Skipped", "RecipeLoader",
        "Invalid recipe","Recipe body is not an object",raw,"Validation"));
    assert(PS::StructuredDiagnostic::Encode(reports.back()).has_value());
    reports.clear();
    const auto many=root/"mods"/"ModTest"/"recipes";
    fs::create_directories(many);
    for(int index=0;index<4097;++index){
        std::ofstream out(many/(std::to_string(index)+".json"));out<<"{}";
    }
    failed=false;
    try{PS::JsonHelpers::ParseJsonFilesInPath(many,[](const auto&){});}catch(const std::exception&){failed=true;}
    assert(failed&&reports.size()==1&&reports[0].Code=="RS-JSON-005");
    PS::JsonHelpers::SetDiagnosticReporter(nullptr);
    fs::remove_all(root);
}
