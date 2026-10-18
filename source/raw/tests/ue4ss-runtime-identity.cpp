#include "Core/UE4SSBuildIdentity.h"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace PS::UE4SSBuildIdentity;
int main(int argc,char** argv){
    const auto current=ParseStartup("[2026-10-16 09:16:13.1640908] UE4SS - v3.0.1 Beta #0 - Git SHA #f6d5f942\r\n");
    assert(current&&current->Version=="3.0.1 Beta #0"&&current->Commit=="f6d5f942");
    const auto future=ParseStartup("UE4SS - v99.12.45 PreRelease #9999 - Git SHA #ABCDEF0123456789\n");
    assert(future&&future->Commit=="abcdef0123456789"); // identity is not a version gate
    assert(!ParseStartup("UE4SS - vlatest - Git SHA #f6d5f942"));
    assert(!ParseStartup("UE4SS - v3.0.1 - Git SHA #unknown"));
    assert(!ParseStartup("a log without a startup banner"));
    assert(!ParseStartup(std::string(16385,'x')));
    assert(argc==2);std::ifstream file(argv[1]);const std::string source{std::istreambuf_iterator<char>(file),{}};
    assert(source.find("LoadedContainsCommit")!=source.npos&&source.find("CompareFileTime")!=source.npos);
    assert(source.find("MatchesTarget")==source.npos&&source.find("no build-number ceiling")!=source.npos);
}
