#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream f(path);if(!f)throw std::runtime_error("Canvas contract input unavailable");return {std::istreambuf_iterator<char>(f),{}};}
static int check=0;
static void Require(bool value){++check;if(!value){std::cerr<<"Helpy Canvas fallback regression, check "<<check<<'\n';throw std::runtime_error("Helpy Canvas fallback regression");}}
int main(int argc,char** argv){
    Require(argc==5);
    const auto menu=Read(argv[2]),actor=Read(argv[3]),plugin=Read(argv[4]);
    Require(plugin.find("VK_F2")!=std::string::npos && plugin.find("PS::HelpyHotkeys::Active.load()")!=std::string::npos);
    Require(plugin.find("state->Menu->Initialize(false)")!=std::string::npos && plugin.find("state->Menu->Shutdown()")!=std::string::npos);
    Require(menu.find("ResolvePostRender()")!=std::string::npos && menu.find("UGameViewportClient::VTableLayoutMap")!=std::string::npos
        && menu.find("PostRenderThunk")!=std::string::npos);
    Require(menu.find("/Script/Engine.HUD:ReceiveDrawHUD")==std::string::npos);
    Require(menu.find("/Script/Engine.Canvas:K2_DrawTexture")!=std::string::npos && menu.find("/Script/Engine.Canvas:K2_DrawText")!=std::string::npos);
    Require(menu.find("WhiteSquareTexture.WhiteSquareTexture")!=std::string::npos && menu.find("Roboto.Roboto")!=std::string::npos);
    Require(menu.find("SetIgnoreLookInput")!=std::string::npos && menu.find("SetIgnoreMoveInput")!=std::string::npos && menu.find("ReleaseViewportInput")!=std::string::npos);
    Require(menu.find("menu->AbortOpen(stage,e.what())")!=std::string::npos && menu.find("CloseWithReason(CloseReason::Shutdown")!=std::string::npos);
    Require(actor.find("FunctionCall& FunctionCall::JsonArg")!=std::string::npos);
}
