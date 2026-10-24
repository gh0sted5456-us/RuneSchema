#pragma once
#include "Core/StructuredDiagnosticContract.h"
#include <cstdint>

namespace PS::StructuredDiagnostics {
void Initialize() noexcept;
bool Emit(StructuredDiagnostic::Record record) noexcept;
void Shutdown() noexcept;
struct Health {std::uint64_t Written=0,Dropped=0,Errors=0;};
Health Counters() noexcept;
}
