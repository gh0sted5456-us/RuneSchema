#include "Generator/DiagnosticLibrary.h"
#include <chrono>
#include <fstream>
using namespace PS::DiagnosticLibrary;
void Require(bool b){if(!b)throw std::runtime_error("Diagnostic library regression");}
int main() {
    const auto root=std::filesystem::temp_directory_path()/("RuneSchemaDiagnosticTest-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    struct Cleanup {std::filesystem::path Root;~Cleanup(){std::error_code error;std::filesystem::remove_all(Root,error);}} cleanup{root};
    const auto write=[&](const char* path,const char* text){std::ofstream stream(root/path);stream<<text;Require(stream.good());};
    write("a.json","{\"Name\":\"Same\"}");write("b.jsonc","{\"Name\":\"Same\"}");
    write("c.json","invalid");write("d.json","{\"Name\":\"Last\"}");write("notes.txt","ignored");
    const auto parse=[](const std::string& text){return Json::parse(text);};
    const auto result=Load(root,{10,20,512},parse);
    Require(result.Documents.size()==2 && result.Rejected==2 && !result.Truncated);
    Require(result.Documents[0].at("Name")=="Last" && result.Errors[0].find("Duplicate")!=std::string::npos);
    Require(Load(root,{10,2,512},parse).Documents.empty());
    Require(Load(root,{1,20,512},parse).Truncated);
    Require(Load(root,{10,20,2},parse).Rejected==4);
}
