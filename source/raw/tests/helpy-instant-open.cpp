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
    const auto window=Read(argv[2]);
    const auto lazy=Read(argv[3]);
    const auto host=Read(argv[4]);
    const auto entry=Read(argv[5]);
    const auto storefront=Read(argv[6]);
    const auto catalog=Read(argv[7]);
    Check(Has(plugin,"HelpyStandaloneWindow")&&Has(plugin,"QuerySubmit"),"standalone Helpy uses a bounded query service");
    Check(Has(plugin,"state->ToolsCatalog=nlohmann::json::object()"),"initialization does not copy an eager full catalogue");
    Check(Has(plugin,"for(auto it=payload.begin();it!=payload.end();++it)state->ToolsCatalog[it.key()]=it.value()"),"core catalogue updates are merged");
    const auto begin=window.find("bool HelpyStandaloneWindow::OpenPopup()");
    const auto end=window.find("void HelpyStandaloneWindow::ClosePopup()",begin);
    Check(begin!=std::string::npos&&end!=std::string::npos,"desktop open implementation bounded");
    const auto open=window.substr(begin,end-begin);
    Check(Has(open,"LazyCatalog::FirstOpen(")&&Has(open,"QueuePlayerRefresh(true)"),"open uses lazy cached catalogue and lightweight player refresh");
    Check(!Has(open,"IndexQuickCatalog")&&!Has(open,"Reindex")&&!Has(open,"LazyCatalog::Refresh("),"open never starts a full scan or catalogue reset");
    Check(Has(lazy,"FirstOpen(")&&Has(lazy,"RestoreCache("),"lazy catalogue retains cache-first startup");
    Check(Has(host,"plugin.Manifest.Version")&&Has(host,"plugin.Manifest.ConsoleMessage"),"plugin announcement is manifest driven");
    Check(Has(entry,"PS::ConsolidatedLog<RC::LogLevel::Normal>")&&Has(entry,"Runtime storefront:"),"storefront is visible in standard logs and mirrored to RuneSchema.log");
    Check(Has(entry,"RSNetworking is not active. Local loaders remain enabled."),"plugins are optional to RuneSchema core");
    Check(Has(storefront,"GetCurrentPackageFullName")&&Has(storefront,"UE4SS_Signatures")&&Has(storefront,"WinGDK/Windows package executable path"),"storefront pivot combines package, path, and signature evidence");
    Check(Has(catalog,"A missing DLL does not hide the manifest or its PAKs.")&&Has(catalog,"loading without it"),"plugin DLL and dependency mismatches do not suppress PAK discovery");
    std::cout<<checks<<" Helpy instant-open/plugin announcement checks passed.\n";
}
