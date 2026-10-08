#include "Core/JsonDocument.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    using namespace PS::JsonHelpers;
    namespace fs = std::filesystem;
    auto check=[](bool value){if(!value)throw std::runtime_error("JSON document contract failed");};
    auto rejects=[&](auto action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected);};
    nlohmann::json values={{"real",2.5},{"whole",7},{"text","rune"}};
    double real=0;int whole=0;std::string text;
    ParseDouble(values,"real",real);ParseInteger(values,"whole",whole);ParseString(values,"text",text);
    check(real==2.5 && whole==7 && text=="rune");
    rejects([&]{ParseInteger(values,"real",whole);});
    rejects([&]{ParseString(values,"whole",text);});
    rejects([&]{ValidateFieldExists(values,"missing");});
    check(FieldExists(values,"text") && !FieldExists(values,"missing"));
    const auto folder=fs::temp_directory_path()/("runeschema-json-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(folder);
    struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{folder};
    std::ofstream(folder/"b.json")<<R"({"order":2})";
    std::ofstream(folder/"a.jsonc")<<"// comment\n{\"order\":1}";
    std::ofstream(folder/"ignored.txt")<<"invalid";
    fs::create_directory(folder/"nested");std::ofstream(folder/"nested"/"c.json")<<R"({"order":3})";
    std::vector<int> visited;
    ParseJsonFilesInPath(folder,[&](const auto& document){visited.push_back(document.at("order").template get<int>());});
    check(visited==std::vector<int>({1,2,3}));
    ParseJsonFileInPath(folder/"missing.json",[&](const auto&){throw std::runtime_error("Missing file visited");});
    std::ofstream(folder/"broken.json")<<"{";
    bool contextual=false;
    try{ParseJsonFilesInPath(folder,[](const auto&){});}catch(const std::exception& error){contextual=std::string(error.what()).find("broken.json")!=std::string::npos;}
    check(contextual);
    std::vector<int> isolated;std::vector<std::string> failures;
    ParseJsonFilesInPathIsolated(folder,
        [&](const auto& document){isolated.push_back(document.at("order").template get<int>());},
        [&](const auto& path,const auto& error){failures.push_back(path.filename().string()+":"+error);});
    check(isolated==std::vector<int>({1,2,3}) && failures.size()==1
        && failures[0].find("broken.json")!=std::string::npos);
    std::vector<std::string> sources;
    ParseJsonFilesInPathWithSourceIsolated(folder,
        [&](const auto& document,const auto& source){
            if(document.contains("order"))sources.push_back(source.generic_string());
        },[](const auto&,const auto&){});
    check(sources==std::vector<std::string>({"a.jsonc","b.json","nested/c.json"}));
    std::cout<<"JSON document contracts passed\n";
}
