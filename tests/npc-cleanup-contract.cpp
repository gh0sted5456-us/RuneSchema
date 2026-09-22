#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("NPC loader and cleanup sources required");
    const auto read=[](const char* path){
        std::ifstream file(path);if(!file)throw std::runtime_error("Source unavailable");
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto loader=read(argv[1]),cleanup=read(argv[2]);
    const auto require=[](bool value){if(!value)throw std::runtime_error("NPC cleanup contract regression");};
    require(loader.find("actor=pending;")!=loader.npos);
    require(loader.find("QueueNpcCleanup(actor);")!=loader.npos);
    require(loader.find("PumpNpcCleanup(deltaSeconds);")<loader.find("if (m_definitions.empty() || m_scanBudget.Exhausted()) return;"));
    require(cleanup.find("m_pendingNpcCleanup.empty()")!=cleanup.npos);
    require(cleanup.find("slot->GetUObject()!=actor")<cleanup.find("IsNpcObjectUsable(actor)"));
    require(cleanup.find("actor->SetRootSet()")!=cleanup.npos);
    require(cleanup.find("if(it->AddedRoot)actor->ClearRootSet()")!=cleanup.npos);
    const auto destroy=cleanup.find("ActorHelper::DestroyActor(actor)");
    require(destroy!=cleanup.npos && cleanup.find("if(IsNpcObjectUsable(actor)){++it;continue;}",destroy)!=cleanup.npos);
    for(const auto* name:{"SetActorEnableCollision","SetActorHiddenInGame","InteractionComponent","ChildActorComponent","BillboardComponent","MapIconComponent"})
        require(cleanup.find(name)!=cleanup.npos);
    const auto release=loader.find("void DragonWildsNpcLoader::ReleaseVendorTracking()");
    const auto next=loader.find("void DragonWildsNpcLoader::OnVendorCellShown",release);
    require(release!=loader.npos && next!=loader.npos);
    require(loader.substr(release,next-release).find("m_pendingNpcCleanup.clear()")==loader.npos);
}
