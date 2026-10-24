#pragma once
namespace PS::StructuredDiagnosticOutcomeEvidence {
inline const char* InterruptedAfter(bool anyApplied) noexcept {return anyApplied?"Partial":"Skipped";}
inline const char* PhaseAfter(bool anyApplied) noexcept {return anyApplied?"Application":"Validation";}
class AppliedProgress {
    bool m_applied=false;
public:
    void MarkApplied() noexcept {m_applied=true;}
    bool HasApplied() const noexcept {return m_applied;}
    const char* Interrupted() const noexcept {return InterruptedAfter(m_applied);}
    const char* InterruptedPhase() const noexcept {return PhaseAfter(m_applied);}
};
}
