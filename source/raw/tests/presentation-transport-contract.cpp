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
    if(argc!=8)throw std::runtime_error("Expected RegistryBridge source/header, RegistryLoader source, networking PAK, ActorHelper source/header and networking UTOC");
    const auto source=Read(argv[1]),header=Read(argv[2]),loader=Read(argv[3]),pak=Read(argv[4]);
    const auto actorHeader=Read(argv[5]),actorSource=Read(argv[6]);
    const auto toc=Read(argv[7]);
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
    require(header.find("m_reportedMissingPresentationRoutes")!=std::string::npos
        && source.find("emitted mod='{}' connection='{}' entity='{}' action='{}'")!=std::string::npos
        && source.find("local connections for mod={}")!=std::string::npos
        && source.find("local owners for connection={}")!=std::string::npos
        && source.find("m_reportedMissingPresentationRoutes.emplace(diagnosticKey).second")!=std::string::npos
        && source.find("RecordDiagnostic(pluginId+\".\"+connection,entityId,revision,\"rejected\",detail)")!=std::string::npos,
        "missing client routes are not attributed or deduplicated");
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
    require((pak.find("BPC_RuneSchemaPluginPresentation.uasset")!=std::string::npos
        &&pak.find("Content/Networking/Extensions/")!=std::string::npos)
        ||(toc.find("BPC_RuneSchemaPluginPresentation.uasset")!=std::string::npos&&toc.find("Extensions")!=std::string::npos),
        "the packaged networking bridge does not contain the presentation component");
    require(actorHeader.find("Arg(const RC::CharType* Name, const std::string& Value)")!=std::string::npos
        &&actorHeader.find("return StringArg(Name, Value);")!=std::string::npos,
        "reflected Unreal string arguments can fall through to raw byte copying");
    const auto stringStart=actorSource.find("FunctionCall& FunctionCall::StringArg(");
    const auto stringEnd=actorSource.find("FunctionCall& FunctionCall::SoftObjectArg(",stringStart);
    require(stringStart!=std::string::npos&&stringEnd!=std::string::npos,
        "the reflected FString argument adapter is missing");
    const auto stringArg=actorSource.substr(stringStart,stringEnd-stringStart);
    require(stringArg.find("CastField<FStrProperty>")!=std::string::npos
        &&stringArg.find("ReflectedContract::Input(property,m_function)")!=std::string::npos
        &&stringArg.find("GetArrayDim()!=1")!=std::string::npos
        &&stringArg.find("GetElementSize()!=sizeof(FString)")!=std::string::npos,
        "reflected FString arguments do not validate their native parameter layout");
    require(stringArg.find("InitializeValue_InContainer(m_params.data())")!=std::string::npos
        &&stringArg.find("m_initialized.push_back(property)")!=std::string::npos
        &&stringArg.find("SetPropertyValue(")!=std::string::npos
        &&actorSource.find("DestroyValue_InContainer(m_params.data())")!=std::string::npos,
        "reflected FString arguments do not retain native initialization and cleanup");
}
