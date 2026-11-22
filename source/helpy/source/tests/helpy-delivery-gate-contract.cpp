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
        const auto forward=At(tools,"if(!ForwardHelpyAuthority||!ForwardHelpyAuthority(action,request->dump()))",client);
        Check(clientGate<forward&&At(tools,"if(action==\"GiveItems\"||action==\"DestroyItem\"){",client)<clientGate,"A client must check its delivery flags before forwarding GiveItems or DestroyItem");
        Check(At(tools,"action==\"DestroyItem\")) {",client)<clientGate,"DestroyItem is forwarded to the server from a client");
        const auto clientOwn=At(tools,"PS::HelpyItemDestroy::Body(*request).Exact.Player!=PS::ItemSnapshot::NormalizeGuid(own)",client);
        Check(clientGate<clientOwn&&clientOwn<forward,"A client forwards Destroy only for its own character, after the gate");
        // Authority: checked after the authoritative-world gate and before any grant runs.
        const auto authority=At(tools,"Only the authoritative server may run spawn tools");
        const auto gate=At(tools,"PS::HelpyDeliveryGate::Require(GetPlayerCharacterGuid(selected),place);",authority);
        Check(gate<At(tools,"if(action==\"CreateClone\") {",authority)&&gate<At(tools,"if(action==\"GiveItems\") {",authority)&&gate<At(tools,"if(action==\"GiveItem\") {",authority),"The gate must run before every Helpy grant");
        const auto condition=tools.substr(tools.rfind("if(action==\"GiveItems\"||action==\"GiveItem\"||(action==\"CreateClone\"",gate),gate-tools.rfind("if(action==\"GiveItems\"||action==\"GiveItem\"||(action==\"CreateClone\"",gate));
        Check(condition.contains("\"HelpyNetwork\"")&&condition.contains("IsLocalController")&&condition.contains("catch(...){own=true;}"),"Only own-character deliveries are gated, and an unknown owner is treated as own");
        Check(condition.contains("WorldSaveIdentity::Guid(selected,\"Helpy delivery flags\")"),"World flags use the shared world-save GUID reader");
        // Helpy's Destroy button: own character gated before the native call; bounded result only.
        const auto destroy=At(tools,"if(action==\"DestroyItem\"){",authority);
        const auto destroyGate=At(tools,"PS::HelpyDeliveryGate::Require(body.Exact.Player,place);",destroy);
        const auto destroyCall=At(tools,"PS::ItemDestroyService::Call(",destroy);
        Check(destroyGate<destroyCall&&At(tools,"PS::HelpyItemDestroy::Body(*request);",destroy)<destroyGate,"Destroy checks confirmation and target, then the gate, before the native call");
        const auto destroyBlock=tools.substr(destroy,At(tools,"if(action==\"InspectPlayerInventory\"){",destroy)-destroy);
        Check(destroyBlock.contains("\"HelpyNetwork\"")&&destroyBlock.contains("IsLocalController")&&destroyBlock.contains("catch(...){own=true;}"),"Destroy gates the player's own character, and an unknown owner counts as own");
        Check(destroyBlock.contains("HelpyItemDestroy::Bounded(reply,body)")&&!destroyBlock.contains("{\"Snapshot\""),"Destroy publishes the bounded result, never the snapshot");
        // Server authority: saved access or allowlist, own character, before the generic access check.
        const auto network=At(tools,"std::string DragonWildsSpawnLoader::HandleNetworkHelpyAuthority(");
        const auto allow=At(tools,"PS::HelpyItemDestroy::Allow({true,savedAccess,allowlisted,pinAccess,true});",network);
        Check(allow<At(tools,"Remote access denied. Connect with the server's world PIN",network),"Destroy permission is decided before the PIN fallback");
        Check(At(tools,"if((pinAccess||savedAccess)&&!allowlisted&&action!=\"DestroyItem\"){",network)>allow,"Destroy skips the delivery-only self check, having checked its own target");
        Check(At(tools,"} else if(action==\"DestroyItem\"){",network)<At(tools,"if(!savedAccess&&!policy.allowClientTemporarySpawns)",network),"Destroy never falls into the temporary-spawn branch");
        const auto bridge=Read(root/"raw/src/Runtime/RegistryBridge.cpp");
        Check(bridge.contains("compact[\"DestroyResult\"]=result[\"DestroyResult\"]"),"The server returns the bounded Destroy result to the client");
        const auto inventory=Read(root/"helpy/source/include/Generator/PlayerInventoryPanel.h");
        Check(inventory.contains("HelpyItemDestroy::Build(")&&inventory.contains("ImGui::Checkbox(label.c_str(),&DestroyUnderstood)")&&inventory.contains("BeginDisabled(!DestroyUnderstood||expired"),"Destroy needs the menu item, the checkbox and the button");
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
