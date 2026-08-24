#include "Utility/ObjectPath.h"

#include <algorithm>

namespace {
    bool IsNumeric(const RC::StringType& value)
    {
        return !value.empty() && std::all_of(value.begin(), value.end(), [](auto character) {
            return character >= TEXT('0') && character <= TEXT('9');
        });
    }
}

namespace PS::ObjectPath {
    RC::StringType Normalize(
        const RC::StringType& path,
        const RC::StringType& objectName,
        bool classReference)
    {
        if (path.empty())
        {
            return path;
        }

        auto colon = path.find(TEXT(':'));
        auto topLevelPath = colon == RC::StringType::npos ? path : path.substr(0, colon);
        auto subPath = colon == RC::StringType::npos ? RC::StringType{} : path.substr(colon);
        auto slash = topLevelPath.find_last_of(TEXT('/'));
        auto dot = topLevelPath.find_last_of(TEXT('.'));
        auto hasAssetSuffix = dot != RC::StringType::npos
            && (slash == RC::StringType::npos || dot > slash);

        auto resolvedName = objectName;
        if (resolvedName.empty())
        {
            auto packageStart = slash == RC::StringType::npos ? 0 : slash + 1;
            auto packageEnd = hasAssetSuffix ? dot : topLevelPath.size();
            resolvedName = topLevelPath.substr(packageStart, packageEnd - packageStart);
        }

        if (!hasAssetSuffix)
        {
            topLevelPath += TEXT(".") + resolvedName;
        }
        else if (IsNumeric(topLevelPath.substr(dot + 1)))
        {
            topLevelPath = topLevelPath.substr(0, dot + 1) + resolvedName;
        }

        if (classReference && !topLevelPath.ends_with(TEXT("_C")))
        {
            topLevelPath += TEXT("_C");
        }

        return topLevelPath + subPath;
    }
}
