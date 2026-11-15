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
    constexpr auto pin="f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44";
    const auto shortPin=ParseStartup("UE4SS - v3.0.1 Beta #0 - Git SHA #f6d5f94");
    assert(shortPin&&Compare(*shortPin,pin,"3.0.1")==Relation::Pinned);
    assert(Compare(*current,pin,"3.0.1")==Relation::Pinned);
    assert(Compare(*future,pin,"3.0.1")==Relation::Newer);
    assert(Compare({"3.0.1","d935b5b"},pin,"3.0.1")==Relation::Older);
    assert(Compare({"3.0.1 Beta #0","1ba38db4"},pin,"3.0.1")==Relation::Newer);
    assert(Compare({"2.5.2","1234567"},pin,"3.0.1")==Relation::Older);
    assert(Compare({"3.0.1 Beta #0","1234567"},pin,"3.0.1")==Relation::Unknown);
    assert(!ParseStartup("UE4SS - vlatest - Git SHA #f6d5f942"));
    assert(!ParseStartup("UE4SS - v3.0.1 - Git SHA #unknown"));
    assert(!ParseStartup("a log without a startup banner"));
    assert(!ParseStartup(std::string(16385,'x')));
    assert(argc==2);std::ifstream file(argv[1]);const std::string source{std::istreambuf_iterator<char>(file),{}};
    assert(source.find("LoadedContainsCommit")!=source.npos&&source.find("CompareFileTime")!=source.npos);
    assert(source.find("MatchesTarget")==source.npos&&source.find("no build-number ceiling")!=source.npos);
}
