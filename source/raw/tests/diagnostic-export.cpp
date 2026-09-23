#include "Generator/DiagnosticExport.h"
#include "Generator/PlayerTraceOptions.h"
#include "Runtime/NetworkRole.h"
#include "Runtime/NetworkRoleNotice.h"
#include <cassert>
#include <sstream>
#include <iomanip>
int main() {
    using namespace PS::InspectionTools;
    using namespace PS::Network;
    assert(!IsGameplayRoleWorld(""));
    assert(!IsGameplayRoleWorld("/Game/Maps/L_FrontEnd.L_FrontEnd"));
    assert(IsGameplayRoleWorld("/Game/Maps/TestWorld.TestWorld"));
    RoleNotice loadedNotice;
    assert(loadedNotice.Observe(Role::Client,"loaded",true));
    assert(!loadedNotice.Observe(Role::Client,"loaded",true));
    RoleRetryBudget budget;
    assert(!budget.Take());assert(budget.Start());assert(!budget.Start());
    for(int i=0;i<8;++i)assert(budget.Take());
    assert(!budget.Active() && !budget.Take());assert(budget.Start());
    budget.Clear();assert(!budget.Active());
    RoleNotice notices;
    assert(!notices.Observe(Role::Unknown,""));
    assert(notices.Observe(Role::Unknown,""));
    assert(!notices.Observe(Role::Unknown,""));
    assert(!notices.Observe(Role::Standalone,"menu"));
    assert(notices.Observe(Role::Standalone,"menu"));
    assert(!notices.Observe(Role::Standalone,"menu"));
    assert(!notices.Observe(Role::Unknown,""));
    assert(!notices.Observe(Role::Standalone,"menu"));
    assert(!notices.Observe(Role::Standalone,"menu"));
    assert(!notices.Observe(Role::Client,"world"));
    assert(notices.Observe(Role::Client,"world"));
    assert(!notices.Observe(Role::Client,"world"));
    assert(!notices.Observe(Role::ListenServer,"world"));
    assert(notices.Observe(Role::ListenServer,"world"));
    assert(!notices.Observe(Role::DedicatedServer,"world"));
    assert(notices.Observe(Role::DedicatedServer,"world"));
    assert(!notices.Observe(Role::DedicatedServer,"other-world"));
    assert(notices.Observe(Role::DedicatedServer,"other-world"));
    assert(Classify(true,true,false)==Role::Standalone);
    assert(Classify(false,false,false)==Role::Client);
    assert(Classify(false,true,false)==Role::ListenServer);
    assert(Classify(false,true,true)==Role::DedicatedServer);
    assert(Classify(true,true,true)==Role::Unknown);
    assert(Classify(true,false,false)==Role::Unknown);
    assert(Classify(false,false,true)==Role::Unknown);
    assert(!OwnsGameplay(Role::Client) && !OwnsGameplay(Role::Unknown));
    assert(OwnsGameplay(Role::DedicatedServer) && !HasLocalPresentation(Role::DedicatedServer));
    const auto original=WithDiagnosticOrigin(nlohmann::json::object(),{{"mode","client"},{"world","first"}});
    assert(DiagnosticOriginLabel(original)=="client");
    assert(WithDiagnosticOrigin(original,{{"mode","dedicated-server"}})==original);
    assert(DiagnosticOriginLabel({{"origin",{{"mode","../escape"}}}})=="unknown");
    assert(DiagnosticExportName("Rod Cast","Trace","123",false)=="Rod Cast_trace_123.json");
    assert(DiagnosticExportName("","Trace","123",true)=="Trace_trace_123.jsonc");
    assert(DiagnosticExportName("characterburning","NiagaraAttachment","123",true,"niagara")=="characterburning_niagara_123.jsonc");
    assert(DiagnosticExportName("Goblin","Reference","123",true,"assets")=="Goblin_assets_123.jsonc");
    for (auto name : {"../escape","a/b","a\\b","c:bad","bad.json","x\n"}) {
        bool rejected=false;try { DiagnosticExportName(name,"Trace","1",false); } catch (...) { rejected=true; }
        assert(rejected);
    }
    using nlohmann::json;
    const auto empty=SearchReport("missing","Loaded objects","",json::array(),250);
    assert(empty["Count"]==0 && empty["LimitReached"]==false);
    const auto found=SearchReport("Goblin Pack","Loaded game records","assets",
        json::array({{{"Key","/Game/Test"},{"Name","Goblin Pack"}}}),1);
    assert(found["Query"]=="Goblin Pack" && found["Loader"]=="assets");
    assert(found["LimitReached"]==true && found["Results"][0]["Key"]=="/Game/Test");
    assert(json::parse(found.dump())==found);
    const auto escaped=SearchReport("x\nnot a comment\r\n/*","Loaded objects","",json::array(),500);
    assert(json::parse(escaped.dump())==escaped);
    assert(escaped["Limit"]==500 && escaped["LimitReached"]==false);
    for(const auto& rows:{json::object(),json::array({1,2})}) {
        bool rejected=false;try{SearchReport("q","scope","raw",rows,1);}catch(...){rejected=true;}
        assert(rejected);
    }
    const json streamed={{"text","quoted \" text"},{"values",{1,true,nullptr}}};
    std::ostringstream output;output<<std::setw(2)<<streamed;
    assert(output.str()==streamed.dump(2));
    const json valid={{"Filter","SendPayloadForSpellCasting"},{"EventCaptures",json::array({
        {{"Root","Context"},{"Path",json::array({"*"})}}})},{"TraceSelectedObject",false}};
    PS::PlayerTrace::ValidateOptions(valid);
    for(int i=0;i<5;++i) {
        auto invalid=valid;
        if(i==0)invalid["Filter"]="";
        if(i==1)invalid["EventCaptures"][0]["Root"]="World";
        if(i==2)invalid["EventCaptures"][0]["Path"]=json::array();
        if(i==3)invalid["EventCaptures"][0]["Path"]=json::array({"*","Other"});
        if(i==4)invalid["TraceSelectedObject"]="yes";
        bool rejected=false;try { PS::PlayerTrace::ValidateOptions(invalid); } catch (...) { rejected=true; }
        assert(rejected);
    }
}
