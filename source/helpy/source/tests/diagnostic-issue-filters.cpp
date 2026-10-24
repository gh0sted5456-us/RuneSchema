#include "Core/DiagnosticIssueFilters.h"
#include <cassert>
using namespace PS;
int main(){
    DiagnosticCenterModel::Group first,second,third;
    first.Id=1;first.Code="RS-BP-001";first.Title="Property missing";
    first.Message="Requested member was not found";
    first.ModId="DemoMod";first.File="mods/DemoMod/blueprints/patch.jsonc";
    first.Property="Radius";first.Target="/Game/Test/BP_Test";
    first.Phase="Application";first.Outcome="Partial";
    first.CauseDescription="Maybe unavailable";
    first.Severity=LiveLog::Level::Warning;first.Count=25;
    first.LastSeen=DiagnosticCenterModel::Clock::now();
    second=first;second.Id=2;second.Code="RS-ASSET-001";second.ModId="OtherMod";
    second.File="mods/OtherMod/asset.jsonc";second.Property="Size";
    second.Severity=LiveLog::Level::Error;second.Count=3;
    second.LastSeen=first.LastSeen+std::chrono::seconds(2);second.Viewed=true;
    third=first;third.Id=3;third.Severity=LiveLog::Level::Info;
    assert(DiagnosticIssueFilters::Matches(first,"bp-001",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"demomod",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"patch.jsonc",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"radius",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"member was not found",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"bp_test",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"application",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"partial",false,false));
    assert(DiagnosticIssueFilters::Matches(first,"maybe unavailable",false,false));
    assert(!DiagnosticIssueFilters::Matches(first,"unrelated",false,false));
    assert(!DiagnosticIssueFilters::Matches(third,"",false,false));
    assert(DiagnosticIssueFilters::Matches(third,"",true,false));
    assert(!DiagnosticIssueFilters::Matches(second,"",false,true));
    assert(DiagnosticIssueFilters::Matches(first,"",false,true));
    assert(DiagnosticIssueFilters::Matches(second,"",true,false,1));
    assert(!DiagnosticIssueFilters::Matches(first,"",true,false,1));
    assert(DiagnosticIssueFilters::Matches(first,"",false,false,2));
    assert(DiagnosticIssueFilters::Matches(third,"",true,false,3));
    assert(!DiagnosticIssueFilters::Matches(first,"",true,false,3));
    std::vector<const DiagnosticCenterModel::Group*> groups{&first,&second};
    DiagnosticIssueFilters::Sort(groups,3);assert(groups.front()==&first);
    DiagnosticIssueFilters::Sort(groups,2);assert(groups.front()==&second);
    DiagnosticIssueFilters::Sort(groups,0);assert(groups.front()==&second);
    DiagnosticIssueFilters::Sort(groups,1);assert(groups.front()==&first);
}
