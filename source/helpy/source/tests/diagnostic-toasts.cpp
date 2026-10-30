#include "Core/DiagnosticToastPresentation.h"
#include <cassert>
#include <chrono>
#include <string>
int main(){
    using namespace PS;
    using namespace std::chrono;
    using namespace DiagnosticToastPresentation;
    {
        DiagnosticToastFeed::Feed registry;DiagnosticToastFeed::Options options;
        for(const auto* line:{"[RuneSchema] [NETWORK][BUILDINGS-PENDING] Waiting for IDs",
            "[RuneSchema] [NETWORK][BUILDINGS-READY] IDs ready",
            "[RuneSchema] [warning] [RS-NET-002] Content differs",
            "[RuneSchema] RuneSchema shared registry parity verified: match"})registry.Accept(LiveLog::Parse(line),options);
        assert(registry.Empty());
        registry.Accept(LiveLog::Parse("[RuneSchema] [warning] [RS-NET-003] Placement blocked"),options);
        registry.Accept(LiveLog::Parse("[RuneSchema] [error] [RS-NET-002] Fatal transport failure"),options);
        assert(registry.Visible().size()==2);
        registry.Clear();options.RegistryActivity=true;
        registry.Accept(LiveLog::Parse("[RuneSchema] [NETWORK][BUILDINGS-PENDING] Waiting for IDs"),options);
        assert(!registry.Empty());
    }
    {DiagnosticToastFeed::Notice first,second;assert(first==second);
     second.ImportStats.Errors=1;assert(first!=second);
     second=first;second.BatchErrors=1;assert(first!=second);}
    assert(LiveLog::Parse("[RuneSchema] [debug] module scan").Severity==LiveLog::Level::Diagnostic);
    assert(Body("[2026-10-20T00:00:00.000Z thread=1] [RuneSchema] [diagnostic] Applied Blueprint changes to BP_BaseBuilding_Torch_C.")=="Applied Blueprint changes to BP_BaseBuilding_Torch_C.");
    assert(Body("[RuneSchema] [warning] [AssetLoader] missing asset")=="[AssetLoader] missing asset");
    assert(Body("[RuneSchema] [Warning] [DEBUG] custom")=="custom");
    assert(Body("[RuneSchema] [informative] Keep unknown tag")=="[informative] Keep unknown tag");
    DiagnosticToastFeed::Feed feed;DiagnosticToastFeed::Options cfg;
    const auto start=steady_clock::now();cfg.MaxVisible=2;
    auto entry=LiveLog::Parse("[2026-10-20T00:00:00.000Z thread=1] [RuneSchema] [warning] target missing");
    feed.Accept(entry,cfg,start);
    entry=LiveLog::Parse("[2026-10-20T00:00:01.000Z thread=2] [RuneSchema] [warning] target missing");
    feed.Accept(entry,cfg,start+milliseconds(1));
    assert(feed.Visible().size()==1&&feed.Visible().front().Repetitions==2);
    feed.Accept(LiveLog::Parse("[RuneSchema] [error] other failure"),cfg,start);
    feed.Accept(LiveLog::Parse("[RuneSchema] [info] overflow A"),cfg,start);
    feed.Accept(LiveLog::Parse("[RuneSchema] [info] overflow A"),cfg,start);
    feed.Accept(LiveLog::Parse("[RuneSchema] [info] overflow B"),cfg,start);
    assert(feed.Visible().size()==2);
    feed.Tick(cfg,start+seconds(7));
    assert(feed.Visible().size()==1&&feed.Visible().front().Summary);
    auto overview=Format(feed.Visible().front());
    assert(overview.Title=="More log activity");
    assert(overview.Detail.find("3 additional log messages")!=std::string::npos);
    assert(overview.Detail.find("1 repeated")!=std::string::npos);
    assert(overview.Subtitle=="Full details in HELPY > Diagnostics > Live Log");
    // When a summary is already on screen, new overflow adds to its count.
    feed.Accept(LiveLog::Parse("[RuneSchema] [info] overflow C"),cfg,start+seconds(7)+milliseconds(10));
    feed.Accept(LiveLog::Parse("[RuneSchema] [info] overflow D"),cfg,start+seconds(7)+milliseconds(11));
    feed.Tick(cfg,start+seconds(7)+milliseconds(12));
    assert(feed.Visible().size()==2);
    assert(feed.Visible().front().Summary && feed.Visible().front().SummaryCount==4);
    // Long toast text is bounded without rewriting the preserved log entry.
    auto longEntry=LiveLog::Parse("[RuneSchema] [info] "+std::string(500,'a'));
    DiagnosticToastFeed::Feed longFeed;longFeed.Accept(longEntry,cfg,start);
    assert(Format(longFeed.Visible().front()).Detail.size()<200);
    assert(longEntry.Text.size()>500);
    // New errors are never discarded by formatting.
    auto blueprint=LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_BaseBuilding_Torch_C.");
    DiagnosticToastFeed::Feed single;single.Accept(blueprint,cfg,start);
    auto formatted=Format(single.Visible().front());
    assert(formatted.Title=="Blueprint changes applied");
    assert(formatted.Detail=="BP_BaseBuilding_Torch_C");
    assert(single.Visible().front().Text==blueprint.Text);
    DiagnosticToastFeed::Notice warningEntry;warningEntry.Severity=LiveLog::Level::Warning;warningEntry.Text="[RuneSchema] [warning] Configuration invalid";
    const auto warning=Format(warningEntry);
    assert(warning.Title=="RuneSchema warning"&&warning.Detail=="Configuration invalid");
    // Important errors displace an informational card when the screen is full.
    DiagnosticToastFeed::Feed priority;
    priority.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] loaded A"),cfg,start);
    priority.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] loaded B"),cfg,start);
    priority.Accept(LiveLog::Parse("[RuneSchema] [error] failed C"),cfg,start);
    bool hasError=false;
    for(const auto& item:priority.Visible())if(item.Severity==LiveLog::Level::Error)hasError=true;
    assert(hasError&&priority.Visible().size()==2);
    priority.Tick(cfg,start+seconds(7));
    assert(priority.Visible().size()==1&&priority.Visible().front().Summary);
    // Repeated events cannot pin a toast beyond its maximum lifetime.
    DiagnosticToastFeed::Feed repeated;
    repeated.Accept(blueprint,cfg,start);
    for(int i=1;i<20;++i)repeated.Accept(blueprint,cfg,start+seconds(i));
    assert(repeated.Visible().size()==1);
    assert(repeated.Visible().front().Repetitions<20);
    // Confirmed related Blueprint changes share one notification instead of
    // consuming every visible slot. Original texts stay in the live-log source.
    DiagnosticToastFeed::Feed related;
    auto blueA=LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_BaseBuilding_Torch_C.");
    auto blueB=LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_BaseBuilding_Chandelier_C.");
    auto blueC=LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_BaseBuilding_TorchStanding_C.");
    auto blueD=LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_Torch_C.");
    related.Accept(blueA,cfg,start);
    related.Accept(blueB,cfg,start+milliseconds(1));
    related.Accept(blueC,cfg,start+milliseconds(2));
    related.Accept(blueD,cfg,start+milliseconds(3));
    assert(related.Visible().size()==1);
    const auto& group=related.Visible().front();
    assert(group.BlueprintBatch&&group.BatchCount==4&&group.BatchAssets.size()==3);
    const auto combined=Format(group);
    assert(combined.Title=="Blueprint changes applied");
    assert(combined.Detail.find("4 Blueprint modifications applied")!=std::string::npos);
    assert(combined.Detail.find("BP_BaseBuilding_Chandelier_C")!=std::string::npos);
    assert(combined.Detail.find("+1 more modifications")!=std::string::npos);
    assert(combined.Type==Display::Icon::Diagnostic);
    assert(blueA.Text.find("Applied Blueprint changes to BP_BaseBuilding_Torch_C.")!=std::string::npos);
    // Repeated occurrences count as modifications without duplicating examples.
    related.Accept(blueA,cfg,start+milliseconds(4));
    assert(related.Visible().front().BatchCount==5);
    assert(related.Visible().front().BatchAssets.size()==3);
    // Different and incomplete log messages must not be guessed into this batch.
    related.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] Preparing Blueprint changes to BP_Other_C"),cfg,start+milliseconds(5));
    assert(related.Visible().size()==2);
    DiagnosticToastFeed::Notice unknown;unknown.Text="[RuneSchema] [debug] arbitrary";
    unknown.Severity=LiveLog::Level::Diagnostic;
    assert(Format(unknown).Level=="Debug"&&Format(unknown).Type==Display::Icon::Debug);
    DiagnosticToastFeed::Notice warn;warn.Text="[RuneSchema] [warning] invalid property";
    warn.Severity=LiveLog::Level::Warning;
    assert(Format(warn).Type==Display::Icon::Warning);
    DiagnosticToastFeed::Notice err;err.Text="[RuneSchema] [error] invalid property";
    err.Severity=LiveLog::Level::Error;
    assert(Format(err).Type==Display::Icon::Error);
    assert(Format(feed.Visible().front()).Type==Display::Icon::Activity);
    // A shortened message visibly points the user to its unmodified source.
    assert(Format(longFeed.Visible().front()).Truncated);
    assert(Format(longFeed.Visible().front()).Subtitle.find("Live Log")!=std::string::npos);
    // Multiline notes are preserved for measured/wrapped renderers.
    DiagnosticToastFeed::Notice multi;multi.Text="[RuneSchema] [error] First line\nSecond line\nThird line";
    multi.Severity=LiveLog::Level::Error;
    assert(Format(multi).Detail.find("\nSecond line\n")!=std::string::npos);
    // M4: one $ImpDry report, not three unrelated generic toasts.
    DiagnosticToastFeed::Feed imports;DiagnosticToastFeed::Options many;many.MaxVisible=4;
    const auto impMarker=LiveLog::Parse("[2026-10-20T00:00:00.000Z thread=1] [RuneSchema] [TestAsset] [$ImpDry]");
    imports.Accept(impMarker,many,start);
    imports.Accept(LiveLog::Parse("IMPORT MAP"),many,start+milliseconds(1));
    imports.Accept(LiveLog::Parse("  READY | requested 3 | resolved 6 | planned 6 | reused 1 | errors 0"),many,start+milliseconds(2));
    assert(imports.Visible().empty()); // No partial notifications yet.
    imports.Accept(LiveLog::Parse("  Requested: SpellModule_Dig_0, SpellModule_Cast_0, SpellModule_Damage_0"),many,start+milliseconds(3));
    assert(imports.Visible().size()==1 && imports.Visible().front().ImportReport);
    const auto complete=Format(imports.Visible().front());
    assert(complete.Title=="Import dry-run ready");
    assert(complete.Detail.find("Requested 3 | Resolved 6 | Planned 6 | Reused 1 | Errors 0")!=std::string::npos);
    assert(complete.Detail.find("SpellModule_Dig_0")!=std::string::npos);
    assert(complete.Subtitle.find("Dry run only")!=std::string::npos);
    imports.Accept(LiveLog::Parse("  Dependencies"),many,start+milliseconds(4));
    imports.Accept(LiveLog::Parse("    + SpellModule_Dig_0"),many,start+milliseconds(5));
    assert(imports.Visible().size()==1);
    imports.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] [loader=assets][mod=TEST_LOADERS] targets: updated=1 properties=0."),many,start+milliseconds(6));
    assert(imports.Visible().size()==2); // An unrelated loader event remains distinct.
    const auto loader=Format(imports.Visible().back());
    assert(loader.Title=="Asset loader update");
    // Blocked dry-run remains explicitly a *dry run*, never an applied operation.
    DiagnosticToastFeed::Feed blocked;
    blocked.Accept(LiveLog::Parse("IMPORT MAP"),many,start);
    blocked.Accept(LiveLog::Parse("BLOCKED | requested 2 | resolved 1 | planned 0 | reused 0 | errors 1"),many,start+milliseconds(1));
    blocked.Accept(LiveLog::Parse("Requested: SpellModule_A, SpellModule_B"),many,start+milliseconds(2));
    assert(blocked.Visible().size()==1);
    assert(Format(blocked.Visible().front()).Title=="Import dry-run blocked");
    assert(blocked.Visible().front().Severity==LiveLog::Level::Error);
    // Incomplete reports show their incompleteness without inventing success.
    DiagnosticToastFeed::Feed interrupted;
    interrupted.Accept(LiveLog::Parse("IMPORT MAP"),many,start);
    interrupted.Accept(LiveLog::Parse("[RuneSchema] [warning] independent failure"),many,start+milliseconds(1));
    assert(interrupted.Visible().size()==2);
    assert(Format(interrupted.Visible().front()).Title=="Import dry-run report");
    assert(Format(interrupted.Visible().front()).Detail.find("Incomplete")!=std::string::npos);
    // Verbose $ImpDry report uses separate READY and totals lines.
    DiagnosticToastFeed::Feed verbose;
    verbose.Accept(LiveLog::Parse("IMPORT MAP"),many,start);
    verbose.Accept(LiveLog::Parse("  Requested : 3 modules"),many,start+milliseconds(1));
    verbose.Accept(LiveLog::Parse("    + SpellModule_Dig_0"),many,start+milliseconds(2));
    verbose.Accept(LiveLog::Parse("  READY"),many,start+milliseconds(3));
    verbose.Accept(LiveLog::Parse("  requested 3 | resolved 6 | planned 6 | reused 1 | errors 0"),many,start+milliseconds(4));
    assert(verbose.Visible().size()==1);
    assert(Format(verbose.Visible().front()).Title=="Import dry-run ready");
    assert(Format(verbose.Visible().front()).Detail.find("SpellModule_Dig_0")!=std::string::npos);
    // Concurrent/sequential dry runs are only joined within the explicit
    // report envelope. A new source marker starts a new independent report.
    DiagnosticToastFeed::Feed separate;
    separate.Accept(LiveLog::Parse("[RuneSchema] [FirstSource] [$ImpDry]"),many,start);
    separate.Accept(LiveLog::Parse("IMPORT MAP"),many,start+milliseconds(1));
    separate.Accept(LiveLog::Parse("READY | requested 1 | resolved 1 | planned 1 | reused 0 | errors 0"),many,start+milliseconds(2));
    separate.Accept(LiveLog::Parse("Requested: FirstModule"),many,start+milliseconds(3));
    separate.Accept(LiveLog::Parse("[RuneSchema] [SecondSource] [$ImpDry]"),many,start+milliseconds(4));
    separate.Accept(LiveLog::Parse("IMPORT MAP"),many,start+milliseconds(5));
    separate.Accept(LiveLog::Parse("BLOCKED | requested 1 | resolved 0 | planned 0 | reused 0 | errors 1"),many,start+milliseconds(6));
    separate.Accept(LiveLog::Parse("Requested: SecondModule"),many,start+milliseconds(7));
    assert(separate.Visible().size()==2);
    assert(Format(separate.Visible().front()).Title=="Import dry-run ready");
    assert(Format(separate.Visible().back()).Title=="Import dry-run blocked");
    // M4: Loader summary is authoritative and must not be added to the
    // individual detail count (8 + 3 omitted => 11, not 19 or 22).
    DiagnosticToastFeed::Feed blueFinal;
    for(int i=0;i<8;++i)
        blueFinal.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] Applied Blueprint changes to BP_Test_"+std::to_string(i)+"_C."),many,start+milliseconds(i));
    assert(blueFinal.Visible().size()==1 && blueFinal.Visible().front().BatchCount==8);
    blueFinal.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] Blueprints: 3 additional successful change detail(s) omitted."),many,start+milliseconds(9));
    assert(blueFinal.Visible().size()==1 && blueFinal.Visible().front().BatchCount==11);
    blueFinal.Accept(LiveLog::Parse("[RuneSchema] [diagnostic] Blueprints: 3 additional successful change detail(s) omitted."),many,start+milliseconds(9));
    assert(blueFinal.Visible().front().BatchCount==11); // Duplicate summary is idempotent.
    blueFinal.Accept(LiveLog::Parse("[RuneSchema] [summary] blueprints | loaded=11 | created=0 | updated=11 | placed=0 | errors=0"),many,start+milliseconds(10));
    assert(blueFinal.Visible().size()==1 && blueFinal.Visible().front().BatchCount==11);
    assert(Format(blueFinal.Visible().front()).Detail.find("11 Blueprint modifications")!=std::string::npos);
    assert(Format(blueFinal.Visible().front()).Detail.find("+8 more")!=std::string::npos);
    // Formatting recognizes actual messages without inventing categories.
    DiagnosticToastFeed::Notice reload;
    reload.Text="[RuneSchema] Auto-reloaded mod LightingSettings_8";
    assert(Format(reload).Title=="Mod automatically reloaded");
    assert(Format(reload).Detail=="LightingSettings_8");
    DiagnosticToastFeed::Notice ordinary;
    ordinary.Text="[RuneSchema] IMPORT MAP trace";
    assert(Format(ordinary).Subtitle.empty()); // Don't repeat RuneSchema in footer.
    // Disabling notifications clears both visible and aggregated work.
    cfg.Enabled=false;feed.Tick(cfg,start+seconds(8));assert(feed.Empty());
    cfg.Enabled=true;feed.Tick(cfg,start+seconds(9));assert(feed.Empty());
    return 0;
}
