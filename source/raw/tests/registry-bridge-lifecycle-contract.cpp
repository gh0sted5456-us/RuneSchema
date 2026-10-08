#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path);
    if(!file) throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=3) throw std::runtime_error("Expected RegistryBridge source and header");
    const auto source=Read(argv[1]),header=Read(argv[2]);
    const auto require=[](bool value,const char* message) {
        if(!value) throw std::runtime_error(std::string("Registry bridge lifecycle regression: ")+message);
    };
    require(header.find("PS::WeakObjectHandle m_pendingMode")!=std::string::npos,
        "pending GameMode is not serial validated");
    require(source.find("m_pendingMode.Assign(mode)")!=std::string::npos
        && source.find("m_pendingMode.Get()")!=std::string::npos,
        "world attachment does not use the weak pending GameMode");
    require(source.find("StartRetryTick();\n    m_started=")!=std::string::npos,
        "the single retry callback is not installed during process startup");
    require(source.find("IsGameplayRoleWorld(RC::to_string(world->GetPathName()))")!=std::string::npos,
        "front-end GameModes can still schedule a registry bridge attachment");
    require(source.find("iteration.RemoveSelf()") == std::string::npos,
        "the retry callback still removes and re-registers itself during world travel");
    require(source.find("m_retryTick=Hook::ERROR_ID;iteration") == std::string::npos,
        "callback identity is cleared from inside its own invocation");
    require(header.find("m_sessionRegistryOwners")!=std::string::npos
        && header.find("m_sessionNetworkOwners")!=std::string::npos
        && header.find("m_sessionAuthorityFingerprint")!=std::string::npos,
        "connection-scoped authority manifest state is missing");
    require(source.find("server-baseline-permissive")!=std::string::npos
        && source.find("clientExtras\",\"allowed")!=std::string::npos
        && source.find("missingContent\",\"allowed")!=std::string::npos
        && source.find("actionValidation\",\"independent")!=std::string::npos,
        "the server does not publish the permissive session policy");
    require(source.find("SendReceipt(source,channel,entity,revision,protocolMatch,detail)")!=std::string::npos
        && source.find("protocolMatch&&buildMatch&&registryMatch")==std::string::npos,
        "manifest or build differences can still reject a compatible client");
    require(source.find("client extras remain active, unavailable local content is permitted")!=std::string::npos
        && source.find("m_sessionRegistryOwners.clear();m_sessionNetworkOwners.clear();m_sessionAuthorityFingerprint.clear()")!=std::string::npos,
        "the permissive session is not announced once or cleared on world reset");
}
