// Included once by DragonWildsAssetModLoader.cpp. All entry points run on the
// game thread through SpawnToolRequests, never from the Canvas/ImGui renderer.
namespace {
    constexpr std::uint64_t CloneUnsafeFlags = CPF_Transient | CPF_DuplicateTransient
        | CPF_NonPIEDuplicateTransient | CPF_InstancedReference | CPF_ContainsInstancedReference
        | CPF_Deprecated | CPF_EditorOnly;

    bool CloneHasSetter(FProperty* p) {
        // Match the concrete setters in PropertyHelper, not arbitrary reflected
        // fields which its fallback would merely log and silently leave alone.
        return p && (CastField<FBoolProperty>(p)||CastField<FNumericProperty>(p)
            ||DragonWilds::PropertyHelper::CastProperty<FEnumProperty>(p)||CastField<FStrProperty>(p)||CastField<FNameProperty>(p)
            ||CastField<FTextProperty>(p)||CastField<FClassProperty>(p)
            ||(CastField<FObjectProperty>(p)&&p->GetClass().GetName()==TEXT("ObjectProperty"))
            ||CastField<FSoftObjectProperty>(p)||CastField<FSoftClassProperty>(p)
            ||CastField<FStructProperty>(p)||CastField<FArrayProperty>(p)||CastField<FMapProperty>(p));
    }
    bool CloneEditable(FProperty* p) {
        return CloneHasSetter(p) && p->GetArrayDim()==1 && !p->HasAnyPropertyFlags(CloneUnsafeFlags)
            && PS::Authoring::SafeFieldName(RC::to_string(p->GetName()));
    }
    FBoolProperty* CloneSoftDelete(UClass* type) {
        FBoolProperty* result=nullptr;
        for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if(!PS::Authoring::SoftDeleteName(RC::to_string(p->GetName())))continue;
            auto* flag=CastField<FBoolProperty>(p);
            if(!flag || flag->GetArrayDim()!=1 || flag->HasAnyPropertyFlags(CloneUnsafeFlags))
                throw std::runtime_error("The item soft-delete field is not a supported persistent Boolean");
            if(result && result!=flag)throw std::runtime_error("More than one item soft-delete field exists; ambiguous contract");
            result=flag;
        }
        return result;
    }
    // Return actual values only when they have a verified, lossless JSON form.
    // Composite fields remain inherited until the user supplies an override.
    bool CloneReadValue(FProperty* p,void* container,nlohmann::json& value,int depth=0) {
        if(!p||!container||depth>4||p->GetArrayDim()!=1)return false;
        auto* address=p->ContainerPtrToValuePtr<void>(container);
        if(auto* b=CastField<FBoolProperty>(p)){value=b->GetPropertyValue(address);return true;}
        if(auto* e=DragonWilds::PropertyHelper::CastProperty<FEnumProperty>(p)) {
            auto enumeration=e->GetEnum();if(!enumeration)return false;
            const auto size=p->GetElementSize();int64 n=0;
            if(size==1)n=*static_cast<int8_t*>(address);else if(size==2)n=*static_cast<int16_t*>(address);
            else if(size==4)n=*static_cast<int32_t*>(address);else if(size==8)n=*static_cast<int64_t*>(address);else return false;
            for(const auto& pair:enumeration->GetEnumNames())if(pair.Value==n){value=RC::to_string(pair.Key.ToString());return true;}return false;
        }
        if(auto* n=CastField<FNumericProperty>(p)) {
            if(n->IsEnum()) {auto enumeration=n->GetIntPropertyEnum();if(!enumeration)return false;const auto v=n->GetSignedIntPropertyValue(address);
                for(const auto& pair:enumeration->GetEnumNames())if(pair.Value==v){value=RC::to_string(pair.Key.ToString());return true;}return false;}
            if(n->IsInteger()) {
                const auto kind=PS::HelpyPropertyValue::Kind(DragonWilds::PropertyHelper::GetPropertyTypeAsUTF8String(p));
                if(kind=="Unsigned") {
                    uint64_t u=0;const auto size=p->GetElementSize();
                    if(size!=1&&size!=2&&size!=4&&size!=8)return false;
                    std::memcpy(&u,address,static_cast<std::size_t>(size));
                    if(u>static_cast<uint64_t>(INT64_MAX))return false;value=static_cast<int64_t>(u);
                }else value=n->GetSignedIntPropertyValue(address);
            }
            else {const auto v=n->GetFloatingPointPropertyValue(address);if(!std::isfinite(v))return false;value=v;}
            return true;
        }
        if(auto* s=CastField<FStrProperty>(p)){value=RC::to_string(RC::StringType(*s->GetPropertyValue(address)));return true;}
        if(CastField<FNameProperty>(p)){value=RC::to_string(static_cast<FName*>(address)->ToString());return true;}
        // Text is shown as display text, but is never reapplied unless edited.
        if(CastField<FTextProperty>(p)){value=RC::to_string(DragonWilds::PropertyHelper::GetTextAsString(*static_cast<FText*>(address)));return true;}
        // Soft references need their full path, not just the currently loaded object.
        if(CastField<FSoftObjectProperty>(p) || CastField<FSoftClassProperty>(p)) {
            if(p->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))return false;
            const auto& path=static_cast<UECustom::FSoftObjectPtr*>(address)->ObjectID;
            const auto package=RC::to_string(path.GetLongPackageFName().ToString());
            const auto asset=RC::to_string(path.GetAssetFName().ToString());
            if(path.SubPathString.GetCharArray().Num()>1)return false;
            if(package.empty()||package=="None"){value=nullptr;return true;}
            if(!package.starts_with('/')||asset.empty()||asset=="None")return false;
            value=package+"."+asset;return true;
        }
        if(auto* array=CastField<FArrayProperty>(p)) {
            auto* data=static_cast<FScriptArray*>(address);auto* inner=array->GetInner();
            const auto count=data->Num();
            if(!inner||inner->GetOffset_Internal()!=0||inner->GetElementSize()<=0||count<0||count>64||(count&&!data->GetData()))return false;
            value=nlohmann::json::array();
            for(int i=0;i<count;++i) {
                nlohmann::json v;
                if(!CloneReadValue(inner,static_cast<uint8*>(data->GetData())+static_cast<std::size_t>(i)*inner->GetElementSize(),v,depth+1))return false;
                value.push_back(std::move(v));
            }
            return true;
        }
        if(auto* structure=CastField<FStructProperty>(p)) {
            auto* type=structure->GetStruct().Get();if(!type)return false;value=nlohmann::json::object();
            for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
                if(field->HasAnyPropertyFlags(CloneUnsafeFlags))continue;
                nlohmann::json v;if(!CloneReadValue(field,address,v,depth+1))return false;
                value[RC::to_string(field->GetName())]=std::move(v);
                if(value.size()>PS::Authoring::MaxCloneFields)return false;
            }return true;
        }
        if(auto* mapProperty=CastField<FMapProperty>(p)) {
            value=nlohmann::json::array();bool valid=true;std::size_t count=0;
            UECustom::FScriptMapHelper map(mapProperty,address);
            map.ForEachPair([&](void* k,void* v) {
                if(!valid)return;nlohmann::json key,child;
                if(++count>64||mapProperty->GetKeyProp()->GetOffset_Internal()!=0||mapProperty->GetValueProp()->GetOffset_Internal()!=0
                    ||!CloneReadValue(mapProperty->GetKeyProp(),k,key,depth+1)||!CloneReadValue(mapProperty->GetValueProp(),v,child,depth+1)){valid=false;return;}
                value.push_back({{"Key",key},{"Value",child}});
            });return valid;
        }
        if(auto* o=CastField<FObjectProperty>(p)) {
            auto* target=o->GetObjectPropertyValue(address);
            if(target)value=RC::to_string(target->GetPathName());else value=nullptr;return true;
        }
        return false;
    }
    bool CloneValueMatches(const nlohmann::json& actual,const nlohmann::json& expected) {
        if(actual.is_number_float()&&expected.is_number()) {
            const auto a=actual.get<double>(),e=expected.get<double>();
            return std::isfinite(a)&&std::isfinite(e)&&std::abs(a-e)<=std::max(1e-7,std::abs(e)*2e-7);
        }
        if(actual.is_array()&&expected.is_array()) {
            if(actual.size()!=expected.size())return false;
            for(std::size_t i=0;i<actual.size();++i)if(!CloneValueMatches(actual[i],expected[i]))return false;
            return true;
        }
        if(actual.is_object()&&expected.is_object()) {
            if(actual.size()!=expected.size())return false;
            for(const auto& [key,value]:expected.items())if(!actual.contains(key)||!CloneValueMatches(actual.at(key),value))return false;
            return true;
        }
        return actual==expected;
    }
    std::string CloneVisualType(FProperty* p) {
        if(!p||!CloneEditable(p)||!PS::ClonePresentation::VisualField(RC::to_string(p->GetName())))return {};
        UClass* expected=nullptr;
        if(auto* soft=CastField<FSoftObjectProperty>(p))expected=soft->GetPropertyClass().Get();
        else if(auto* ref=CastField<FObjectProperty>(p))expected=ref->GetPropertyClass().Get();
        if(!expected)return {};
        const auto type=RC::to_string(expected->GetName());
        return PS::ClonePresentation::VisualAssetType(type)?type:std::string{};
    }
    void CloneRequireCookedReferences(const nlohmann::json& value) {
        if(value.is_null())return;
        if(value.is_array()){for(const auto& child:value)CloneRequireCookedReferences(child);return;}
        if(!value.is_string())throw std::runtime_error("Appearance references require an installed cooked object path");
        const auto path=value.get<std::string>();PS::Authoring::ValidateObjectPath(path);
        if(PS::CookedAssets::ContainsReference(path))return;
        // Only a missing registry layout uses loaded-package evidence. A known
        // registry miss is still a rejection; no runtime asset is promoted to cooked.
        if(PS::CookedAssets::Available())throw std::runtime_error("Appearance asset is absent from the mounted registry");
        auto* asset=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(path).c_str(),false);
        if(!asset){auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(path)));asset=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);}
        if(!PS::CookedAssets::VerifiedLoadedObject(asset)||RC::to_string(asset->GetPathName())!=path)
            throw std::runtime_error("Appearance asset has no verified installed-package evidence");
    }
    void CloneValidateOverride(FProperty* p,const nlohmann::json& value,int depth=0) {
        if(depth>16 || !CloneHasSetter(p) || p->GetArrayDim()!=1 || p->HasAnyPropertyFlags(CloneUnsafeFlags))
            throw std::runtime_error("Clone override targets a runtime-only, instanced, or unsupported field");
        auto* classProperty=CastField<FClassProperty>(p);
        auto* softClass=CastField<FSoftClassProperty>(p);
        auto* softObject=CastField<FSoftObjectProperty>(p);
        auto* objectProperty=CastField<FObjectProperty>(p);
        if(classProperty || softClass || softObject || objectProperty) {
            // PropertyHelper accepts inline ObjectProperty edits which would
            // traverse a SHARED reference copied from the source. Never allow
            // that here, including inside arrays, maps or structs.
            if(value.is_null())return;
            if(!value.is_string())throw std::runtime_error("Live clone references require a full object path string or null; inline shared-object edits are forbidden");
            const auto path=value.get<std::string>();PS::Authoring::ValidateObjectPath(path);
            auto* target=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(path).c_str(),false);
            if(!target) {
                auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(path)));
                target=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);
            }
            if(!target || target->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedInitialization|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects)))
                throw std::runtime_error("Cannot resolve clone reference: "+path);
            if(classProperty || softClass) {
                auto* expected=classProperty?classProperty->GetMetaClass().Get():softClass->GetMetaClass().Get();
                if(!target->IsA<UClass>() || (expected && !static_cast<UClass*>(target)->IsChildOf(expected)))
                    throw std::runtime_error("Clone class reference is incompatible: "+path);
            }else {
                auto* expected=softObject?softObject->GetPropertyClass().Get():objectProperty->GetPropertyClass().Get();
                if(!expected || !target->IsA(expected))throw std::runtime_error("Clone object reference has the wrong asset type: "+path);
            }
            return;
        }
        if(auto* structure=CastField<FStructProperty>(p);structure && value.is_array()) {
            auto* type=structure->GetStruct().Get();
            if(!type || !DragonWilds::PropertyHelper::GetPropertyByName(type,TEXT("GameplayTags")))
                throw std::runtime_error("This struct does not support the gameplay-tag array shorthand");
            auto tags=nlohmann::json::array();
            for(const auto& tag:value)tags.push_back(tag.is_string()?nlohmann::json{{"TagName",tag}}:tag);
            CloneValidateOverride(p,nlohmann::json{{"GameplayTags",tags}},depth+1);return;
        }
        if(auto* e=DragonWilds::PropertyHelper::CastProperty<FEnumProperty>(p)) {
            (void)DragonWilds::PropertyHelper::ParseEnumFromJsonValue(e,value);return;
        }
        if(auto* number=CastField<FNumericProperty>(p)) {
            if(number->IsEnum()){if(!value.is_string())throw std::runtime_error("Select an enum name, not a numeric ordinal");(void)DragonWilds::PropertyHelper::ParseByteFromJsonValue(number,value);return;}
            if(number->IsInteger()) {
                if(!value.is_number_integer())throw std::runtime_error("Integer stat requires a whole JSON number");
                if(value.is_number_unsigned()&&value.get<uint64_t>()>static_cast<uint64_t>(INT64_MAX))throw std::runtime_error("Integer stat exceeds the native writer range");
                const auto v=value.get<int64_t>();const auto bytes=number->GetElementSize();
                if(bytes!=1&&bytes!=2&&bytes!=4&&bytes!=8)throw std::runtime_error("Unsupported integer field width");
                const bool unsignedField=PS::HelpyPropertyValue::Kind(DragonWilds::PropertyHelper::GetPropertyTypeAsUTF8String(p))=="Unsigned";
                if(unsignedField) {
                    const uint64_t hi=bytes==8?static_cast<uint64_t>(INT64_MAX):(uint64_t{1}<<(bytes*8))-1;
                    if(v<0||static_cast<uint64_t>(v)>hi)throw std::runtime_error("Unsigned stat is outside the reflected field range");
                }else if(bytes<8) {
                    const auto limit=int64_t{1}<<(bytes*8-1);
                    if(v < -limit||v>=limit)throw std::runtime_error("Integer stat is outside the reflected field range");
                }
            }else {
                if(!value.is_number()||!std::isfinite(value.get<double>()))throw std::runtime_error("Numeric stat must be finite");
                if(number->GetElementSize()==sizeof(float)&&!std::isfinite(static_cast<float>(value.get<double>())))throw std::runtime_error("Stat exceeds native float range");
            }
        }
        if(value.is_string()&&value.get_ref<const std::string&>().find('\0')!=std::string::npos)
            throw std::runtime_error("Item text and name values cannot contain embedded NUL characters");
        DragonWilds::PropertyHelper::ValidateJsonValueType(p,value);
        if(auto* structure=CastField<FStructProperty>(p);structure && value.is_object()) {
            if(!structure->GetStruct())throw std::runtime_error("Unresolved clone struct type");
            for(const auto& [key,childValue]:value.items()) {
                if(key.empty() || key.front()=='$')throw std::runtime_error("Directives are not allowed inside live clone overrides");
                auto* child=DragonWilds::PropertyHelper::GetPropertyByName(structure->GetStruct().Get(),RC::to_generic_string(key));
                CloneValidateOverride(child,childValue,depth+1);
            }
        }else if(auto* array=CastField<FArrayProperty>(p)) {
            const nlohmann::json* items=&value;
            if(value.is_object()) {
                for(const auto& [key,unused]:value.items()) {
                    (void)unused;
                    if(key!="Action" && key!="Items")throw std::runtime_error("Live array edits allow only Action=Clear and Items; inline patches are forbidden");
                }
                if(value.contains("Action") && value.at("Action")!="Clear")throw std::runtime_error("Unsupported live array Action");
                if(!value.contains("Items"))return;
                items=&value.at("Items");
            }
            if(!items->is_array() || items->size()>4096)throw std::runtime_error("Clone array must contain at most 4096 values");
            for(const auto& v:*items)CloneValidateOverride(array->GetInner(),v,depth+1);
        }else if(auto* map=CastField<FMapProperty>(p)) {
            if(!value.is_array() || value.size()>4096)throw std::runtime_error("Clone map must be an array of at most 4096 key/value pairs");
            for(const auto& entry:value) {
                if(!entry.is_object() || entry.size()!=2 || !entry.contains("Key") || !entry.contains("Value"))
                    throw std::runtime_error("Each live map entry requires exactly Key and Value");
                CloneValidateOverride(map->GetKeyProp(),entry.at("Key"),depth+1);
                CloneValidateOverride(map->GetValueProp(),entry.at("Value"),depth+1);
            }
        }
    }
    bool CloneMapPointsTo(FMapProperty* property,UObject* subsystem,const FString& key,UObject* item) {
        bool found=false;
        UECustom::FScriptMapHelper map(property,property->ContainerPtrToValuePtr<void>(subsystem));
        map.ForEachPair([&](void* k,void* v){
            if(*static_cast<FString*>(k)!=key)return;
            UObject* object=nullptr;std::memcpy(&object,v,sizeof(object));found=object==item;
        });return found;
    }
    void CloneRemoveMapping(FMapProperty* property,UObject* subsystem,FString key,UObject* item) {
        if(!property || !CloneMapPointsTo(property,subsystem,key,item))return;
        UECustom::FScriptMapHelper map(property,property->ContainerPtrToValuePtr<void>(subsystem));
        if(!map.Remove(&key))throw std::runtime_error("Could not roll back a clone registry entry");
        map.Rehash();
        if(CloneMapPointsTo(property,subsystem,key,item))throw std::runtime_error("Clone registry rollback did not verify");
    }
