#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=4)throw std::runtime_error("Expected SpawnTools.inl, DragonWildsSpawnLoader.cpp and SpawnToolsPanel.h");
    const auto tools=Read(argv[1]);
    const auto loader=Read(argv[2]);
    const auto panel=Read(argv[3]);
    const auto require=[](bool value,const char* what){if(!value)throw std::runtime_error(std::string("Building preview safety regression: ")+what);};

    require(tools.find("VerifyToolBuildingInstance")!=std::string::npos,"instance verifier missing");
    require(tools.find("actor->GetClassPrivate()!=actorClass")!=std::string::npos,"exact actor-class match missing");
    require(tools.find("RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient|RF_BeginDestroyed|RF_FinishDestroyed")!=std::string::npos,
        "native preflight does not exclude transient/default/destroying actors");
    require(tools.find("Saved building safety is stale; rebuild catalogs in the current world")!=std::string::npos,
        "saved building safety is being trusted across worlds");
    require(tools.find("if(request->contains(\"Grid\"))throw std::runtime_error(\"Building preview grids are disabled")!=std::string::npos,
        "building grids are not blocked");
    require(tools.find("if(count!=1")!=std::string::npos,"building previews are not single-shot");

    const auto configure=tools.find("value->SetFlags(RF_Transient)");
    const auto setSkip=tools.find("skip->SetPropertyValue",configure);
    const auto applyData=tools.find("ApplyBuildingData(value,prototype)",configure);
    const auto postVerify=tools.find("VerifyToolBuildingInstance(actor,buildingObject,prototype,world,true)",configure);
    require(configure!=std::string::npos && setSkip!=std::string::npos && applyData!=std::string::npos && postVerify!=std::string::npos,
        "spawn safety sequence incomplete");
    require(configure<setSkip && setSkip<applyData && applyData<postVerify,
        "spawn safety order changed (transient/save exclusion/binding/post-check)");

    require(loader.find("!CastField<FObjectProperty>(property) && !CastField<FSoftObjectProperty>(property)")!=std::string::npos,
        "RegisterBuildingProp still accepts unsupported object-reference types");
    require(panel.find("ImGui::BeginDisabled(!safe)")!=std::string::npos,"unsafe catalog rows are selectable");
    require(panel.find("Safety mode: one building preview per spawn")!=std::string::npos,"single-preview safety UI missing");
    require(panel.find("{\"Count\",1}")!=std::string::npos,"UI no longer submits Count=1");

    std::cout << "PASS: building preview catalog and spawn path remain fail-closed, single-shot, transient and post-verified.\n";
}
