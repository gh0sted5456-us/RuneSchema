#pragma once
#include "Generator/AppearanceSignatures.h"
namespace DragonWilds::JournalWinGDKContract {
inline constexpr PS::AppearanceSignatures::Target CategoryTargets[]{{18,4,true}};
inline constexpr PS::AppearanceSignatures::Signature Definitions[]{
{"JournalHierarchyInsertWinGDK",0,"4c894c24204c894424185355565741544155415641574883ec68be010000004c8be28b51344d8bd0488bf9448d6efe85d27439486369308d42ff4c8b09","ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",nullptr,0},
{"JournalCategory1WinGDK",0,"48895c2408574883ec20488bf94883c130e86ab686fa488bd84885c07423","ffffffffffffffffffffffffffffffffffff00000000ffffffffffffffff",CategoryTargets,std::size(CategoryTargets)},
{"JournalCategory2WinGDK",0,"48895c2408574883ec20488bf94883c158e8fab586fa488bd84885c07423","ffffffffffffffffffffffffffffffffffff00000000ffffffffffffffff",CategoryTargets,std::size(CategoryTargets)},
{"JournalCategory3WinGDK",0,"48895c2408574883ec20488bf94883e980e88ab586fa488bd84885c07423","ffffffffffffffffffffffffffffffffffff00000000ffffffffffffffff",CategoryTargets,std::size(CategoryTargets)},
{"JournalHierarchyBuilderLayoutWinGDK",0,"48894c2408555356574154415541564157488d6c24984881ec68010000488bd9","ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",nullptr,0}
};
}
