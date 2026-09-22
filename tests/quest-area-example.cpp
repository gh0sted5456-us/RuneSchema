#include "Loader/QuestDefinition.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/NpcCatalog.h"
#include <cassert>
#include <filesystem>
#include <fstream>
int main(int argc,char** argv) {
    assert(argc==3);
    const std::filesystem::path areaRoot(argv[1]),pointRoot(argv[2]);
    const auto read=[](const std::filesystem::path& path){std::ifstream stream(path);assert(stream.good());return nlohmann::json::parse(stream);};
    const std::string mod="RuneSchema2VendorTest";
    const auto area=read(areaRoot/"quests/30-CabbageArea.json");
    const auto point=read(pointRoot/"quests/20-MarkerSupper.json");
    DragonWilds::Quests::Catalog quests;quests.Add(mod,area);quests.Add(mod,point);
    const auto& quest=quests.Find(mod,"cabbage_area");
    assert(quest.Marker && quest.Marker->RadiusMeters==75);
    assert(DragonWilds::Quests::RadiusCentimeters(*quest.Marker->RadiusMeters)==7500);
    assert(quest.PersistenceId!=quests.Find(mod,"marker_supper").PersistenceId);
    const auto dialogue=DragonWilds::Dialogue::Parse(mod,read(areaRoot/"dialogue/40-MarkerSupper.json"));
    assert(dialogue.Quests.size()==2);
    for(const auto& reference:dialogue.Quests)quests.Find(mod,reference);
    const auto npc=read(areaRoot/"npc/40-MarkerSurveyor.json");
    const auto priorNpc=read(pointRoot/"npc/40-MarkerSurveyor.json");
    DragonWilds::NpcCatalog catalog;catalog.AddNpc(mod,npc);
    assert(npc["Id"]==priorNpc["Id"] && npc["Location"]==priorNpc["Location"]);
    assert(DragonWilds::Dialogue::Reference(mod,npc.at("DialogueID"))==dialogue.Key);
    assert(npc["Map"]["Enabled"]==true && npc["Map"]["ShowName"]==false);
}
