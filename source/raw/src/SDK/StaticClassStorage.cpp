#include "SDK/StaticClassStorage.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    void StaticClassStorage::Initialize()
    {
        const auto find=[](const TCHAR* name){return DragonWilds::PropertyHelper::FindFieldClassByName(RC::StringType{name});};
        ObjectPropertyStaticClass=find(STR("ObjectProperty"));
        ObjectPtrPropertyStaticClass=find(STR("ObjectPtrProperty"));
        NamePropertyStaticClass=find(STR("NameProperty"));
        BoolPropertyStaticClass=find(STR("BoolProperty"));
        ArrayPropertyStaticClass=find(STR("ArrayProperty"));
        MapPropertyStaticClass=find(STR("MapProperty"));
        StructPropertyStaticClass=find(STR("StructProperty"));
        ClassPropertyStaticClass=find(STR("ClassProperty"));
        ClassPtrPropertyStaticClass=find(STR("ClassPtrProperty"));
        SoftClassPropertyStaticClass=find(STR("SoftClassProperty"));
        SoftObjectPropertyStaticClass=find(STR("SoftObjectProperty"));
        EnumPropertyStaticClass=find(STR("EnumProperty"));
        TextPropertyStaticClass=find(STR("TextProperty"));
        StrPropertyStaticClass=find(STR("StrProperty"));
        NumericPropertyStaticClass=find(STR("NumericProperty"));
    }
}
