#include "Utility/Logging.h"
#include <cassert>
#include <vector>
namespace PS::DiagnosticLog {
std::vector<std::wstring> Rows;
void Write(std::wstring_view text,bool) noexcept {Rows.emplace_back(text);}
}
int main(){
    using namespace PS;namespace L=RC::LogLevel;
    Log<L::Verbose>(STR("hidden verbose {}"),1);Log<L::Normal>(STR("hidden detail"));RoutineLog("spawns",STR("hidden routine"));
    assert(RC::Output::Rows.empty()&&DiagnosticLog::Rows.empty());
    ConsolidatedLog<L::Normal>(STR("[RuneSchema] normal {}"),42);
    Log<L::Warning>(STR("warning {}"),43);Log<L::Error>(STR("error {}"),44);
    LoaderSummary("recipes",10,1,2,3,0);
    assert(RC::Output::Rows==DiagnosticLog::Rows&&RC::Output::Rows.size()==4);
    assert(RC::Output::Rows[0]==L"[RuneSchema] normal 42");
    assert(RC::Output::Rows[1].contains(L"[warning] warning 43")&&RC::Output::Rows[2].contains(L"[error] error 44"));
    PSConfig::Get()->Advanced=true;
    Log<L::Verbose>(STR("verbose {}"),45);Log<L::Normal>(STR("per-object {}"),46);RoutineLog("spawns",STR("placed {}"),47);
    assert(RC::Output::Rows.size()==4&&DiagnosticLog::Rows.size()==7);
    assert(DiagnosticLog::Rows[4]==L"[RuneSchema] [diagnostic] verbose 45");
    assert(DiagnosticLog::Rows[5]==L"[RuneSchema] per-object 46");
    assert(DiagnosticLog::Rows[6].contains(L"[loader=spawns] placed 47"));
    Log<L::Error>(STR("still visible"));assert(RC::Output::Rows.size()==5&&DiagnosticLog::Rows.size()==8);
    DiagnosticLog::Formatted(L"malformed {",false);assert(DiagnosticLog::Rows.size()==8);
    ConsolidatedLog<L::Normal>(L"phase={} source={}",std::wstring(L"npcs.scan"),std::wstring(L"Mod/quests/test.jsonc"));
    assert(RC::Output::Rows.back()==DiagnosticLog::Rows.back()&&DiagnosticLog::Rows.back().contains(L"npcs.scan"));
}
