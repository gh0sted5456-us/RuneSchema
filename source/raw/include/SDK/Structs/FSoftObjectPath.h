#pragma once

#include "SDK/Structs/FTopLevelAssetPath.h"
#include "Unreal/FString.hpp"
#include "Unreal/NameTypes.hpp"

namespace UECustom {
    struct FSoftObjectPath
    {
    public:
        FSoftObjectPath() {};

        FSoftObjectPath(RC::StringViewType Path);

        FSoftObjectPath(RC::Unreal::FName InPackageName, RC::Unreal::FName InAssetName, RC::Unreal::FString InSubPathString)
        {
            SetPath(FTopLevelAssetPath(InPackageName, InAssetName), RC::Unreal::MoveTemp(InSubPathString));
        }

		FTopLevelAssetPath GetAssetPath() const
		{
			return AssetPath;
		}

		RC::Unreal::FString GetLongPackageName() const
		{
			RC::Unreal::FName PackageName = GetAssetPath().GetPackageName();
			return PackageName == RC::Unreal::NAME_None ? RC::Unreal::FString() : RC::Unreal::FString(PackageName.ToString().c_str());
		}

		RC::Unreal::FName GetLongPackageFName() const
		{
			return GetAssetPath().GetPackageName();
		}

		RC::Unreal::FString GetAssetName() const
		{
			RC::Unreal::FName AssetName = GetAssetPath().GetAssetName();
			return AssetName == RC::Unreal::NAME_None ? RC::Unreal::FString() : RC::Unreal::FString(AssetName.ToString().c_str());
		}

		RC::Unreal::FName GetAssetFName() const
		{
			return GetAssetPath().GetAssetName();
		}

		void SetPath(const FTopLevelAssetPath& InAssetPath, RC::Unreal::FString InSubPathString = RC::Unreal::FString());

		void Reset()
		{
			AssetPath.Reset();
			SubPathString = RC::Unreal::FString();
		}

        FTopLevelAssetPath AssetPath;
		RC::Unreal::FString SubPathString{};
    };
}
