#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
// The core grants items in files that build only with the game SDK, so this checks that every
// player-driven grant path calls the delivery gate before the grant, and that host deliveries
// to other players stay outside it.
static std::string Read(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Missing delivery gate contract input: "+path.string());return {std::istreambuf_iterator<char>(input),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static size_t At(const std::string& text,const std::string& needle,size_t from=0){const auto found=text.find(needle,from);if(found==text.npos)throw std::runtime_error("Missing: "+needle);return found;}
int main(int argc,char** argv){
    try{
        Check(argc==2,"Expected source directory");const auto root=std::filesystem::path(argv[1]);
        const auto tools=Read(root/"raw/src/Loader/SpawnTools.inl");
        // Client: checked before the request is forwarded to the server.
        const auto client=At(tools,"if(!selected&&catalogWorld&&PS::Network::Detect(catalogWorld).Mode==PS::Network::Role::Client");
        const auto clientGate=At(tools,"PS::HelpyDeliveryGate::Require(own,\"\");",client);
        Check(clientGate<At(tools,"if(!ForwardHelpyAuthority||!ForwardHelpyAuthority(action,request->dump()))",client)&&At(tools,"if(action==\"GiveItems\"){",client)<clientGate,"A client must check its delivery flags before forwarding GiveItems");
        // Authority: checked after the authoritative-world gate and before any grant runs.
        const auto authority=At(tools,"Only the authoritative server may run spawn tools");
        const auto gate=At(tools,"PS::HelpyDeliveryGate::Require(GetPlayerCharacterGuid(selected),place);",authority);
        Check(gate<At(tools,"if(action==\"CreateClone\") {",authority)&&gate<At(tools,"if(action==\"GiveItems\") {",authority)&&gate<At(tools,"if(action==\"GiveItem\") {",authority),"The gate must run before every Helpy grant");
        const auto condition=tools.substr(tools.rfind("if(action==\"GiveItems\"||action==\"GiveItem\"||(action==\"CreateClone\"",gate),gate-tools.rfind("if(action==\"GiveItems\"||action==\"GiveItem\"||(action==\"CreateClone\"",gate));
        Check(condition.contains("\"HelpyNetwork\"")&&condition.contains("IsLocalController")&&condition.contains("catch(...){own=true;}"),"Only own-character deliveries are gated, and an unknown owner is treated as own");
        Check(condition.contains("WorldSaveIdentity::Guid(selected,\"Helpy delivery flags\")"),"World flags use the shared world-save GUID reader");
        // Offline save editing that adds items is a delivery too.
        const auto saves=Read(root/"raw/src/Generator/CharacterSaveEditor.inl");
        const auto add=At(saves,"if(op==\"SaveEditorAddItem\"){");
        Check(At(saves,"HelpyDeliveryGate::Require(characterEditor.Session.CharacterGuid(),\"\");",add)<At(saves,"characterEditor.Session.AddRecord(",add),"Adding a save-editor item must check the gate first");
        const auto stack=At(saves,"if(op==\"SaveEditorStack\"){");
        Check(At(saves,"HelpyDeliveryGate::Require(characterEditor.Session.CharacterGuid(),\"\");",stack)<At(saves,"characterEditor.Session.SetCount(",stack),"Increasing a save-editor stack must check the gate first");
        // NPC equipment and the gate share one world-save GUID reader.
        const auto npc=Read(root/"raw/src/Loader/NpcEquipment.inl");
        Check(npc.contains("WorldSaveIdentity::Guid(context,\"NPC equipment\")")&&!npc.contains("WorldSaveSettings"),"NPC equipment must use the shared world-save GUID reader");
        const auto gateHeader=Read(root/"raw/include/Runtime/HelpyDeliveryGate.h");
        Check(gateHeader.contains("SharedDirectory(\"HELPY\")/\"flags\"")&&gateHeader.contains("AnyCharacterPause"),"The gate reads Helpy's shared flags folder and refuses unknown characters only when a pause exists");
        const auto panel=Read(root/"helpy/source/include/Generator/DeliveryFlagsPanel.h");
        Check(panel.contains("SharedDirectory(\"HELPY\")/\"flags\""),"Helpy's Deliveries settings read the same folder as the core");
        Check(panel.contains("\"Allowed until \"")&&panel.contains("Snapshot.Lifted"),"Settings > Deliveries shows temporary lifts as Allowed until {date} by {mod}");
        // Temporary lifts end by the clock, so the gate must read flags at the moment of each delivery.
        Check(gateHeader.contains("system_clock::now()")&&gateHeader.contains("HelpyDeliveryFlags::Read(Folder(),HelpyDeliveryFlags::Signature::DefaultVerifier(),Now())"),"The gate evaluates temporary lifts against the current time on every delivery");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
