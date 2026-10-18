#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
static std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Networking contract input unavailable");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    Check(argc==7,"Expected authoring, public header, package script, receipt and authoring mirrors");
    const auto authoring=Read(argv[1]),header=Read(argv[2]),packaging=Read(argv[3]);
    Check(authoring==Read(argv[5])&&header==Read(argv[6]),"Authoring templates differ from the compiled module");
    Check(authoring.contains("URuneSchemaBridgeAuthoringLibrary::AuthorNetworkBridges()")
        &&authoring.contains("if (IsRunningCommandlet()) return;")
        &&header.contains("static bool AuthorNetworkBridges();"),"Commandlet authoring must be explicit, not a delayed ticker");
    Check(authoring.contains("EnsureVariable(Identity,TEXT(\"IdentityPayload\")")
        &&authoring.contains("Payload->HasAllPropertyFlags(CPF_Net | CPF_RepNotify)")
        &&authoring.contains("!VerifyIdentityContract()"),"Identity authoring must enforce the actual replicated String contract");
    Check(packaging.contains("$networkContentLockPath")&&packaging.contains("$networkContainerNames")
        &&packaging.contains("(Get-Sha256Hex $contentFile) -ne $container.SHA256")
        &&packaging.contains("RSNetworking\\CONTENT-RECEIPT.json"),"Networking package must check complete pinned containers");
    const auto receipt=nlohmann::json::parse(Read(argv[4]));const auto& identity=receipt.at("IdentityContract");
    Check(identity.at("Field")=="IdentityPayload"&&identity.at("Type")=="String"
        &&identity.at("Bytes")==16&&identity.at("ArrayDim")==1&&identity.at("Net")==true
        &&identity.at("RepNotify")==true&&identity.at("Callback")=="OnRep_IdentityPayload","Receipt cannot weaken replication validation");
    Check(receipt.at("Containers").size()==3,"A complete PAK/IoStore receipt is required");
}
