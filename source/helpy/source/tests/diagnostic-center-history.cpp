#include "Core/DiagnosticCenterHistory.h"
#include "Core/DiagnosticCenterMutes.h"
#include <cassert>
#include <filesystem>
#include <string>
#include <chrono>
using namespace PS;
int main(){
    using namespace DiagnosticCenterHistory;
    const auto folder=std::filesystem::temp_directory_path()/("helpy-d3-test-"+std::to_string(Millis(Clock::now())));
    std::filesystem::create_directories(folder);
    DiagnosticCenterModel::Store source;
    auto entry=LiveLog::Parse("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED] item=ExampleMod/Blue");
    auto second=LiveLog::Parse("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED] item=OtherMod/Green");
    auto error=LiveLog::Parse("[RuneSchema] [error] [WEARABLE-BRIDGE][NOT-ENROLLED] item=ExampleMod/Blue");
    const auto id=source.Accept(entry);source.Accept(entry);source.Accept(second);
    source.MarkViewed(id,true);
    assert(source.FindGroup(id)->Acknowledged);
    const auto doc=Snapshot(source,"s-test");
    DiagnosticCenterModel::Store restored;
    assert(Restore(doc,restored));
    assert(restored.Events().size()==3&&restored.Groups().size()==2);
    assert(restored.FindGroup(id)->Viewed&&restored.FindGroup(id)->Acknowledged);
    assert(!Restore(json::object(),restored));
    DiagnosticCenterMutes::Manager mute;mute.Open(folder);
    assert(mute.Add(*source.FindGroup(id),DiagnosticCenterMutes::Scope::Exact,DiagnosticCenterMutes::Duration::Session));
    assert(mute.Suppress(entry));assert(!mute.Suppress(second));assert(!mute.Suppress(error));
    assert(mute.Add(*source.FindGroup(id),DiagnosticCenterMutes::Scope::Category,DiagnosticCenterMutes::Duration::Persistent));
    assert(mute.Suppress(second));assert(!mute.Suppress(error));
    // The other two scope/duration combinations must work as well.
    assert(mute.Add(*source.FindGroup(id),DiagnosticCenterMutes::Scope::Exact,DiagnosticCenterMutes::Duration::Persistent));
    assert(mute.Add(*source.FindGroup(id),DiagnosticCenterMutes::Scope::Category,DiagnosticCenterMutes::Duration::Session));
    assert(mute.Rules().size()==4);
    assert(!mute.Add(*source.FindGroup(id),DiagnosticCenterMutes::Scope::Category,DiagnosticCenterMutes::Duration::Session));
    DiagnosticCenterMutes::Manager recoveredMute;recoveredMute.Open(folder);
    assert(recoveredMute.Rules().size()==2&&recoveredMute.Suppress(second));
    assert(recoveredMute.Rules().front().Label==source.FindGroup(id)->Category);
    assert(recoveredMute.Rules().back().Label==source.FindGroup(id)->Title);
    assert(!recoveredMute.Suppress(error));
    Service history;history.Open(folder,{14,10,20});
    const auto savedId=history.CurrentId();
    history.SetRetention({30,2,5});
    assert(history.CurrentId()==savedId); // retention changes must not restart writer
    history.Queue(source,true);history.Close();
    assert(std::filesystem::exists(folder/"diagnostic-history"/(savedId+".json")));
    history.Open(folder,{14,10,20});
    DiagnosticCenterModel::Store previous;
    assert(history.Load(savedId,previous));
    assert(previous.Events().size()==3&&previous.FindGroup(id)->Acknowledged);
    assert(history.Sessions().size()==1);
    history.Queue(source,true);history.Close();
    assert(std::filesystem::exists(folder/"diagnostic-history"/(savedId+".json")));
    history.ClearHistory(); // no open current; old sessions are removable
    assert(history.Sessions().empty());
    // A fresh session must prune old files even if no new event is emitted.
    const auto oldDir=folder/"diagnostic-history";
    std::filesystem::create_directories(oldDir);
    const auto oldFile=oldDir/"s-1111111111111-123.json";
    {std::ofstream out(oldFile);out<<"{}";}
    std::error_code ec;
    std::filesystem::last_write_time(oldFile,
        std::filesystem::file_time_type::clock::now()-std::chrono::hours(24*40),ec);
    assert(!ec);
    history.Open(folder,{7,10,20});history.Close();
    assert(!std::filesystem::exists(oldFile));
    std::filesystem::remove_all(folder);
}
