#include "Core/StructuredDiagnosticReader.h"
#include "Core/DiagnosticCenterHistory.h"
#include "Core/StructuredDiagnosticAdvice.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
using namespace PS;
static StructuredDiagnostic::Record Make(std::string mod="A") {
    StructuredDiagnostic::Record record;
    record.EventId="ev-01";record.SessionId="s-100";record.Timestamp="2026-10-21T06:30:01Z";
    record.Code="RS-ASSET-001";record.Severity="Error";record.Outcome="Failed";
    record.Source="AssetLoader";record.Title="Asset unresolved";record.Message="Could not resolve target";
    record.Context={{"ModId",mod},{"Target","/Game/Example/BP"},{"Phase","Resolution"}};
    record.Cause={{"Status","Unknown"}};return record;
}
int main(){
    const auto directory=std::filesystem::temp_directory_path()/"rsdiag-reader-b1";
    std::filesystem::create_directories(directory);const auto file=directory/"RSDiagLog.jsonl";
    std::filesystem::remove(file);
    auto valid=StructuredDiagnostic::Encode(Make());assert(valid);
    {std::ofstream out(file,std::ios::binary);out<<*valid<<"bad json\n";out<<valid->substr(0,12);}
    auto first=StructuredDiagnosticReader::Read(file,{});
    assert(first.Records.size()==1&&first.Invalid==1&&first.Next.Partial.size()==12);
    {std::ofstream out(file,std::ios::binary|std::ios::app);out<<valid->substr(12);}
    auto second=StructuredDiagnosticReader::Read(file,first.Next);
    assert(second.Records.size()==1);
    // B3B regression: both BuildingLoader and SpawnLoader are legal Source values
    // on the HELPY reader side, not merely in the RuneSchema producer contract.
    for (const auto& [source, code] : std::initializer_list<std::pair<const char*,const char*>>{
             {"BuildingLoader", "RS-BLD-002"}, {"SpawnLoader", "RS-SPAWN-001"}}) {
        auto record = Make();
        record.Source=source;record.Code=code;
        record.Outcome="Skipped";
        const auto encoded=StructuredDiagnostic::Encode(record);
        assert(encoded.has_value());
        const auto decoded=StructuredDiagnostic::Parse(*encoded);
        assert(decoded.has_value());
        assert(decoded->Code==code && decoded->Source==source);
        const auto probeFile=directory/(std::string(source)+".jsonl");
        { std::ofstream output(probeFile,std::ios::binary);output<<*encoded; }
        const auto batch=StructuredDiagnosticReader::Read(probeFile,{});
        assert(batch.Records.size()==1 && batch.Invalid==0);
        assert(batch.Records.front().Source==source);
        std::filesystem::remove(probeFile);
    }
    DiagnosticCenterModel::Store store;
    const auto one=store.AcceptStructured(first.Records[0]);assert(one!=0);
    const auto* details=store.FindGroup(one);
    assert(details && details->Title=="Asset unresolved");
    assert(details->Category=="Assets"&&store.Events().back().Category=="Assets");
    assert(details->Message=="Could not resolve target");
    assert(details->Outcome=="Failed" && details->Phase=="Resolution");
    assert(details->ModId=="A" && details->Target=="/Game/Example/BP");
    assert(details->CauseStatus=="Unknown" && details->CauseDescription.empty());
    assert(!StructuredDiagnosticAdvice::Find("RS-ASSET-001").empty());
    assert(StructuredDiagnosticAdvice::Find("RS-CORE-999").empty());
    assert(!StructuredDiagnosticAdvice::Find("RS-BLD-014").empty());
    assert(!StructuredDiagnosticAdvice::Find("RS-JRN-002").empty());
    assert(!StructuredDiagnosticAdvice::Find("RS-BP-003").empty());
    assert(StructuredDiagnosticAdvice::Find("RS-EQP-006").find("validation error")!=std::string_view::npos);
    assert(StructuredDiagnosticAdvice::Find("RS-EQP-006").find("Restart")==std::string_view::npos);
    assert(store.AcceptStructured(second.Records[0])==0);
    auto secondMod=Make("B");secondMod.EventId="ev-02";
    assert(store.AcceptStructured(secondMod)!=one);
    assert(store.Groups().size()==2&&store.UnreadIssues()==2);
    auto grouped=Make();grouped.EventId="ev-03";grouped.Evidence={{"RepeatCount",15}};
    assert(store.AcceptStructured(grouped)==one);
    assert(store.FindGroup(one)->Count==16);
    assert(store.MarkViewed(one));assert(store.UnreadIssues()==1);
    const auto snap=DiagnosticCenterHistory::Snapshot(store,"s-xyz");
    DiagnosticCenterModel::Store restored;
    assert(DiagnosticCenterHistory::Restore(snap,restored));
    assert(restored.Groups().size()==2&&restored.UnreadIssues()==1);
    assert(restored.FindGroup(1)->Count==16);
    assert(restored.FindGroup(1)->Message=="Could not resolve target");
    assert(restored.FindGroup(1)->Outcome=="Failed");
    assert(restored.FindGroup(1)->Target=="/Game/Example/BP");
    auto followup=Make();followup.EventId="ev-04";
    followup.Message="The asset remains unresolved";
    followup.Outcome="Deferred";
    followup.Cause={{"Status","Suspected"},{"Description","Asset may not be mounted yet"}};
    assert(store.AcceptStructured(followup)==one);
    assert(store.FindGroup(one)->Message=="The asset remains unresolved");
    assert(store.FindGroup(one)->CauseStatus=="Suspected");
    assert(store.FindGroup(one)->CauseDescription=="Asset may not be mounted yet");
    assert(DiagnosticCenterHistory::Restore({{"version",1},{"events",nlohmann::json::array()}},restored));
    std::filesystem::remove_all(directory);
}
