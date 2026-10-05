#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("Expected RegistryBridge source/header, RegistryLoader source and summoning guide");
    const auto source=Read(argv[1]),header=Read(argv[2]),loader=Read(argv[3]),guide=Read(argv[4]);
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(std::string("Summoning authority regression: ")+message);
    };
    require(loader.find("GameplayAuthority")!=std::string::npos
        &&loader.find("ConsumedItemAuthority")!=std::string::npos,
        "the registry schema does not expose the consumed-follower action");
    require(loader.find("LoadCookedRegistries")!=std::string::npos
        &&loader.find("RuneSchemaRegistryJson")!=std::string::npos,
        "self-contained cooked registry discovery is unavailable");
    require(header.find("ConsumptionPermit")!=std::string::npos
        &&header.find("PendingAuthorityAction")!=std::string::npos
        &&header.find("PendingClientAction")!=std::string::npos,
        "consumption correlation queues are missing");
    require(source.find("/Script/Dominion.InventoryComponent:RemoveItemByData")!=std::string::npos
        &&source.find("ConsumeAuthorityPermit")!=std::string::npos,
        "authority requests are not tied to an actual server-side item removal");
    require(source.find("InvokeConsumedItemAuthority")!=std::string::npos
        &&source.find("ItemGameplayTags")!=std::string::npos
        &&source.find("GameplayTagContainer")!=std::string::npos,
        "the existing cooked consume callback is not replayed with its native contract");
    require(source.find("action.DataAsset")!=std::string::npos
        &&source.find("item->IsA(graphType)")!=std::string::npos,
        "the server does not validate the registered item and callback class");
    require(guide.find("DA_RuneSchemaRegistry")!=std::string::npos
        &&guide.find("No loose registry file is required")!=std::string::npos,
        "the self-contained cooked-mod packaging path is undocumented");
}
