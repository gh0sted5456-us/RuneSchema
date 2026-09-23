#pragma once
#include <optional>
#include <string>
namespace PS::AssetMetadata {
struct Declaration {
    std::optional<bool> modded,runeSchema,cooked,safeToClone;
    bool Empty()const{return !modded.has_value()&&!runeSchema.has_value()&&!cooked.has_value()&&!safeToClone.has_value();}
};
inline Declaration Merge(Declaration base,const Declaration& override) {
    if(override.modded.has_value())base.modded=override.modded;
    if(override.runeSchema.has_value())base.runeSchema=override.runeSchema;
    if(override.cooked.has_value())base.cooked=override.cooked;
    if(override.safeToClone.has_value())base.safeToClone=override.safeToClone;
    return base;
}
// Pure clone eligibility policy. Author permission cannot establish cooked or
// runtime identity, registration, dependency availability, or type compatibility.
inline bool Allowed(const Declaration& d,bool cookedEvidence,bool runtimeCreated,bool registered,
    bool modPath,bool stableRuntimeSource,bool permanent,std::string& reason) {
    if(d.safeToClone.has_value()&&!*d.safeToClone){reason="The author disabled cloning for this asset";return false;}
    if(runtimeCreated) {
        if(!d.safeToClone.value_or(false)){reason="Runtime source has no explicit SafeToClone permission";return false;}
        if(!registered){reason="Runtime source has not completed item registration";return false;}
        if(permanent&&!stableRuntimeSource){reason="A permanent clone cannot depend on a session-only runtime source";return false;}
        reason.clear();return true;
    }
    if(!cookedEvidence){reason="No verified cooked-package or registered RuneSchema-creation evidence";return false;}
    if((modPath||d.modded.value_or(false))&&!d.safeToClone.value_or(false)) {
        reason="Modded source requires author opt-in: Modded.SafeToClone = true";return false;
    }
    reason.clear();return true;
}
}
