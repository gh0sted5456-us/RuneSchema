#include "Core/DiagnosticIssueNavigation.h"
#include <cassert>
#include <chrono>
using namespace PS;
int main(){
    DiagnosticCenterModel::Group a,b,c;
    a.Id=1;a.Code="RS-BP-001";
    b=a;b.Id=2;
    c=a;c.Id=3;
    std::vector<const DiagnosticCenterModel::Group*> filtered{&c,&a,&b};
    using namespace DiagnosticIssueNavigation;
    assert(Adjacent(filtered,3,1)==1);
    assert(Adjacent(filtered,1,-1)==3);
    assert(Adjacent(filtered,2,1)==0);
    assert(Adjacent(filtered,3,-1)==0);
    assert(Adjacent(filtered,999,1)==3);
    assert(Adjacent(filtered,999,-1)==2);
    assert(Contains(filtered,2)&&!Contains(filtered,99));
    assert(Adjacent({},3,1)==0);

    DiagnosticCenterModel::Store store;
    auto make=[](const char* event,const char* mod,const char* property){
        StructuredDiagnostic::Record r;
        r.EventId=event;r.SessionId="s-b5h";r.Timestamp="2026-10-21T12:00:00Z";
        r.Source="BlueprintLoader";r.Code="RS-BP-001";r.Severity="Warning";
        r.Outcome="Partial";r.Title="Property missing";r.Message="A property was not applied";
        r.Context={{"ModId",mod},{"Target","/Game/Example"},{"Property",property}};
        r.OperationId="operation-1";return r;
    };
    const auto first=store.AcceptStructured(make("first","TestA","Health"));
    const auto second=store.AcceptStructured(make("second","TestA","Speed"));
    const auto third=store.AcceptStructured(make("third","TestB","Radius"));
    assert(first&&second&&third&&first!=second&&second!=third);
    std::vector<const DiagnosticCenterModel::Group*> sorted;
    for(const auto& group:store.Groups())sorted.push_back(&group);
    const auto parent=ParentFor(store,sorted,second);
    assert(parent==first);
    assert(ParentFor(store,sorted,third)==third);
    assert(ParentFor(store,sorted,0)==0);
    std::unordered_set<std::uint64_t> expanded;
    auto rows=DiagnosticIssuePresentation::Rows(store,sorted,true,expanded);
    assert(SelectedRowIndex(rows,second)==rows.size());
    expanded.insert(parent);
    rows=DiagnosticIssuePresentation::Rows(store,sorted,true,expanded);
    assert(SelectedRowIndex(rows,second)<rows.size());
}
