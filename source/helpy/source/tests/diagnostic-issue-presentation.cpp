#include "Core/DiagnosticIssuePresentation.h"
#include "Core/DiagnosticCenterHistory.h"
#include <cassert>
#include <unordered_set>
using namespace PS;
static StructuredDiagnostic::Record Make(const char* event,const char* mod,const char* operation,
                                         const char* code,const char* target){
    StructuredDiagnostic::Record r;
    r.EventId=event;r.SessionId="s-ui-b5";r.Timestamp="2026-10-21T12:00:00Z";
    r.Source="AssetLoader";r.Code=code;r.Severity="Warning";r.Outcome="Partial";
    r.Title="Failed operation";r.Message="One field was not applied";
    r.Context={{"ModId",mod},{"Target",target},{"Phase","Application"}};
    r.OperationId=operation;
    return r;
}
int main(){
    DiagnosticCenterModel::Store store;
    const auto a=store.AcceptStructured(Make("one","ModA","transaction-1","RS-ASSET-003","/Game/A"));
    const auto b=store.AcceptStructured(Make("two","ModA","transaction-1","RS-BP-001","/Game/B"));
    const auto c=store.AcceptStructured(Make("three","ModB","transaction-1","RS-ASSET-003","/Game/C"));
    const auto d=store.AcceptStructured(Make("four","ModA","transaction-2","RS-BP-001","/Game/D"));
    auto e=Make("five","ModA","","RS-ASSET-003","/Game/E");
    const auto unlinked=store.AcceptStructured(e);
    assert(a&&b&&c&&d&&unlinked);
    assert(DiagnosticIssuePresentation::Related(store,a).size()==1);
    assert(DiagnosticIssuePresentation::Related(store,a)[0]==b);
    assert(DiagnosticIssuePresentation::Related(store,c).empty());
    assert(DiagnosticIssuePresentation::Related(store,unlinked).empty());
    const auto* ga=store.FindGroup(a),*gb=store.FindGroup(b),*gc=store.FindGroup(c),*gd=store.FindGroup(d),*ge=store.FindGroup(unlinked);
    assert(ga&&gb&&gc&&gd&&ge);
    assert(DiagnosticIssueFilters::Matches(*ga,"",false,false,2));
    assert(!DiagnosticIssueFilters::Matches(*ga,"",false,false,1));
    std::vector<const DiagnosticCenterModel::Group*> sorted{ga,gb,gc,gd,ge};
    auto rows=DiagnosticIssuePresentation::Rows(store,sorted,true,{});
    assert(rows.size()==4&&rows[0].Issue==ga&&rows[0].Related==1);
    assert(rows[1].Issue==gc);
    rows=DiagnosticIssuePresentation::Rows(store,sorted,true,{a});
    assert(rows.size()==5&&rows[1].Issue==gb&&rows[1].Child);
    rows=DiagnosticIssuePresentation::Rows(store,sorted,false,{});
    assert(rows.size()==5);
    {
        // RowsView returns the same rows as Rows() and rebuilds only when an input changes.
        const auto same=[&](const std::vector<DiagnosticIssuePresentation::Row>& left,
                            const std::vector<DiagnosticIssuePresentation::Row>& right){
            if(left.size()!=right.size())return false;
            for(std::size_t i=0;i<left.size();++i)
                if(left[i].Issue!=right[i].Issue||left[i].Related!=right[i].Related||left[i].Child!=right[i].Child)return false;
            return true;
        };
        DiagnosticIssueFilters::View view;
        DiagnosticIssuePresentation::RowsView cache;
        std::unordered_set<std::uint64_t> expanded;
        view.Update(store,"",true,false,2,0);
        assert(same(cache.Update(store,view,true,expanded),DiagnosticIssuePresentation::Rows(store,view.Groups,true,expanded)));
        assert(cache.Rebuilds==1);
        const auto collapsed=cache.Items.size();
        view.Update(store,"",true,false,2,0);
        cache.Update(store,view,true,expanded);
        assert(cache.Rebuilds==1&&view.Rebuilds==1);
        expanded.insert(a);expanded.insert(b);
        assert(same(cache.Update(store,view,true,expanded),DiagnosticIssuePresentation::Rows(store,view.Groups,true,expanded)));
        assert(cache.Rebuilds==2&&cache.Items.size()==collapsed+1);
        assert(same(cache.Update(store,view,false,expanded),DiagnosticIssuePresentation::Rows(store,view.Groups,false,expanded)));
        assert(cache.Rebuilds==3);
        expanded.clear();
        cache.Update(store,view,false,expanded);
        assert(cache.Rebuilds==3); // The expanded set only matters while clustering.
        view.Update(store,"transaction",true,false,2,0);
        assert(same(cache.Update(store,view,false,expanded),DiagnosticIssuePresentation::Rows(store,view.Groups,false,expanded)));
        assert(cache.Rebuilds==4);
        assert(store.AcceptStructured(Make("six","ModC","transaction-3","RS-BP-001","/Game/F")));
        view.Update(store,"",true,false,2,0);
        assert(same(cache.Update(store,view,true,expanded),DiagnosticIssuePresentation::Rows(store,view.Groups,true,expanded)));
        assert(cache.Rebuilds==5);
    }
    LiveLog::Entry legacy{};
    legacy.Id=100;legacy.Text="[WARNING] [SAVE-CLEANUP] independent recovery warning";
    legacy.Severity=LiveLog::Level::Warning;
    legacy.Scope="BuildingLoader";
    const auto legacyId=store.Accept(legacy);
    assert(legacyId);
    assert(DiagnosticIssuePresentation::Related(store,legacyId).empty());
    const auto saved=DiagnosticCenterHistory::Snapshot(store,"s-ui-b5");
    DiagnosticCenterModel::Store restored;
    assert(DiagnosticCenterHistory::Restore(saved,restored));
    assert(restored.Groups().size()==7);
    assert(DiagnosticIssuePresentation::Related(restored,a).size()==1);
}
