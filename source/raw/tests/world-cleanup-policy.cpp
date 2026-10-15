#include "Core/WorldCleanupPolicy.h"
#include "Runtime/WorldBuildingSavePreflight.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
namespace P=PS::WorldBuildingSavePreflight;
static unsigned checks=0;
static void Need(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
template<class F>static void Reject(F run){bool failed=false;try{run();}catch(...){failed=true;}Need(failed,"invalid save accepted");}
static void U32(P::Bytes& bytes,std::uint32_t value){const auto at=bytes.size();bytes.resize(at+4);P::PutU32(bytes,at,value);}
static void Text(P::Bytes& bytes,const std::string& value){U32(bytes,static_cast<std::uint32_t>(value.size()+1));bytes.insert(bytes.end(),value.begin(),value.end());bytes.push_back(0);}
static P::Bytes Chunk(const char* tag,const P::Bytes& payload){P::Bytes out(tag,tag+4);U32(out,static_cast<std::uint32_t>(payload.size()));out.insert(out.end(),payload.begin(),payload.end());return out;}
static P::Bytes Piece(std::uint32_t id,const std::string& identity,std::initializer_list<std::uint32_t> links) {
    P::Bytes out;U32(out,id);Text(out,identity);out.resize(out.size()+36,0); // finite position/orientation
    U32(out,0);out.push_back(0);U32(out,static_cast<std::uint32_t>(links.size()));for(auto link:links)U32(out,link);return out;
}
static P::Bytes World(bool stations) {
    auto rows=Piece(1,"vanilla-platform",stations?std::initializer_list<std::uint32_t>{2,3}:std::initializer_list<std::uint32_t>{3});
    if(stations){auto station=Piece(2,"dye-station",{1});rows.insert(rows.end(),station.begin(),station.end());}
    auto separate=Piece(3,"vanilla-platform",{1});rows.insert(rows.end(),separate.begin(),separate.end());
    const auto pieces=Chunk("Pces",rows);P::Bytes payload;U32(payload,static_cast<std::uint32_t>(pieces.size()));payload.insert(payload.end(),pieces.begin(),pieces.end());
    for(const auto* tag:{"CUST","NOBJ","LATS","LEVL","LVLS","SAVE"})payload=Chunk(tag,payload);return payload;
}
static void Policies() {
    using namespace PS::WorldCleanupPolicy;
    const auto maximum=std::numeric_limits<std::int64_t>::max();
    Need(ArchiveEligible(L"DYE",0,0,maximum,1024),"unlimited native archive rejected");
    Need(ArchiveEligible(L"DYE",0,0,1024,1024),"finite exact-size native archive rejected");
    Need(ArchiveEligible(L"DYE",0,0,2048,1024),"bounded larger native archive rejected");
    Need(ArchiveEligible(L"DYE",0,0,maximum,64ll*1024*1024),"supported world larger than old 32MB cap rejected");
    Need(!ArchiveEligible(L"DYE",0,1,maximum,1024),"partly consumed archive accepted");
    Need(!ArchiveEligible(L"DYE",0,0,1023,1024),"truncated archive accepted");
    Need(!ArchiveEligible(L"DYE",1,0,maximum,1024),"network load accepted");
    Need(!ArchiveEligible(L"",0,0,maximum,1024),"unidentified load accepted");
    Need(!ArchiveEligible(L"<unavailable>",0,0,maximum,1024),"unidentified slot accepted");
    Need(!ArchiveEligible(L"DYE",0,0,maximum,MaximumPayloadBytes+1),"unbounded payload accepted");
    const auto start=MenuWindow::Clock::time_point{};MenuWindow window(start);
    Need(window.MayWrite(start),"initial preflight unavailable");
    Need(window.MayWrite(start+std::chrono::seconds(29)),"menu registry retry ended early");
    Need(!window.MayWrite(start+std::chrono::seconds(31)),"expired write window accepted");
    window.Close();Need(!window.MayWrite(start+std::chrono::seconds(1)),"write allowed after world entry/shutdown");
}
static void Transform() {
    const auto original=World(true);
    Need(P::Parse(original).pieces.size()==3,"fixture parse failed");
    Need(P::RemoveUnresolved(original,{"vanilla-platform","dye-station"}).bytes.empty(),"enabled content rewritten");
    const auto repair=P::RemoveUnresolved(original,{"vanilla-platform"});
    Need(repair.removedIds==std::vector<std::uint32_t>{2},"unrelated building removed");
    Need(repair.removedPersistenceIds==std::vector<std::string>{"dye-station"},"wrong orphan identity");
    Need(repair.bytes==World(false),"platform transform or unrelated world bytes changed");
    const auto clean=P::Parse(repair.bytes);
    Need(clean.pieces.size()==2&&clean.pieces[0].links==std::vector<std::uint32_t>{3}
        &&clean.pieces[1].id==3,"platform connections were not retained");
    Need(P::RemoveUnresolved(repair.bytes,{"vanilla-platform"}).bytes.empty(),"second cleanup was not a no-op");
    Reject([&]{P::RemoveUnresolved(original,{});});
    auto malformed=original;P::PutU32(malformed,4,1);Reject([&]{P::RemoveUnresolved(malformed,{"vanilla-platform"});});
    malformed=original;malformed.pop_back();Reject([&]{P::Parse(malformed);});
    Need(original==World(true),"source bytes mutated");
}
static void Audit(const char* path,const char* retired,const char* expected=nullptr) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("save unavailable");
    const P::Bytes original(std::istreambuf_iterator<char>{file},{});const auto parsed=P::Parse(original);
    std::unordered_set<std::string> live;for(const auto& row:parsed.pieces)live.insert(row.persistenceId);
    if(retired&&std::string_view(retired)=="--list") {
        for(const auto& id:live)std::cout<<"Building identity: "<<id<<'\n';
        retired=nullptr;
    }
    if(live.empty())live.insert("audit-only-empty-world");
    Need(P::RemoveUnresolved(original,live).bytes.empty(),"all-live save unexpectedly rewritten");
    if(retired) {
        std::string_view ids(retired);
        while(!ids.empty()) {
            const auto end=ids.find(',');live.erase(std::string(ids.substr(0,end)));
            if(end==ids.npos)break;ids.remove_prefix(end+1);
        }
    }
    const auto repair=P::RemoveUnresolved(original,live);
    const auto retained=repair.bytes.empty()?parsed.pieces.size():P::Parse(repair.bytes).pieces.size();
    Need(retained+repair.removedIds.size()==parsed.pieces.size(),"record count changed unexpectedly");
    if(expected) {
        std::ifstream reference(expected,std::ios::binary);if(!reference)throw std::runtime_error("expected repaired payload unavailable");
        const P::Bytes knownClean(std::istreambuf_iterator<char>{reference},{});
        Need((repair.bytes.empty()?original:repair.bytes)==knownClean,"repair differs from preserved clean payload");
    }
    std::cout<<"Read-only save audit: records="<<parsed.pieces.size()<<", simulated removals="<<repair.removedIds.size()<<", retained="<<retained<<"; source file never written.\n";
}
int main(int argc,char** argv) {
    try {Policies();Transform();if(argc>=3&&std::string_view(argv[1])=="--audit-save")Audit(argv[2],argc>=4?argv[3]:nullptr,argc==5?argv[4]:nullptr);
        std::cout<<checks<<" world cleanup policy/byte-preservation checks passed.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
