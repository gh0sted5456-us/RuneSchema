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
    require(source.find("at->Delay=0.5f;")!=std::string::npos,
        "failed authority attachment retries can hammer reflection every frame");
    require(source.find("const std::string BuildIdentity{PS::BuildInfo::Version};")!=std::string::npos,
        "numeric release versions must not lose their last character on the wire");
    require(source.find("SlowWork::Measure(\"network.attach\"")!=std::string::npos,
        "slow join attachment work has no phase attribution");
    require(source.find("CatchUpAuthorityWorld();")!=std::string::npos
        &&source.find("modes.Num()>64")!=std::string::npos
        &&source.find("Existing authority worlds are ambiguous")!=std::string::npos,
        "late dedicated startup can miss the existing world or select an arbitrary one");
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
    require(source.find("static const BridgeFunctionNames names;")!=std::string::npos
        &&source.find("static thread_local PS::WeakObjectHandle cached;")!=std::string::npos
        &&source.find("actual==cached.Get()")!=std::string::npos,
        "bridge names must be interned once and class caches must be serial validated");
    const auto method=[&](const std::string& name){const auto begin=source.find("void RegistryBridge::"+name+"(");require(begin!=std::string::npos,"observer is missing");const auto end=source.find("\nvoid RegistryBridge::",begin+1);return source.substr(begin,end-begin);};
    for(const auto* name:{"ObservePresentationPre","ObserveClientTransport","ObservePresentationTransport","Observe","ObserveAuthorityPre","ObserveAuthorityPost","ObserveConsumedItemAuthority","ObserveSelectionNotify","ObserveInventoryRemoval"}){
        const auto body=method(name);
        require(body.find("FNAME_Add")==std::string::npos,"global observer repeats a name-table lookup");
        require(body.find("RC::to_string(source->GetClassPrivate()->GetPathName())")==std::string::npos,"global observer allocates a class path before filtering");
        const auto filter=body.find("GetFName()");
        const auto expensive=body.find("ExactClass(")!=std::string::npos?body.find("ExactClass("):body.find("BridgeClassMatches<");
        require(filter!=std::string::npos&&(expensive==std::string::npos||filter<expensive),"name filtering must precede class resolution");
    }
    require(source.find("if(!Names().Pre(name)&&!AuthorityEntry(name)&&source!=m_activeAuthorityGraph)return;")!=std::string::npos
        &&source.find("if(!Names().Post(name)&&!AuthorityEntry(name))return;")!=std::string::npos,
        "unrelated ProcessEvent calls still reach the bridge observers");
    require(header.find("FName EntryName;")!=std::string::npos&&header.find("WeakObjectHandle GraphType;")!=std::string::npos
        &&source.find("if(!source||!AuthorityEntry(name))return nullptr;")<source.find("m_authorityGraphRoutes.find(type)")
        &&source.find("found->second.Type.Get()!=type")!=std::string::npos&&source.find("m_authorityGraphRoutes.size()>=1024")!=std::string::npos
        &&source.find("action.GraphClass==path")!=std::string::npos&&source.find("m_authorityGraphRoutes.clear();")!=std::string::npos,
        "graph routes must filter names first and use bounded, serial-validated class caches invalidated on manifest/world changes");
    require(source.find("identity.ObjectSerialNumber")!=std::string::npos
        &&source.find("m_modDataRelay.Receive(peer,payload,local.Result<bool>()")!=std::string::npos
        &&source.find("channel!=\"mod.data\"&&revision<=window.Revision")!=std::string::npos
        &&source.find("m_modDataRelay.Drain(4,")!=std::string::npos&&source.find("m_modDataRelay.Remove(at->first)")!=std::string::npos
        &&source.find("m_modDataRelay.Reset();m_relayPeers.clear();")!=std::string::npos,
        "relay native sender/authority, independent channel sequences, bounded drain or session cleanup is missing");
}
