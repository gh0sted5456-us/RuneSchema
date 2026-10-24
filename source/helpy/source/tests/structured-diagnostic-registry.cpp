#include "Core/StructuredDiagnosticRegistry.h"
#include <cassert>
#include <string_view>
using namespace PS;
static_assert(StructuredDiagnosticRegistry::All.size()==221);
static_assert(StructuredDiagnosticRegistry::Find("RS-BLD-001")!=nullptr);
static_assert(StructuredDiagnosticRegistry::Find("RS-BLD-001")->TypicalSeverity=="Error");
static_assert(StructuredDiagnosticRegistry::Find("RS-BLD-022")->TypicalSeverity=="Warning");
static_assert(StructuredDiagnosticRegistry::Find("RS-JRN-001")->LegacyCollision);
static_assert(StructuredDiagnosticRegistry::Find("RS-JRN-002")->LegacyCollision);
static_assert(StructuredDiagnosticRegistry::Find("RS-SPAWN-001")->LegacyCollision);
static_assert(StructuredDiagnosticRegistry::Find("RS-MISSING-999")==nullptr);
int main(){
    std::size_t errors=0,warnings=0,collisions=0;
    std::string_view previous{};
    for(const auto& row:StructuredDiagnosticRegistry::All){
        assert(previous.empty()||previous<row.Code);previous=row.Code;
        assert(!row.Description.empty()&&!row.ProducerSite.empty());
        errors+=row.TypicalSeverity=="Error";
        warnings+=row.TypicalSeverity=="Warning";
        collisions+=row.LegacyCollision;
        assert(StructuredDiagnosticRegistry::Find(row.Code)==&row);
    }
    assert(errors==149&&warnings==72&&collisions==3);
}
