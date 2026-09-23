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
    Check(argc==6,"five source paths supplied");
    const auto plugin=Read(argv[1]);
    const auto menu=Read(argv[2]);
    const auto ui=Read(argv[3]);
    const auto host=Read(argv[4]);
    const auto entry=Read(argv[5]);
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
    Check(Has(host,"plugin.Manifest.Version")&&Has(host,"plugin.Manifest.ConsoleMessage"),"plugin announcement is manifest driven");
    Check(Has(entry,"RC::Output::send<RC::LogLevel::Normal>")&&Has(entry,"Runtime storefront:"),"storefront is visible in standard logs");
    std::cout<<checks<<" Helpy instant-open/plugin announcement checks passed.\n";
}
