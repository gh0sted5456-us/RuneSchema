#include "Core/LiveLog.h"
namespace Diagnostics=PS::Diagnostics;
#include <cassert>
#include <fstream>
using namespace PS::LiveLog;
int main(){
    const auto bypass=Parse("[RuneSchema] [info] [SAVE-ENTRY][BYPASSED][PLAYER-STATE] Save Bypass initiated (player state). Details: RuneSchema.log.");
    assert(bypass.Severity==Level::Info);
    const auto bypassDetail=Parse("[RuneSchema] [diagnostic] [SAVE-ENTRY][BYPASSED][PLAYER-STATE][UNATTRIBUTED] Native loading ran once; progress retention remains unverified.");
    assert(bypassDetail.Severity==Level::Diagnostic);
    const auto coded=Parse("[2026-10-18T14:15:16.000Z thread=12] [RuneSchema] [warning] [RS-CACHE-003] Content differs: dye_bronze");
    assert(coded.Time=="14:15:16"&&coded.ReportedCode=="RS-CACHE-003"&&!coded.Suggested&&coded.Message=="Content differs: dye_bronze"&&coded.Scope=="Cache");
    assert(Guides[coded.Help].Value==PS::Diagnostics::Id::CacheMismatch);
    const auto unknown=Parse("[RuneSchema] [error] [RS-FUTURE-999] Keep the original condition");assert(unknown.ReportedCode=="RS-FUTURE-999"&&unknown.Help<0&&!unknown.Suggested);
    assert(!CodeToken("RS-CACHE-003-extra")&&!CodeToken("prefixRS-CACHE-003")&&!CodeToken("RS--003"));
    assert(ReferenceUrl(unknown).ends_with("#rs-log-000"));
    const auto native=Parse("[2026.10.20-03.44.12:123][ 77]LogAnimation: Warning: Skeleton GUID is out-of-date, /Game/Mods/Flintlocks/Montage");
    assert(native.Time=="03:44:12"&&native.Source=="LogAnimation"&&native.Suggested&&Guides[native.Help].Value==PS::Diagnostics::Id::NativeSkeleton);
    assert(ReferenceUrl(native).ends_with("#rs-game-002"));
    for(const auto& [message,code]:std::vector<std::pair<std::string,std::string>>{
        {"LogPlayerVitalsUI: Warning: UPlayerVitalsWidget::RemovePlayerActionInstancesUpdatedCallback: Failed to remove function callback for vitals","RS-GAME-006"},
        {"LogNetSessionSettings: Error: Could not get User Settings!","RS-GAME-007"},
        {"LogDominionUI: Error: Tick(): Expected UDominionUserSettings to exist by now, rethinking this function.","RS-GAME-007"},
        {"LogCrowdFollowing: Warning: Unable to find RecastNavMesh instance while trying to create UCrowdManager instance","RS-GAME-008"},
        {"DominionLog: Warning: URegistryTagSubsystem::UnregisterTaggedObject : Tried to unregister BP_Anvil","RS-GAME-009"},
        {"LogSentinel: Error: Unrecognized native failure (provider=123)","RS-GAME-000"}}){
        const auto entry=Parse("[2026.10.20-22.51.58:123][Standalone]"+message);
        assert(entry.Source==message.substr(0,message.find(':'))&&entry.Text.ends_with(message)&&entry.Suggested&&Guides[entry.Help].Code==code);
        assert(entry.Severity==(message.find("Error:")!=message.npos?Level::Error:Level::Warning));
    }
    const auto explicitNative=Parse("[RS-RT-001] LogTemp: Error: Could not get User Settings!");assert(!explicitNative.Suggested&&Guides[explicitNative.Help].Value==PS::Diagnostics::Id::RuntimeContract);
    assert(Parse("[2026-10-20T12:00:00Z thread=1] [group=runtime] [RuneSchema] [warning] delayed").Source=="RuneSchema");
    Filter words;words.Query="source:loganimation guid -optional";assert(words.Matches(native));
    words.Query="\"skeleton guid\" level:warning code:rs-game-002";assert(words.Matches(native));
    words.Query="skeleton -guid";assert(!words.Matches(native));words.Query="guid missing";assert(!words.Matches(native));
    History repeats;repeats.Append("[2026-10-18T10:00:00Z] [RuneSchema] [warning] [RS-CACHE-003] Content differs");repeats.Append("[2026-10-18T10:00:01Z] [RuneSchema] [warning] [RS-CACHE-003] Content differs");
    assert(Grouped(repeats,{},true).size()==1&&Grouped(repeats,{},true)[0].Count==2&&Grouped(repeats,{},false).size()==2);
    repeats.Append("[RuneSchema] [warning] [RS-CACHE-003] Different owner");assert(Grouped(repeats,{},true).size()==2);
    const auto row=Parse("[2026-10-16T12:00:00Z thread=12] [RuneSchema] [error] [PERSISTENCE-PRUNER] Duplicate quest owner marker.");
    assert(row.Severity==Level::Error&&row.Scope=="PERSISTENCE-PRUNER"&&Guides[row.Help].Value==Diagnostics::Id::QuestOwnerConflict);
    assert(row.Source=="RuneSchema");
    assert(Parse("[2026-10-17 12:00:00] [Lua] [RuneUI] compass ready").Source=="RuneUI");
    assert(Parse("[12:00:00] [lua] [MyMod] Warning: delayed").Severity==Level::Warning);
    assert(Parse("[12:00:00] [RegisterHook] hooked").Scope=="RegisterHook");
    assert(Parse("[12:00:00] Error: hook failed").Severity==Level::Error);
    assert(Parse("[2026.10.18-12.00.00:123][0]LogEOSSDK: Warning: connection delayed").Source=="LogEOSSDK");
    assert(EOSLine("LogEOSSDK: LogEOS: connection")&&EOSLine("[EOS] socket")&&!EOSLine("LogTemp: general startup"));
    assert(std::string(StreamName(Stream::Game))=="Game"&&std::string(StreamName(Stream::EOS))=="EOS");
    const auto mod=std::filesystem::path("fixture/ue4ss/Mods/RuneSchema");
    assert(LogPath(mod,Stream::RuneSchema)==mod/"RuneSchema.log");
    assert(LogPath(mod,Stream::UE4SS)==std::filesystem::path("fixture/ue4ss/UE4SS.log"));
    assert(ProtectedExport(mod/"RuneSchema.log",mod)&&ProtectedExport(mod/"RuneSchema.log.previous",mod));
    assert(ProtectedExport(mod/"logs"/"RuneSchema-20261017-220000-123Z.log",mod));
    assert(ProtectedExport(mod.parent_path().parent_path()/"UE4SS.log",mod));
    assert(!ProtectedExport(mod/"RuneSchema-filtered.log",mod)&&!ProtectedExport(mod/"exports"/"RuneSchema.log",mod));
    std::vector<ColorRule> colors{{true,true,"RuneSchema",{0,1,0,1}},{true,false,"DUPLICATE",{1,0,0,1}}};
    assert(Color(row,colors).value()==colors[1].RGBA); // keywords override source rules
    colors[1].Enabled=false;assert(Color(row,colors).value()==colors[0].RGBA);
    colors[0].Match="persistence-pruner";assert(Color(row,colors).has_value());
    colors[0].Match="unrelated";assert(!Color(row,colors));
    const auto colorDoc=WriteColors(colors);assert(WriteColors(ReadColors(colorDoc))==colorDoc);
    auto invalid=colorDoc;invalid[0]["color"][0]=2.0;invalid[1]["match"]="";assert(ReadColors(invalid).empty());
    invalid=colorDoc;invalid[0]["kind"]="regex";assert(ReadColors(invalid).size()==1);
    invalid=nlohmann::json::array();for(int i=0;i<100;++i)invalid.push_back(colorDoc[0]);assert(ReadColors(invalid).size()==32);
    const auto reference=[](const char* text,Diagnostics::Id id){const auto row=Parse(text);assert(row.Help>=0&&Guides[row.Help].Value==id&&row.Suggested);};
    reference("[RuneSchema] [warning] [NETWORK-REGISTRY] fingerprint mismatch",Diagnostics::Id::RegistryMismatch);
    reference("[RuneSchema] [warning] [REGISTRY][PERSISTENCE-PRUNER][DEFERRED] not certified",Diagnostics::Id::RegistryDeferred);
    reference("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED]",Diagnostics::Id::WearableUnenrolled);
    reference("[RuneSchema] [warning] [PERFORMANCE][SLOW-WORK] phase=npcs.scan",Diagnostics::Id::SlowWork);
    reference("[RuneSchema] Alphabetical catalogue expired; refresh the list",Diagnostics::Id::CatalogUnavailable);
    reference("[RuneSchema] [diagnostic] NPC spawn waiting for collision",Diagnostics::Id::PlacementPending);
    reference("[RuneSchema] [error] unrecognized error",Diagnostics::Id::Unclassified);
    assert(Parse("[RuneSchema] session started").Help==-1);
    Filter filter;filter.Query="rs-qst-001";assert(filter.Matches(row));filter.Levels[3]=false;assert(!filter.Matches(row));
    History history;history.Append(row.Text);filter={};assert(history.Export(filter).starts_with("[Suggested reference RS-QST-001] "));
    for(int i=0;i<5000;++i)history.Append("entry "+std::to_string(i));assert(history.Rows.size()==4000&&history.Dropped==1001);
    for(int i=0;i<200;++i)history.Append(std::string(16000,'a'));assert(history.Rows.size()<4000&&history.Dropped>1001);
    const auto directory=std::filesystem::temp_directory_path()/("rsmk-live-log-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(directory);const auto file=directory/"RuneSchema.log";
    const auto gameFolder=directory/"RSDragonwilds"/"Saved"/"Logs";std::filesystem::create_directories(gameFolder);const auto gameFile=gameFolder/"RSDragonwilds.log";
    {std::ofstream game(gameFile);game<<"LogEOSSDK: session ready\n";}
    assert(ResolveGameLog(mod,directory)==gameFile);
    assert(GameBinaryDirectory("LogInit: Base Directory: D:/Steam/Binaries/Win64/\r\nother")=="D:/Steam/Binaries/Win64/");
    assert(GameBinaryDirectory("LogInit: Base Directory: \n").empty());assert(GameBinaryDirectory("unrelated log").empty());
    auto append=[&](std::string_view text,bool truncate=false){std::ofstream stream(file,std::ios::binary|(truncate?std::ios::trunc:std::ios::app));stream<<text;};
    assert(!Read(file,{}).Error.empty());append("first\r\npartial",true);auto batch=Read(file,{});assert(batch.Lines==std::vector<std::string>{"first"}&&batch.Next.Partial=="partial");
    const auto hostFile=directory/"UE4SS.log";
    assert(Read(hostFile,{}).Error.find("UE4SS.log")!=std::string::npos);
    {std::ofstream host(hostFile);host<<"[Lua] [ExampleMod] hello\n";}
    const auto hostBatch=Read(hostFile,{});assert(hostBatch.Lines==std::vector<std::string>{"[Lua] [ExampleMod] hello"});
    assert(hostBatch.Next.Identity!=batch.Next.Identity&&batch.Next.Partial=="partial");
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
    for(const auto& guide:Guides){assert(HelpUrl(guide)==PS::Diagnostics::Url(guide));assert(HelpUrl(guide)==std::string(PS::Diagnostics::Reference)+"#"+std::string(guide.Anchor));assert(documentation.find("{#"+std::string(guide.Anchor)+"}")!=std::string::npos);
        const auto parsed=Parse("[RuneSchema] [warning] ["+std::string(guide.Code)+"] exact condition");
        assert(!parsed.Suggested&&parsed.Help>=0&&Guides[parsed.Help].Value==guide.Value);}
    std::filesystem::remove_all(directory);
}
