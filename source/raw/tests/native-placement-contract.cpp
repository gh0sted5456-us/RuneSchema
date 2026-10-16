#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("Native contract input missing");return {std::istreambuf_iterator<char>(f),{}};}
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    Require(argc==2,"Expected native source root");const std::filesystem::path root(argv[1]);
    const auto contract=Read(root/"include/SDK/Helper/ReflectedContract.h");
    Require(contract.find("FText::StaticSize()")!=std::string::npos&&contract.find("property->GetElementSize()==liveSize")!=std::string::npos,"Text validation does not use the running engine extent");
    for(const auto* relative:{"src/Loader/DragonWildsNpcLoader.cpp","src/Loader/NpcQuestPresentation.inl","src/Generator/AssetSearch.cpp","src/Generator/SaveViewer.cpp"}){
        const auto source=Read(root/relative);
        Require(source.find("sizeof(FText)")==std::string::npos,"A consumer still compares native FText to SDK wrapper capacity");
        Require(source.find("ReflectedContract::Text(")!=std::string::npos,"A text consumer bypasses the shared native extent validator");
    }
    const auto npc=Read(root/"src/Loader/DragonWildsNpcLoader.cpp");
    const auto start=npc.find("UObject* DragonWildsNpcLoader::EnsureComponent(");
    const auto end=npc.find("nlohmann::json DragonWildsNpcLoader::VisibleVendorItems",start);
    Require(start!=std::string::npos&&end!=std::string::npos,"NPC component contract not found");
    const auto component=npc.substr(start,end-start);
    for(const auto* expected:{"componentClass->IsChildOf(base)","ActorHelper::IsAbstract(componentClass)","component->GetOuterPrivate()==actor","component->GetWorld()==actor->GetWorld()","if(existing&&existing!=component)","/Script/Engine.Actor:AddComponentByClass","if(!listed)","componentClass->GetPathName()"})
        Require(component.find(expected)!=std::string::npos,"Native NPC component validation regressed");
    Require(component.find("ConstructTransientObject")==std::string::npos&&component.find("StaticConstructObject")==std::string::npos,"NPC components bypass native engine ownership");
    const auto header=Read(root/"include/SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h");
    const auto cell=Read(root/"src/SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.cpp");
    Require(header.find("Offset =")==std::string::npos&&cell.find("ptr_cast")==std::string::npos,"World-cell readiness still dereferences fixed native offsets");
    for(const auto* field:{"ContentBounds","bIsHLOD","RuntimeCellData"})Require(cell.find(field)!=std::string::npos,"World-cell reflection field missing");
    Require(cell.find("GetPropertyValue(")!=std::string::npos&&cell.find("std::isfinite")!=std::string::npos,"World-cell boolean or finite bounds validation missing");
    Require(header.find("std::optional<FBox> GetContentBounds()")!=std::string::npos,
        "Cell bounds must be decoded into owned storage, not alias engine padding");
    Require(cell.find("sizeof(FBox)")==std::string::npos&&cell.find("ContainerPtrToValuePtr<FBox>")==std::string::npos,
        "Engine Box extent still depends on the differently aligned SDK wrapper");
    for(const auto* field:{"TEXT(\"Min\")","TEXT(\"Max\")","TEXT(\"IsValid\")"})
        Require(cell.find(field)!=std::string::npos,"Native Box fields must use reflected offsets");
    Require(cell.find("Fits(vector,extent)")!=std::string::npos&&cell.find("Fits(valid,extent)")!=std::string::npos,
        "Decoded bounds bypass reflected extent checks");
    Require(npc.find("x >= bounds->Min.X() && x <= bounds->Max.X()")!=std::string::npos,
        "NPC placements exactly on a visible-cell edge never activate");
}
