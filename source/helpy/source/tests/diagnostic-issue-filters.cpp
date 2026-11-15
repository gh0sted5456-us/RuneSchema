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
    // Case-insensitive matching never needs the haystack lowered first.
    assert(DiagnosticIssueFilters::Contains("Mods/DemoMod/PATCH.jsonc","patch.json"));
    assert(!DiagnosticIssueFilters::Contains("","x")&&DiagnosticIssueFilters::Contains("x",""));
    // View rebuilds only when the store revision or filters change.
    DiagnosticCenterModel::Store store;
    for(int i=0;i<4;++i){LiveLog::Entry entry;entry.Severity=i%2?LiveLog::Level::Error:LiveLog::Level::Warning;
        entry.Message="Problem "+std::to_string(i);entry.Text=entry.Message;entry.Source="Mod"+std::to_string(i);store.Accept(entry);}
    DiagnosticIssueFilters::View view;
    const auto all=view.Update(store,"",false,false,0,0).size();
    assert(all==store.Groups().size()&&all>0&&view.Rebuilds==1);
    view.Update(store,"",false,false,0,0);assert(view.Rebuilds==1);   // Unchanged: cached.
    view.Update(store,"",false,false,1,0);assert(view.Rebuilds==2);   // Filter change.
    store.MarkViewed(store.Groups().front().Id);
    view.Update(store,"",false,false,1,0);assert(view.Rebuilds==3);   // Store revision change.
    const auto onlyNew=view.Update(store,"",false,true,0,0).size();assert(onlyNew==all-1);
    LiveLog::Entry late;late.Severity=LiveLog::Level::Error;late.Message="Late";late.Text="Late";store.Accept(late);
    for(const auto* group:view.Update(store,"",false,false,0,0))assert(group>=store.Groups().data()&&group<store.Groups().data()+store.Groups().size());
}
