#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static int checks=0;
static void Check(bool value,const char* why){++checks;if(!value){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
static std::string Read(const char* path){std::ifstream file(path,std::ios::binary);Check(bool(file),"contract input opens");std::ostringstream out;out<<file.rdbuf();return out.str();}
static bool Has(const std::string& text,const std::string& value){return text.find(value)!=std::string::npos;}

int main(int argc,char** argv){
    Check(argc==8,"seven source paths supplied");
    const auto plugin=Read(argv[1]);
    const auto menu=Read(argv[2]);
    const auto ui=Read(argv[3]);
    const auto host=Read(argv[4]);
    const auto entry=Read(argv[5]);
    const auto storefront=Read(argv[6]);
    const auto catalog=Read(argv[7]);
    Check(Has(plugin,"EmbeddedCatalog()")&&Has(plugin,"F2Catalog::BundledPaths()"),"Helpy DLL owns an embedded seed catalogue");
    Check(Has(plugin,"state->ToolsCatalog=EmbeddedCatalog()"),"clear and initialization restore the embedded catalogue");
    Check(Has(plugin,"for(auto it=core.begin();it!=core.end();++it)state->ToolsCatalog[it.key()]=it.value()"),"core updates overlay rather than discard the embedded catalogue");
    const auto begin=menu.find("void InGameQuickMenu::Toggle()");
    const auto end=menu.find("void InGameQuickMenu::SelectPrevious()",begin);
    Check(begin!=std::string::npos&&end!=std::string::npos,"toggle implementation bounded");
    const auto toggle=menu.substr(begin,end-begin);
    Check(Has(toggle,"Command::Kind::Players"),"open requests only lightweight target information");
    Check(!Has(toggle,"Command::Kind::Refresh")&&!Has(toggle,"Command::Kind::Index"),"open never starts refresh or full scan");
    Check(Has(ui,"Columns=3")&&Has(ui,"ItemsPerPage=12"),"fixed three-by-four catalogue layout");
    Check(Has(ui,"FontScale=1.12f"),"larger unified Helpy typography scale");
    Check(Has(ui,"SurfaceWidth=ContentWidth-SurfaceInset*2.f")
        &&Has(ui,"panel({SurfaceInset,28,SurfaceWidth,664},ash)"),"tabs and overlays do not share one painted width");
    Check(Has(menu,"IconCacheLimit=256")&&Has(menu,"FirstFrameIconBudget=16")
        &&Has(menu,"m_canvasIconQueue")&&Has(menu,"for(const auto& draw:frame.draws)"),
        "bounded visible-first session icon capture missing");
    Check(!Has(menu,"const auto frame=m_ui.Render(mx,my);bool loadedIcon=false"),
        "one-icon-per-frame loading throttle returned");
    Check(Has(host,"plugin.Manifest.Version")&&Has(host,"plugin.Manifest.ConsoleMessage"),"plugin announcement is manifest driven");
    Check(Has(entry,"RC::Output::send<RC::LogLevel::Normal>")&&Has(entry,"Runtime storefront:"),"storefront is visible in standard logs");
    Check(Has(entry,"RuneSchema.Networking is not active. Local loaders remain enabled."),"plugins are optional to RuneSchema core");
    Check(Has(storefront,"GetCurrentPackageFullName")&&Has(storefront,"UE4SS_Signatures")&&Has(storefront,"WinGDK/Windows package executable path"),"storefront pivot combines package, path, and signature evidence");
    Check(Has(catalog,"A missing DLL does not hide the manifest or its PAKs.")&&Has(catalog,"loading without it"),"plugin DLL and dependency mismatches do not suppress PAK discovery");
    std::cout<<checks<<" Helpy instant-open/plugin announcement checks passed.\n";
}
