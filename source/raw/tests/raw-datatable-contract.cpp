#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);if(!file)return {};
    std::ostringstream out;out<<file.rdbuf();return out.str();
}
static void Need(const std::string& text,const char* token,const char* message) {
    if(text.find(token)==std::string::npos){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    const auto loader=Read(argv[1]),guide=Read(argv[2]);
    Need(loader,"LoadAndApplyRawTargets();","exact raw DataTable targets are not eagerly prepared");
    Need(loader,"path.starts_with('/') && path.find('.')!=std::string::npos","exact DataTable paths are not recognized");
    Need(loader,"LoadAsset_Blocking(soft)","unloaded DataTables are not loaded on demand");
    Need(loader,"m_appliedExactTargets.insert(path).second","exact targets can be applied twice during synchronous serialization");
    Need(loader,"Apply(datatable->GetName(), datatable);","legacy short-name tables lost compatibility");
    Need(guide,"Any reflected `UDataTable`","generic raw DataTable support is undocumented");
    std::cout<<"Raw DataTable contract supports exact paths and legacy names without double application.\n";
}