#include "HelpyItemCompanions.inl"
}
namespace DragonWilds {
    nlohmann::json DragonWildsAssetModLoader::InspectToolClone(const std::string& sourcePath,bool requireCloneEligibility) {
        std::scoped_lock lock{m_mutex};
        PS::Authoring::ValidateObjectPath(sourcePath);
        PS::CookedAssets::TryEnsure();
        auto* source=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(sourcePath).c_str(),false);
        if(!source){auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(sourcePath)));source=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);}
        if(!source || !m_itemDataClass || !source->IsA(m_itemDataClass) || !IsReadyForPatch(source)
            ||RC::to_string(source->GetPathName())!=sourcePath
            || source->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
            throw std::runtime_error("Clone source must be a fully loaded ItemData asset, not a class default");
        if(requireCloneEligibility)HelpyAllowClone(source,false);
        auto fields=nlohmann::json::array();std::set<std::string> seen;
        for(auto* p:TFieldRange<FProperty>(source->GetClassPrivate(),EFieldIterationFlags::Default)) {
            const auto name=RC::to_string(p->GetName());if(!seen.insert(name).second)continue;
            if(fields.size()>=PS::Authoring::MaxCloneFields)throw std::runtime_error("Item has too many reflected fields for the live editor");
            nlohmann::json entry={{"Name",name},{"Type",PropertyHelper::GetPropertyTypeAsUTF8String(p)},
                {"Editable",CloneEditable(p)},{"HasValue",false},{"VisualType",CloneVisualType(p)},
                {"Kind",HelpyFieldKind(p)},{"NativeName",name},{"Scope","Item"}};
            if(!CloneEditable(p))entry["Reason"]="Identity, soft-delete, runtime-only, instanced or unsupported setter; not editable live";
            else {
                nlohmann::json value;
                if(CloneReadValue(p,source,value) && value.dump().size()<=PS::Authoring::MaxCloneValueBytes) {
                    entry["Value"]=std::move(value);entry["HasValue"]=true;
                } else entry["Reason"]="Inherited from source; supply schema-compatible JSON to override";
            }
            fields.push_back(std::move(entry));
        }
        // Discover linked row fields. A failed row remains visible as unsupported, never a partial snapshot.
        for(auto* p:TFieldRange<FProperty>(source->GetClassPrivate(),EFieldIterationFlags::Default)) {
            try {
                const auto row=HelpyReadRow(source,p);if(!row)continue;
                for(auto* child:TFieldRange<FProperty>(row->type,EFieldIterationFlags::Default)) {
                    const auto key=RC::to_string(child->GetName());if(!row->values.contains(key)||!PS::Authoring::SafeFieldName(key))continue;
                    if(fields.size()>=PS::Authoring::MaxCloneFields)throw std::runtime_error("Combined item/stat field limit reached");
                    fields.push_back({{"Name",PS::HelpyStatKey::Make(row->handle,key)},{"Type",PropertyHelper::GetPropertyTypeAsUTF8String(child)},
                        {"Kind",HelpyFieldKind(child)},{"Scope",row->tableName},{"NativeName",key},{"Table",row->tableName},{"Row",row->sourceRow},
                        {"Editable",CloneEditable(child)},{"HasValue",true},{"Value",row->values.at(key)},{"Reason","Owned stat row; source row is unchanged"}});
                }
            }catch(const std::exception& e){if(fields.size()<PS::Authoring::MaxCloneFields)fields.push_back({{"Name","@row-error/"+RC::to_string(p->GetName())},{"Type","Stat row"},{"Editable",false},{"HasValue",false},{"Reason",e.what()}});}
        }
        auto* soft=CloneSoftDelete(source->GetClassPrivate());
        nlohmann::json icon,name,power;
        CloneReadValue(PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Icon")),source,icon);
        CloneReadValue(PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Name")),source,name);
        CloneReadValue(PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("PowerLevel")),source,power);
        return {{"Name",name.is_string()&&!PS::QuickDecorations::MissingDisplayName(name.get<std::string>())?name.get<std::string>():PS::QuickDecorations::ReadableAssetName(sourcePath)},
            {"Icon",icon.is_string()?icon.get<std::string>():std::string{}},{"PowerLevel",power},{"Cooked",PS::CookedAssets::VerifiedLoadedObject(source)},
            {"JournalChoices",HelpyJournalChoices()},{"StationChoices",HelpyStationChoices()},
            {"Relations",HelpyItemRelations(sourcePath)},
            {"AppearanceGroup",PS::ItemAppearanceMetadata::Group(source)},
            {"Source",sourcePath},{"Class",RC::to_string(source->GetClassPrivate()->GetPathName())},
            {"Fields",std::move(fields)},{"SoftDeleteField",soft?RC::to_string(soft->GetName()):std::string{}},
            {"TemporarySource",m_temporaryToolClones.contains(sourcePath)}};
    }

    nlohmann::json DragonWildsAssetModLoader::ExportToolRecipe(const nlohmann::json& request) {
        std::scoped_lock lock{m_mutex};
        const auto sourcePath=request.at("Source").get<std::string>();PS::Authoring::ValidateObjectPath(sourcePath);
        auto* source=HelpyResolve(sourcePath);
        if(!m_itemDataClass||!source->IsA(m_itemDataClass)||!IsReadyForPatch(source))throw std::runtime_error("Recipe output must be a loaded ItemData asset");
        const auto key=PS::ClonePresentation::NewId("RuneSchemaRecipe");
        const auto document=HelpyRecipeDocument(request.at("Recipe"),sourcePath,key);
        PS::Authoring::StagedFile file("recipes",PS::Authoring::NewName(),document);file.Commit();
        return {{"State","Completed"},{"File",file.Path().string()},{"Key",key},{"RequiresRestart",true}};
    }

    nlohmann::json DragonWildsAssetModLoader::ExportToolOverrides(const nlohmann::json& request) {
        std::scoped_lock lock{m_mutex};
        const auto sourcePath=request.at("Source").get<std::string>();PS::Authoring::ValidateObjectPath(sourcePath);
        auto* source=HelpyResolve(sourcePath);
        if(!m_itemDataClass||!source->IsA(m_itemDataClass)||!IsReadyForPatch(source))throw std::runtime_error("Override target must be a loaded ItemData asset");
        const auto& changes=request.at("Overrides");
        if(!changes.is_object()||changes.empty()||changes.size()>PS::Authoring::MaxCloneFields||changes.dump().size()>PS::Authoring::MaxCloneDocumentBytes)
            throw std::runtime_error("Item overrides must be a bounded, non-empty object");
        nlohmann::json assetChanges=nlohmann::json::object();
        std::map<std::pair<std::string,std::string>,nlohmann::json> rowChanges;
        std::map<std::string,HelpyStatRow> rows;
        for(const auto& [key,value]:changes.items()) {
            if(const auto stat=PS::HelpyStatKey::Parse(key)) {
                if(!rows.contains(stat->handle)) {
                    auto* handle=PropertyHelper::GetPropertyByName(source->GetClassPrivate(),RC::to_generic_string(stat->handle));
                    auto row=HelpyReadRow(source,handle);if(!row)throw std::runtime_error("Linked stat row changed; inspect the item again");
                    rows.emplace(stat->handle,std::move(*row));
                }
                const auto found=rows.find(stat->handle);if(found==rows.end())throw std::runtime_error("Linked stat row changed; inspect the item again");
                auto* property=PropertyHelper::GetPropertyByName(found->second.type,RC::to_generic_string(stat->field));
                if(!CloneEditable(property)||!found->second.values.contains(stat->field))throw std::runtime_error("Unsupported linked item field: "+stat->field);
                CloneValidateOverride(property,value);rowChanges[{found->second.tableName,found->second.sourceRow}][stat->field]=value;
            }else {
                if(!PS::Authoring::SafeFieldName(key))throw std::runtime_error("Unsupported direct item field: "+key);
                auto* property=PropertyHelper::GetPropertyByName(source->GetClassPrivate(),RC::to_generic_string(key));
                if(!CloneEditable(property))throw std::runtime_error("Direct item field is no longer editable: "+key);
                CloneValidateOverride(property,value);assetChanges[key]=value;
            }
        }
        nlohmann::json rawDocument=nlohmann::json::array();
        for(auto& [identity,fields]:rowChanges)rawDocument.push_back({{"$Patch",identity.first+":"+identity.second},{"$Target",std::move(fields)}});
        const auto stem=PS::Authoring::NewName();std::unique_ptr<PS::Authoring::StagedFile> assetFile,rawFile;
        if(!assetChanges.empty())assetFile=std::make_unique<PS::Authoring::StagedFile>("assets",stem,nlohmann::json{{"Patch",{{"$Patch",sourcePath},{"$Target",assetChanges}}}});
        if(!rawDocument.empty())rawFile=std::make_unique<PS::Authoring::StagedFile>("raw",stem,rawDocument);
        nlohmann::json files=nlohmann::json::array();
        if(rawFile){rawFile->Commit();files.push_back(rawFile->Path().string());}
        if(assetFile){assetFile->Commit();files.push_back(assetFile->Path().string());}
        return {{"State","Completed"},{"Files",std::move(files)},{"AssetFields",assetChanges.size()},{"RawRows",rowChanges.size()},{"RequiresRestart",true}};
    }

    nlohmann::json DragonWildsAssetModLoader::CreateToolClone(const nlohmann::json& request,UWorld* world) {
        std::scoped_lock lock{m_mutex};
        if(!world || !request.value("AcknowledgeExperimental",false))
            throw std::runtime_error("Acknowledge experimental clone testing on a disposable/backed-up save first");
        if(m_toolCloneCount>=64)throw std::runtime_error("64 live-authored clones reached. Restart before creating more experimental items.");
        const auto sourcePath=request.at("Source").get<std::string>();
        const bool permanent=request.value("Permanent",false);
        PS::CookedAssets::TryEnsure();
        PS::Authoring::ValidateObjectPath(sourcePath);
        auto* source=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(sourcePath).c_str(),false);
        if(!source){auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(sourcePath)));source=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);}
        if(!source || !m_itemDataClass || !source->IsA(m_itemDataClass) || !IsReadyForPatch(source)
            ||RC::to_string(source->GetPathName())!=sourcePath
            || source->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
            throw std::runtime_error("The selected clone source is not fully loaded anymore; inspect it again");
        HelpyAllowClone(source,permanent);
        if(permanent && m_temporaryToolClones.contains(sourcePath))
            throw std::runtime_error("A permanent clone cannot depend on a temporary clone; choose an installed source");
        auto* type=source->GetClassPrivate();
        auto* soft=CloneSoftDelete(type);
        if(!permanent && !soft)
            throw std::runtime_error("No unambiguous bSoftDeleted/IsSoftDelete Boolean exists on this item. Temporary cloning is blocked; no item was created");
        auto fields=request.value("Overrides",nlohmann::json::object());
        if(!fields.is_object() || fields.size()>PS::Authoring::MaxCloneFields
            || fields.dump().size()>PS::Authoring::MaxCloneDocumentBytes)
            throw std::runtime_error("Clone overrides must be a bounded JSON object");
        if(permanent) {
            const auto flattened=fields.flatten();
            for(const auto& [key,value]:flattened.items()) {
                (void)key;
                if(value.is_string() && m_temporaryToolClones.contains(value.get<std::string>()))
                    throw std::runtime_error("A permanent item cannot reference a temporary tool-created item");
            }
        }
        const auto appearance=request.value("AppearanceSource",std::string{}),mesh=request.value("Mesh",std::string{});
        if(!appearance.empty()&&!mesh.empty())throw std::runtime_error("Copy appearance and direct mesh modes are mutually exclusive");
        if(!appearance.empty()) {
            PS::Authoring::ValidateObjectPath(appearance);
            auto* donor=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(appearance).c_str(),false);
            if(!donor){auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(appearance)));donor=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);}
            if(!donor||donor->GetClassPrivate()!=type||!IsReadyForPatch(donor)
                ||!PS::CookedAssets::VerifiedLoadedObject(donor)||RC::to_string(donor->GetPathName())!=appearance)
                throw std::runtime_error("Choose a loaded, cooked appearance donor of the same item class");
            HelpyAllowClone(donor,true);
            if(!PS::ClonePresentation::CompatibleAppearance(RC::to_string(type->GetPathName()),RC::to_string(donor->GetClassPrivate()->GetPathName()),
                PS::ItemAppearanceMetadata::Group(source),PS::ItemAppearanceMetadata::Group(donor)))
                throw std::runtime_error("Held appearance donors must have the same resolved item category; cross-family actor swaps are blocked");
            int copied=0;
            for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
                const auto key=RC::to_string(p->GetName());
                if(!PS::ClonePresentation::VisualField(key))continue;
                if(!CloneEditable(p))continue;
                if(fields.contains(key))throw std::runtime_error("Remove the advanced override for "+key+" before copying appearance");
                nlohmann::json value;
                if(!CloneReadValue(p,donor,value))throw std::runtime_error("Cannot safely serialize donor visual field: "+key);
                CloneRequireCookedReferences(value);fields[key]=std::move(value);++copied;
            }
            if(!copied)throw std::runtime_error("This item has no supported visual references to copy");
        }else if(!mesh.empty()) {
            const auto field=request.at("MeshField").get<std::string>();
            auto* property=PropertyHelper::GetPropertyByName(type,RC::to_generic_string(field));
            if(CloneVisualType(property).empty())throw std::runtime_error("Selected field is not a supported typed mesh reference");
            if(fields.contains(field))throw std::runtime_error("Remove the advanced override for this mesh slot first");
            CloneRequireCookedReferences(mesh);fields[field]=mesh;
        }
        const auto display=request.value("Name",std::string{});
        auto icon=request.value("Icon",std::string{});const auto iconMode=request.value("IconMode",std::string("Override"));
        if(iconMode!="Auto"&&iconMode!="Source"&&iconMode!="Appearance"&&iconMode!="Override")throw std::runtime_error("Unknown clone icon mode");
        if(iconMode!="Override") {
            UObject* iconSource=source;nlohmann::json value;
            if((iconMode=="Auto"||iconMode=="Appearance")&&!appearance.empty())iconSource=HelpyResolve(appearance);
            else if(iconMode=="Appearance")throw std::runtime_error("Appearance icon requires a donor");
            CloneReadValue(PropertyHelper::GetPropertyByName(iconSource->GetClassPrivate(),TEXT("Icon")),iconSource,value);
            if(iconMode=="Auto"&&(!value.is_string()||value.get<std::string>().empty()))CloneReadValue(PropertyHelper::GetPropertyByName(type,TEXT("Icon")),source,value);
            icon=value.is_string()?value.get<std::string>():std::string{};
        }
        if(display.size()>256 || display.find('\0')!=display.npos)throw std::runtime_error("Clone display name is too long or invalid");
        if(!display.empty() && !fields.contains("Name"))fields["Name"]=display;
        if(!icon.empty())CloneRequireCookedReferences(icon);
        if(!icon.empty() && !fields.contains("Icon")){PS::Authoring::ValidateObjectPath(icon);fields["Icon"]=icon;}
        for(const auto& [key,value]:fields.items()) {
            if(!PS::Authoring::SafeFieldName(key))throw std::runtime_error("Clone identity/directive/soft-delete field cannot be overridden: "+key);
            auto* p=PropertyHelper::GetPropertyByName(type,RC::to_generic_string(key));
            if(!CloneEditable(p))throw std::runtime_error("Not an editable item field: "+key);
            if(value.dump().size()>PS::Authoring::MaxCloneValueBytes)throw std::runtime_error("Clone field is too large: "+key);
            CloneValidateOverride(p,value);
        }
        auto* subsystemClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.ItemSubsystem"),false);
        if(!subsystemClass)throw std::runtime_error("ItemSubsystem is unavailable");
        TArray<UObject*> candidates;UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass,candidates,true);
        UObject* subsystem=nullptr;
        for(auto* candidate:candidates) {
            if(!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            auto* candidateWorld=candidate->GetWorld();if(candidateWorld && candidateWorld!=world)continue;
            if(subsystem && subsystem!=candidate)throw std::runtime_error("ItemSubsystem selection is ambiguous; no clone was created");
            subsystem=candidate;
        }
        if(!subsystem)throw std::runtime_error("No current ItemSubsystem is ready");
        auto* persistenceMap=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),TEXT("PersistenceIDToDataMap")));
        auto* internalMap=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),TEXT("InternalNameToDataMap")));
        if(!persistenceMap || !internalMap)throw std::runtime_error("ItemSubsystem registry contract is unavailable");
        const auto modTag=request.value("ModTag",std::string("RuneSchema"));
        if(modTag.size()>64)throw std::runtime_error("Mod tag exceeds 64 bytes");
        auto id=request.value("PersistenceID",std::string{});
        if(id.empty())id=PS::ClonePresentation::NewId(modTag);
        if(!PS::ClonePresentation::ValidId(id,modTag)||!IsCanonicalPersistenceId(id))
            throw std::runtime_error("Clone PersistenceID is not a canonical RS7 identifier for this mod tag");
        // Shared v5 file naming is retained so existing reload and save ownership work unchanged.
        const auto name="RS_v5_"+id;
        const auto target="/Game/RuneSchema/runeschema/Items/"+name+"."+name;
        const auto statChanges=request.value("StatOverrides",nlohmann::json::object());
        if(!statChanges.is_object()||statChanges.size()>32||statChanges.dump().size()>PS::Authoring::MaxCloneDocumentBytes)throw std::runtime_error("Invalid stat overrides");
        std::vector<HelpyStatRow> statRows;std::set<std::string> statHandles;auto rawDocument=nlohmann::json::object();
        for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            const auto handleName=RC::to_string(p->GetName());
            if(!PS::HelpyStatKey::Required(handleName)&&!statChanges.contains(handleName))continue;
            auto row=HelpyReadRow(source,p);
            if(!row){if(statChanges.contains(handleName))throw std::runtime_error("Selected stat handle is unavailable: "+handleName);continue;}
            if(fields.contains(handleName))throw std::runtime_error("Remove the direct row-handle override before authoring isolated stats: "+handleName);
            statHandles.insert(handleName);row->newRow=name+"_S"+std::to_string(statRows.size());
            if(row->table->FindRowUnchecked(FName(RC::to_generic_string(row->newRow),FNAME_Add)))throw std::runtime_error("Stat row identity already exists");
            if(statChanges.contains(handleName)) {
                const auto& changes=statChanges.at(handleName);if(!changes.is_object())throw std::runtime_error("Stat overrides must be objects");
                for(const auto& [field,value]:changes.items()) {
                    auto* property=PropertyHelper::GetPropertyByName(row->type,RC::to_generic_string(field));
                    if(!CloneEditable(property)||!row->values.contains(field))throw std::runtime_error("Unsupported stat field: "+field);
                    CloneValidateOverride(property,value);row->values[field]=value;
                }
            }
            for(const auto& [field,value]:row->values.items())CloneValidateOverride(PropertyHelper::GetPropertyByName(row->type,RC::to_generic_string(field)),value);
            fields[handleName]={{"DataTable",row->tablePath},{"RowName",row->newRow}};
            rawDocument[row->tableName][row->newRow]=row->values;statRows.push_back(std::move(*row));
        }
        for(const auto& [handle,unused]:statChanges.items())if(!statHandles.contains(handle))throw std::runtime_error("Unknown stat-table handle: "+handle);
        const auto journal=request.value("Journal",nlohmann::json::object()),recipe=request.value("Recipe",nlohmann::json::object());
        if((journal.value("Enabled",false)||recipe.value("Enabled",false))&&!permanent)throw std::runtime_error("Companion files require a permanent asset");
        auto recipeDocument=nlohmann::json::object(),journalDocument=nlohmann::json::object();
        if(recipe.value("Enabled",false))recipeDocument=HelpyRecipeDocument(recipe,target,PS::ClonePresentation::NewId(modTag));
        if(journal.value("Enabled",false))journalDocument=HelpyJournalDocument(journal,target,PS::ClonePresentation::NewId(modTag),icon);
        fields["$Clone"]=sourcePath;fields["InternalName"]=name;fields["PersistenceID"]=id;
        // Explicit owned handles above win; otherwise preserve an inherited row rather than redirecting it blindly.
        fields["$InheritEquipmentStats"]=true;
        if(soft)fields[RC::to_string(soft->GetName())]=!permanent;
        PS::AssetMetadata::Declaration declaration;declaration.modded=true;declaration.runeSchema=true;declaration.cooked=false;declaration.safeToClone=true;
        PendingAsset pending{RC::to_generic_string(target),RC::to_generic_string(target),TEXT("runeschema"),fields,false,declaration,permanent};
        auto savedFields=fields;savedFields["Modded"]={{"RuneSchema",true},{"Cooked",false},{"SafeToClone",true}};
        const auto document=nlohmann::json{{target,savedFields}};
        std::unique_ptr<PS::Authoring::StagedFile> file;
        std::vector<std::unique_ptr<PS::Authoring::StagedFile>> companions;
        if(permanent) {
            file=std::make_unique<PS::Authoring::StagedFile>("assets",name,document);
            if(!rawDocument.empty())companions.push_back(std::make_unique<PS::Authoring::StagedFile>("raw",name,rawDocument));
            if(!recipeDocument.empty())companions.push_back(std::make_unique<PS::Authoring::StagedFile>("recipes",name,recipeDocument));
            if(!journalDocument.empty())companions.push_back(std::make_unique<PS::Authoring::StagedFile>("journal",name,journalDocument));
        }
        auto installedFiles=nlohmann::json::array();std::string fileIssue;
        const auto rootCheckpoint=m_createdAssets.size();
        UObject* created=nullptr;bool registered=false;
        const auto pid=FString(RC::to_generic_string(id).c_str()),internal=FString(RC::to_generic_string(name).c_str());
        try {
            for(auto& row:statRows) {
                FManagedStruct value(row.type);
                for(const auto& [key,v]:row.values.items())PropertyHelper::CopyJsonValueToContainer(value.GetData(),PropertyHelper::GetPropertyByName(row.type,RC::to_generic_string(key)),v);
                row.table->AddRow(FName(RC::to_generic_string(row.newRow),FNAME_Add),*reinterpret_cast<FTableRowBase*>(value.GetData()));
                row.installed=row.table->FindRowUnchecked(FName(RC::to_generic_string(row.newRow),FNAME_Add));
                if(!row.installed)throw std::runtime_error("Failed to install owned stat row");
                for(const auto& [key,v]:row.values.items()){nlohmann::json check;if(!CloneReadValue(PropertyHelper::GetPropertyByName(row.type,RC::to_generic_string(key)),row.installed,check)||!CloneValueMatches(check,v))throw std::runtime_error("Cannot read back owned stat field: "+key);}
            }
            created=CreateFromClone(pending,subsystem);
            LoadResult result{};Apply(created,pending,result);
            if(result.ErrorCount)throw std::runtime_error("Clone field application failed; see the per-field log. No inventory grant was attempted");
            if(soft) {
                auto* flag=CloneSoftDelete(created->GetClassPrivate());
                if(!flag || flag->GetPropertyValue(flag->ContainerPtrToValuePtr<void>(created))!=!permanent)
                    throw std::runtime_error("Clone soft-delete value failed readback");
            }
            if(!permanent)created->SetFlags(RF_Transient);
            if(!RegisterCreatedItem(created,pending,subsystem)
                || !CloneMapPointsTo(persistenceMap,subsystem,pid,created)
                || !CloneMapPointsTo(internalMap,subsystem,internal,created))
                throw std::runtime_error("Clone registration failed verification");
            registered=true;
            if(!file)m_temporaryToolClones.insert(target);

        }catch(...) {
            if(file)m_toolAssetFiles.erase(file->Path().lexically_normal());
            for(auto& row:statRows)if(row.installed&&row.table->FindRowUnchecked(FName(RC::to_generic_string(row.newRow),FNAME_Add))==row.installed)row.table->RemoveRow(FName(RC::to_generic_string(row.newRow),FNAME_Add));
            if(created) {
                // Remove only entries still pointing at this new object, never another item.
                try {
                    CloneRemoveMapping(persistenceMap,subsystem,pid,created);
                    CloneRemoveMapping(internalMap,subsystem,pid,created);
                    CloneRemoveMapping(internalMap,subsystem,internal,created);
                }catch(...) {
                    throw std::runtime_error("Clone registry rollback could not be verified. No grant was attempted; restart before further clone tests");
                }
                PS::AssetMetadata::Forget(created);
                ClearItemIdentity(created,created->GetClassPrivate());
                std::erase_if(m_createdAssetsByTarget,[&](const auto& entry){return entry.second==created;});
                auto* package=created->GetOuterPrivate();
                if(created->IsRootSet())created->ClearRootSet();
                std::erase(m_createdAssets,created);
                if(package && package->IsRootSet()){package->ClearRootSet();std::erase(m_createdAssets,package);}
            }
            // Construction can fail after rooting a package, before returning
            // the clone pointer. Release any remaining roots added by this attempt.
            while(m_createdAssets.size()>rootCheckpoint) {
                auto* owned=m_createdAssets.back();m_createdAssets.pop_back();
                if(owned && owned->IsRootSet())owned->ClearRootSet();
            }
            throw;
        }
        auto pendingFiles=nlohmann::json::array();
        if(file) {
            std::vector<PS::Authoring::StagedFile*> order;
            for(auto& companion:companions)if(companion->Path().parent_path().filename()=="raw")order.push_back(companion.get());
            order.push_back(file.get());
            for(auto& companion:companions)if(companion->Path().parent_path().filename()!="raw")order.push_back(companion.get());
            try {
                m_toolAssetFiles.insert(file->Path().lexically_normal());
                fileIssue=PS::HelpyBundlePublish::Publish(order,[&](const auto& document){installedFiles.push_back(document.Path().string());});
            }catch(const std::exception& e){fileIssue=e.what();}
            catch(...){fileIssue="Unexpected publication failure; inspect pending companion files with the game closed";}
            if(!fileIssue.empty()) {
                // Never erase a published definition or guess a cross-directory rollback.
                // Keep every remaining staging file for repair and block this item from grants/cloning.
                PS::AssetMetadata::MarkIncomplete(created);
                for(auto* document:order)if(!document->Committed()){document->PreservePending();pendingFiles.push_back(document->PendingPath().string());}
                if(!file->Committed())m_toolAssetFiles.erase(file->Path().lexically_normal());
            }
        }
        ++m_toolCloneCount;
        // Nonessential diagnostics must not turn a committed creation into a retryable failure.
        try {PS::AssetProvenance::Record(created,sourcePath,"runeschema",true,registered,0,fields);}
        catch(...) {}
        return {{"ModTag",modTag},{"AppearanceSource",appearance},{"Mesh",mesh},{"State","Created"},{"Path",target},{"PersistenceID",id},{"InternalName",name},
            {"Permanent",permanent},{"File",file?file->Path().string():std::string{}},{"Files",installedFiles},
            {"PendingFiles",pendingFiles},{"CompanionsReady",fileIssue.empty()},{"CompanionError",fileIssue},{"StatRows",statRows.size()},
            {"CompanionsNeedRestart",!recipeDocument.empty()||!journalDocument.empty()},
            {"SoftDeleteField",soft?RC::to_string(soft->GetName()):std::string{}}};
    }
} // namespace DragonWilds
