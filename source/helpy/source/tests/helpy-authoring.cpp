#include "Generator/AuthoringClient.h"
#include <cassert>
#include <thread>
#include <atomic>
using namespace PS;
using Json=nlohmann::json;
template<class F>bool Rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){
    HelpyAuthoring::Session host;
    const auto request=[](uint64_t id,uint64_t generation,const char* op="Catalogue",Json args=Json::object()){
        return Json{{"RequestId",id},{"Generation",generation},{"Operation",op},{"Arguments",args}};
    };
    host.Configure(false,true);assert(!host.Activate()["Accepted"].get<bool>());
    host.Configure(true,false);assert(!host.Activate()["Accepted"].get<bool>());
    host.Configure(true,true);assert(!host.Submit(request(1,1))["Accepted"].get<bool>());
    assert(host.Activate()["Accepted"]&&!host.Active());assert(host.TickActivation());
    assert(Rejects([&]{host.Submit(request(1,1,"InvokeNative"));}));
    assert(Rejects([&]{host.Submit(request(1,1,"Browse",{{"Query",std::string(257,'x')}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"Browse",{{"Limit",501}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"ExportDocument",{{"Document",std::string(262145,'x')}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"CleanupExport",{{"Confirm",false},{"Token","cleanup-1"}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"Preset",{{"Export",{{"Jsonc","yes"}}}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"Preset",{{"Export",{{"Name",std::string(97,'x')}}}}));}));
    assert(Rejects([&]{host.Submit(request(1,1,"PlayerTraceStart",{{"UseProfileTarget","yes"}}));}));
    for(const auto* op:{"DiagnosticGuide","TraceTargetClear","PlayerPresetSave","EventSuggestions"})
        assert(HelpyAuthoring::Validate(request(99,1,op))["Operation"]==op);
    assert(host.Submit(request(1,1))["Accepted"]);assert(host.Submit(request(1,1))["Duplicate"]);
    assert(!host.Submit(request(2,1))["Accepted"].get<bool>());auto work=host.Take();assert(work&&(*work)["Operation"]=="Catalogue");
    host.Complete(1,{{"Loaders",Json::array({"assets"})}},"Ready");
    auto snap=host.Snapshot();assert(snap["Result"]["Loaders"][0]=="assets");assert(!host.Snapshot(snap["Revision"]).contains("Result"));
    assert(host.Submit(request(2,1,"Schema"))["Accepted"]);host.Take();assert(host.Snapshot()["Operation"]=="Catalogue");
    host.WorldChanged();host.Complete(2,{{"Document","stale"}},"stale");assert(host.Snapshot()["Result"].empty());
    assert(!host.Submit(request(3,1))["Accepted"].get<bool>());
    std::atomic<int> accepted=0;std::thread a([&]{if(host.Submit(request(4,2))["Accepted"])++accepted;});
    std::thread b([&]{if(host.Submit(request(5,2))["Accepted"])++accepted;});a.join();b.join();assert(accepted==1);work=host.Take();host.Complete((*work)["RequestId"],{},"done");
    // Production client -> real queue -> snapshot, including polling while busy
    // and missed world changes. The test never performs a native game operation.
    AuthoringClient::Remote=[&](const Json& input,Json& output){
        const auto action=input.at("Action");
        if(action=="AuthoringSnapshot")output=host.Snapshot(input.value("Revision",uint64_t{}));
        else if(action=="AuthoringSubmit")output=host.Submit(input.at("Request"));
        else if(action=="AuthoringActivate")output=host.Activate();else return false;return true;
    };
    AuthoringClient::ExportOptions={{"Name","MyTrace"},{"Jsonc",true}};
    AuthoringClient::Poll(true);assert(AuthoringClient::Submit("Schema",{{"Loader","assets"}}));
    assert(!AuthoringClient::Submit("Catalogue"));work=host.Take();assert((*work)["Arguments"]["Export"]==AuthoringClient::ExportOptions);AuthoringClient::Poll(true);assert(AuthoringClient::Busy());
    host.Complete((*work)["RequestId"],{{"Document",{{"title","assets"}}}},"Schema ready");
    AuthoringClient::Poll(true);assert(!AuthoringClient::Busy()&&AuthoringClient::Result("Schema")["Document"]["title"]=="assets");
    assert(AuthoringClient::Submit("Catalogue"));work=host.Take();host.Complete((*work)["RequestId"],{},"Capture failed",false);
    AuthoringClient::Poll(true);assert(AuthoringClient::Result("Catalogue").contains("Error"));
    assert(AuthoringClient::Submit("Player",{{"DelaySeconds",30}}));work=host.Take();const auto oldId=work->at("RequestId");
    assert(AuthoringClient::Submit("Cancel"));auto cancel=host.TakeCancellation();assert(cancel&&(*cancel)["Operation"]=="Cancel");
    host.Complete(oldId,{{"Stale",true}},"Stale");host.Complete(cancel->at("RequestId"),Json::object(),"Cancelled");
    AuthoringClient::Poll(true);assert(!AuthoringClient::Busy()&&!AuthoringClient::Result("Cancel").contains("Stale"));
    host.WorldChanged();AuthoringClient::Poll(true);assert(AuthoringClient::Results.empty());AuthoringClient::Clear();
}
