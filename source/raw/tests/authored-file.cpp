#include "Runtime/AuthoredFile.h"
#include <cassert>
namespace {std::filesystem::path TestRoot;}
namespace PS::HostServices {std::filesystem::path ModDirectory(){return TestRoot;}}
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    using namespace PS::Authoring;using Json=nlohmann::json;
    TestRoot=std::filesystem::temp_directory_path()/("RuneSchemaAuthored-"+NewName());
    struct Cleanup{~Cleanup(){std::error_code ec;std::filesystem::remove_all(TestRoot,ec);}} cleanup;
    const auto name=NewName();const Json document={{"/Game/Item.Item",{{"$Clone","/Game/Source.Source"}}}};
    std::filesystem::path saved;
    {StagedFile asset("assets",name,document,"MyClones");saved=asset.Path();assert(saved.parent_path()==Directory("assets","MyClones"));assert(!std::filesystem::exists(saved));asset.Commit();}
    std::ifstream input(saved);assert(Json::parse(input)==document);input.close();
    assert(Rejects([&]{StagedFile duplicate("assets",name,Json::object(),"MyClones");duplicate.Commit();}));
    {std::ifstream original(saved);assert(Json::parse(original)==document);}
    {StagedFile raw("raw",name,{{"DT_Stats",{{"NewRow",{{"Power",5}}}}}},"MyClones");assert(raw.Path().parent_path()==Directory("raw","MyClones"));raw.Commit();}
    std::filesystem::path uncommitted;
    {StagedFile pending("assets",NewName(),document,"MyClones");uncommitted=pending.PendingPath();assert(std::filesystem::exists(uncommitted));}
    assert(!std::filesystem::exists(uncommitted));
    {StagedFile legacy("assets",NewName(),document);assert(legacy.Path().parent_path().parent_path().filename()=="runeschema");legacy.Commit();}
    assert(!std::filesystem::exists(TestRoot/"mods"/"runeschema.txt"));
    assert(Rejects([&]{StagedFile bad("assets",NewName(),document,"../Escape");}));
    const auto outside=TestRoot/"Outside";std::filesystem::create_directory(outside);
    std::error_code ec;std::filesystem::create_directory_symlink(outside,TestRoot/"mods"/"Escapes",ec);
    if(!ec)assert(Rejects([&]{StagedFile bad("assets",NewName(),document,"Escapes");}));
}
