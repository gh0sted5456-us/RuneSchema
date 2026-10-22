#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    Need(argc==2,"source/raw root required");const std::filesystem::path root=argv[1];
    const auto read=[](const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);Need(bool(stream),"contract source unavailable");return std::string(std::istreambuf_iterator<char>(stream),{});};
    const auto ui=read(root/"../helpy/source/include/Generator/RuneSchemaSettingsPanel.h");
    Need(ui.find("ImGui::BeginCombo(\"Launch mode\"")!=ui.npos&&ui.find("ImGui::Selectable(\"Normal\"")!=ui.npos&&ui.find("ImGui::Selectable(\"Quick Launch\"")!=ui.npos,"launch selector must offer Normal and Quick Launch");
    Need(ui.find("ImGui::TreeNode(\"Advanced\")")<ui.find("ImGui::Checkbox(\"Vanilla Load\""),"Vanilla Load belongs under Advanced");
    Need(ui.find("QuickToggle(\"Save Bypass\",\"/persistence/allowCorruptCharacterEntry\",true)")!=ui.npos,"Save Bypass remains an independent confirmed toggle");
    const auto quick=read(root/"src/Runtime/QuickLoad.cpp");
    const auto save=quick.substr(quick.find("Json Save(const Json& request)"));
    const auto configuration=read(root/"include/Core/QuickLaunchConfiguration.h");
    Need(save.find("QuickLaunchConfiguration::Save")!=save.npos&&configuration.find("allowCorruptCharacterEntry")==configuration.npos&&configuration.find("/persistence/")==configuration.npos&&quick.find("allowCorruptCharacterEntry")==quick.npos,"Quick Launch must never change or require Save Bypass");
    const auto editor=read(root/"include/Core/RuneSchemaSettingsEditor.h");
    Need(configuration.find("ExpectedContents")!=configuration.npos&&editor.find("ConfigFiles::Backup")<editor.find("ConfigFiles::Write"),"launch save requires stale-write protection and an original backup");
    Need(quick.find("NativePersistenceRecovery::EnsureSessionPolicy(controller)")<quick.find("Character(controller,selected.CharacterName,true)"),"session soft loading must cover native character selection");
    Need(quick.find("tolerantRequested")==quick.npos&&quick.find("NativePersistenceRecovery::ReleaseSessionPolicy")!=quick.npos,"Quick Launch finish or cancellation must not restore session strictness");
    Need(quick.find("void SuppressLoadingScreen(void*,const void*){}")!=quick.npos&&quick.find("Fade(controller,1.f)")!=quick.npos&&quick.find("Fade(controller,0.f)")!=quick.npos,"native loading-screen skip and both fade directions must remain");
    Need(quick.find("if(!config.Enabled){SetStatus(")<quick.find("NativeExecutableDiscovery::Resolve")&&quick.find("if(!Storefront::AllowsEmbeddedAobSignatures())")<quick.find("NativeExecutableDiscovery::Resolve"),"manual modes and unknown executable builds must not install Quick Launch signature hooks");
    Need(quick.find("120000")!=quick.npos&&quick.find("if(shift&&loadingHook&&!finished)")!=quick.npos,"a bounded timeout and Shift fallback must restore normal loading");
    const auto registrar=read(root/"src/Misc/DragonWildsDataRegistrar.cpp");
    const auto body=registrar.substr(registrar.find("void DragonWildsDataRegistrar::ScrubCharacterJsonBeforeLoad("));
    Need(body.find("m_pruner.PruneBeforeCharacterLoad")!=body.npos&&body.find("allowCorruptCharacterEntry")==body.npos&&body.find("TolerantLoadingRequested")==body.npos,"general cleanup and native strictness must not be gated by Save Bypass");
    const auto recovery=read(root/"src/Runtime/NativePersistenceRecovery.cpp");
    Need(recovery.find("NativePersistencePolicy::SessionRequested(Storefront::IsDedicatedServer(),")<recovery.find("NativePersistencePolicy::Hold")&&recovery.find("GetSettings().persistence.allowCorruptCharacterEntry,launch.mode")!=recovery.npos,"dedicated servers use only Save Bypass; clients use their loading mode");
    Need(ui.find("if(!quick.value(\"DedicatedServer\",false)){")<ui.find("ImGui::BeginCombo(\"Launch mode\""),"headless servers must not show client launch choices");
    Need(quick.find("EnsureSessionPolicy(world)")!=quick.npos&&body.find("EnsureSessionPolicy(context)")<body.find("m_pruner.PruneBeforeCharacterLoad"),"session policy must cover menu/world changes and uncertified save loads");
    Need(body.find("if(GetCurrentThreadId()==m_registrationThread)PS::NativePersistenceRecovery::EnsureSessionPolicy(context)")!=body.npos&&recovery.find("sessionThread!=GetCurrentThreadId()")!=recovery.npos,"native console mutation and restoration must remain on the owning game thread");
}
