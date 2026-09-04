#include <SDK/Structs/FSoftObjectPath.h>
#include <SDK/Classes/KismetSystemLibrary.h>
#include <SDK/Classes/AsciiSet.h>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
	FSoftObjectPath::FSoftObjectPath(RC::StringViewType Path)
	{
		if (Path == STR("") || Path == STR("None"))
		{
			Reset();
		}
		else
		{

			FAsciiSet Delimiters = FAsciiSet(".") + (char)SUBOBJECT_DELIMITER_CHAR;
			if (!Path.starts_with('/')
				|| Delimiters.Contains(Path[Path.size() - 1])
				)
			{
				Reset();
				return;
			}


			for (int32 i = 2; i < Path.size(); ++i)
			{
				if (Delimiters.Contains(Path[i]) && Delimiters.Contains(Path[i - 1]))
				{
					Reset();
					return;
				}
			}

			RC::StringViewType PackageNameView = FAsciiSet::FindPrefixWithout(Path, Delimiters);
			if (PackageNameView.size() == Path.size())
			{
				AssetPath = FTopLevelAssetPath(FName(RC::StringType(PackageNameView), FNAME_Add), FName());
				SubPathString.Empty();
				return;
			}

			RC::StringViewType FixedAssetName = Path.size() >= PackageNameView.size() + 1 ? Path.substr(PackageNameView.size() + 1, Path.size()) : TEXT("");
			check(FixedAssetName != "" && !Delimiters.Contains(FixedAssetName[0]));

			RC::StringViewType AssetNameView = FAsciiSet::FindPrefixWithout(FixedAssetName, Delimiters);
			if (AssetNameView.size() == FixedAssetName.size())
			{
				AssetPath = FTopLevelAssetPath(FName(RC::StringType(PackageNameView), FNAME_Add), FName(AssetNameView, FNAME_Add));
				SubPathString.Empty();
				return;
			}

			auto FixedPackageName = FixedAssetName.size() >= AssetNameView.size() + 1 ? FixedAssetName.substr(0, AssetNameView.size() + 1) : TEXT("");
			check(FixedAssetName != "" && !Delimiters.Contains(FixedAssetName[0]));
			
			AssetPath = FTopLevelAssetPath(FName(RC::StringType(PackageNameView), FNAME_Add), FName(AssetNameView, FNAME_Add));
			SubPathString = FString(FixedPackageName.data());
		}
	}

	void FSoftObjectPath::SetPath(const FTopLevelAssetPath& InAssetPath, RC::Unreal::FString InSubPathString)
	{
		AssetPath = InAssetPath;
		SubPathString = MoveTemp(InSubPathString);
	}
}
