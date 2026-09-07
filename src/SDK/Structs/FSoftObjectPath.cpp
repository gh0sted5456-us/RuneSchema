#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/SoftPathParts.h"

using namespace RC;
using namespace RC::Unreal;
namespace UECustom {
FSoftObjectPath::FSoftObjectPath(RC::StringViewType path) {
    const auto parts=PS::SplitSoftPath(path);
    if(!parts.valid) { Reset();return; }
    AssetPath=FTopLevelAssetPath(FName(RC::StringType(parts.package),FNAME_Add),
        parts.asset.empty()?FName{}:FName(RC::StringType(parts.asset),FNAME_Add));
    // string_view may not be NUL-terminated.
    SubPathString=parts.subobject.empty()?FString{}:FString(RC::StringType(parts.subobject).c_str());
}
void FSoftObjectPath::SetPath(const FTopLevelAssetPath& asset,FString subpath) {
    AssetPath=asset;SubPathString=MoveTemp(subpath);
}
}
