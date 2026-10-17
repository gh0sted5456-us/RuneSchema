#include "Core/LiveLog.h"
#include <cassert>
#include <fstream>
using namespace PS::LiveLog;
int main(){
    const auto row=Parse("[2026-10-16T12:00:00Z thread=12] [RuneSchema] [error] [PERSISTENCE-PRUNER] Duplicate quest owner marker.");
    assert(row.Severity==Level::Error&&row.Scope=="PERSISTENCE-PRUNER"&&row.Help==0);
    assert(Parse("[RuneSchema] [warning] [NETWORK-REGISTRY] fingerprint mismatch").Help==2);
    assert(Parse("[RuneSchema] [warning] [REGISTRY][PERSISTENCE-PRUNER][DEFERRED] not certified").Help==1);
    assert(Parse("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED]").Help==3);
    assert(Parse("[RuneSchema] [warning] [PERFORMANCE][SLOW-WORK] phase=npcs.scan").Help==4);
    assert(Parse("[RuneSchema] Alphabetical catalogue expired; refresh the list").Help==5);
    assert(Parse("[RuneSchema] [diagnostic] NPC spawn waiting for collision").Help==6);
    assert(Parse("[RuneSchema] [error] unrecognized error").Help==7);
    assert(Parse("[RuneSchema] session started").Help==-1);
    Filter filter;filter.Query="rs-qst-001";assert(filter.Matches(row));filter.Levels[3]=false;assert(!filter.Matches(row));
    History history;history.Append(row.Text);filter={};assert(history.Export(filter).starts_with("[RS-QST-001] "));
    for(int i=0;i<5000;++i)history.Append("entry "+std::to_string(i));assert(history.Rows.size()==4000&&history.Dropped==1001);
    for(int i=0;i<200;++i)history.Append(std::string(16000,'a'));assert(history.Rows.size()<4000&&history.Dropped>1001);
    const auto directory=std::filesystem::temp_directory_path()/("rsmk-live-log-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(directory);const auto file=directory/"RuneSchema.log";
    auto append=[&](std::string_view text,bool truncate=false){std::ofstream stream(file,std::ios::binary|(truncate?std::ios::trunc:std::ios::app));stream<<text;};
    assert(!Read(file,{}).Error.empty());append("first\r\npartial",true);auto batch=Read(file,{});assert(batch.Lines==std::vector<std::string>{"first"}&&batch.Next.Partial=="partial");
    append(" completed\n");batch=Read(file,batch.Next);assert(batch.Lines==std::vector<std::string>{"partial completed"});
    append("new\n",true);batch=Read(file,batch.Next);assert(batch.Reset&&batch.Lines==std::vector<std::string>{"new"});
    // A replacement of the same length still starts a new stream.
    std::filesystem::rename(file,directory/"old.log");append("rot\n",true);batch=Read(file,batch.Next);assert(batch.Reset&&batch.Lines==std::vector<std::string>{"rot"});
    append(std::string(20000,'x')+"\nafter\n",true);batch=Read(file,{});assert(batch.Lines.size()==2&&batch.Lines[0].find("clipped")!=std::string::npos&&batch.Lines[1]=="after");
    append(std::string(2*1024*1024,'a')+"\nlast\n",true);batch=Read(file,{});assert(batch.Skipped&&batch.More&&batch.Next.Offset<=2*1024*1024);
    for(int i=0;i<8&&batch.More;++i)batch=Read(file,batch.Next);assert(batch.Lines.back()=="last");
    // Readers never block the writer or its rename/rotation.
    const auto writer=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(writer!=INVALID_HANDLE_VALUE);assert(Read(file,{}).Error.empty());CloseHandle(writer);
    std::ifstream help(RUNESCHEMA_HELP_DOC);assert(help);const std::string documentation((std::istreambuf_iterator<char>(help)),{});
    for(const auto& guide:Guides){assert(HelpUrl(guide).starts_with("https://gh0sted5456-us.github.io/RuneSchema/TROUBLESHOOTING/#rs-"));assert(documentation.find("{#"+std::string(guide.Anchor)+"}")!=std::string::npos);}
    std::filesystem::remove(file);std::filesystem::remove(directory/"old.log");std::filesystem::remove(directory);
}
