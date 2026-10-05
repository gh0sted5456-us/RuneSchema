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
    if(argc!=5)throw std::runtime_error("Expected RegistryBridge source/header, RegistryLoader source and networking PAK");
    const auto source=Read(argv[1]),header=Read(argv[2]),loader=Read(argv[3]),pak=Read(argv[4]);
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(std::string("Presentation transport regression: ")+message);
    };
    require(source.find("PluginPresentationClass")!=std::string::npos
        && source.find("MulticastRuneSchemaPluginPresentation")!=std::string::npos,
        "the cooked GameState presentation bridge is not used");
    require(source.find("Storefront::IsDedicatedServer()")<source.find("LoadClassAsset_Blocking"),
        "the dedicated-server guard does not precede consumer loading");
    require(source.find("AllowedKeys.contains")!=std::string::npos
        && source.find("contains an asset reference")!=std::string::npos,
        "payload keys and asset references are not validated");
    require(source.find("m_presentationAuthorityRevisions")!=std::string::npos
        && source.find("m_presentationClientRevisions")!=std::string::npos,
        "authority/client revision deduplication is missing");
    require(source.find("ServerAcknowledgeRuneSchemaPresentation")!=std::string::npos
        && source.find("ServerRequestRuneSchemaResync")!=std::string::npos,
        "acknowledgement or resync is missing");
    require(source.find("RuneSchemaPresentationState")!=std::string::npos
        && source.find("ReplayPresentationSnapshot")!=std::string::npos,
        "durable loop replay is missing");
    require(header.find("PublishPresentation")!=std::string::npos
        && header.find("PresentationRoute")!=std::string::npos,
        "public publish API or route contract is missing");
    require(loader.find("\"Consumer\"")!=std::string::npos
        && loader.find("\"AllowedKeys\"")!=std::string::npos
        && loader.find("MaxPresentationKeysPerConsumer=256")!=std::string::npos,
        "manifest consumer schema is missing or unbounded");
    require(pak.find("BPC_RuneSchemaPluginPresentation.uasset")!=std::string::npos
        &&pak.find("Content/Networking/Extensions/")!=std::string::npos,
        "the packaged networking bridge does not contain the presentation component");
}
