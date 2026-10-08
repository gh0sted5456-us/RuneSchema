// Included inside LiveCloneAuthoring's anonymous namespace. Native calls run only
// on the existing game-thread authoring pump. No table pointers are cached to disk.
struct HelpyStatRow {
    std::string handle,tableName,tablePath,sourceRow,newRow;
    FStructProperty* property=nullptr;
    UDataTable* table=nullptr;
    UScriptStruct* type=nullptr;
    nlohmann::json values=nlohmann::json::object();
    uint8* installed=nullptr;
};
bool HelpyReady(UObject* object) {
    return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|
        RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedInitialization|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects));
}
UObject* HelpyResolve(const std::string& path) {
    PS::Authoring::ValidateObjectPath(path);
    auto* object=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,RC::to_generic_string(path).c_str(),false);
    if(!object){auto ref=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(RC::to_generic_string(path)));object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(ref);}
    if(!HelpyReady(object)||RC::to_string(object->GetPathName())!=path)throw std::runtime_error("Cannot resolve ready asset: "+path);
    return object;
}
void HelpyAllowClone(UObject* object,bool permanent) {
    if(PS::AssetMetadata::IsIncomplete(object))throw std::runtime_error("This item has incomplete companion files. Repair with the game closed before further cloning.");
    const auto provenance=PS::AssetProvenance::Lookup(object);
    const bool runtime=provenance.is_object()&&provenance.value("Kind",std::string{})=="RuneSchemaAssetClone"&&provenance.value("Confirmed",false);
    std::string reason;
    if(!PS::AssetMetadata::Allowed(PS::AssetMetadata::Lookup(object),PS::CookedAssets::VerifiedLoadedObject(object),runtime,
        provenance.is_object()&&provenance.value("Registered",false),PS::QuickDecorations::ModPath(RC::to_string(object->GetPathName())),
        PS::AssetMetadata::HasInstalledDefinition(object),permanent,reason))throw std::runtime_error(reason);
}
std::string HelpyFieldKind(FProperty* p) {
    if(DragonWilds::PropertyHelper::CastProperty<FEnumProperty>(p))return "Enum";
    if(auto* n=CastField<FNumericProperty>(p);n&&n->IsEnum())return "Enum";
    if(CastField<FObjectProperty>(p)||CastField<FSoftObjectProperty>(p)||CastField<FClassProperty>(p)||CastField<FSoftClassProperty>(p))return "Reference";
    return PS::HelpyPropertyValue::Kind(DragonWilds::PropertyHelper::GetPropertyTypeAsUTF8String(p));
}
std::optional<HelpyStatRow> HelpyReadRow(UObject* item,FProperty* p) {
    auto* handle=CastField<FStructProperty>(p);
    if(!handle||!handle->GetStruct()||handle->GetArrayDim()!=1||handle->HasAnyPropertyFlags(CloneUnsafeFlags))return {};
    auto* tableProperty=CastField<FObjectProperty>(DragonWilds::PropertyHelper::GetPropertyByName(handle->GetStruct().Get(),TEXT("DataTable")));
    auto* rowProperty=CastField<FNameProperty>(DragonWilds::PropertyHelper::GetPropertyByName(handle->GetStruct().Get(),TEXT("RowName")));
    if(!tableProperty||!rowProperty)return {};
    auto* data=handle->ContainerPtrToValuePtr<void>(item);
    auto* object=tableProperty->GetObjectPropertyValue(tableProperty->ContainerPtrToValuePtr<void>(data));
    if(!object)return {};
    auto* tableClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.DataTable"),false);
    if(!tableClass||!object->IsA(tableClass)||!HelpyReady(object))throw std::runtime_error("Stat DataTable is not ready");
    if(object->GetClassPrivate()->GetPathName()!=TEXT("/Script/Engine.DataTable"))throw std::runtime_error("Composite/custom stat tables require explicit loader support; source table unchanged");
    auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();
    const auto row=*rowProperty->ContainerPtrToValuePtr<FName>(data);
    auto* value=table->FindRowUnchecked(row);if(!type||!value)throw std::runtime_error("Source stat row is unavailable");
    HelpyStatRow result;result.handle=RC::to_string(p->GetName());result.property=handle;result.table=table;result.type=type;
    result.tableName=RC::to_string(table->GetName());result.tablePath=RC::to_string(table->GetPathName());result.sourceRow=RC::to_string(row.ToString());
    // /raw routes by DataTable name. Refuse an ambiguous name instead of writing to a second table.
    TArray<UObject*> tables;UECustom::UObjectGlobals::GetObjectsOfClass(tableClass,tables,true);
    if(tables.Num()<0||tables.Num()>8192)throw std::runtime_error("Table roster exceeds the authoring limit");
    for(auto* other:tables)if(other!=table&&HelpyReady(other)&&other->GetFName()==table->GetFName())throw std::runtime_error("Stat table name is ambiguous for /raw: "+result.tableName);
    for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        if(field->HasAnyPropertyFlags(CloneUnsafeFlags))continue;
        const auto key=RC::to_string(field->GetName());nlohmann::json v;
        if(!CloneReadValue(field,value,v)||!CloneHasSetter(field))throw std::runtime_error("Stat row has an unsupported export field: "+result.tableName+"."+key);
        if(result.values.size()>=PS::Authoring::MaxCloneFields)throw std::runtime_error("Stat row field limit reached");
        result.values[key]=std::move(v);
    }
    if(result.values.empty()||result.values.dump().size()>PS::Authoring::MaxCloneDocumentBytes)throw std::runtime_error("Stat row is empty or exceeds the authoring limit");
    return result;
}
nlohmann::json HelpyJournalChoices() {
    auto out=nlohmann::json::array();auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalSubCategoryData"),false);
    if(!type)return out;TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(type,objects,true);
    if(objects.Num()<0||objects.Num()>4096)return out;
    for(auto* object:objects) {
        if(!HelpyReady(object))continue;
        const auto cls=RC::to_string(object->GetClassPrivate()->GetPathName());
        const bool grouped=cls=="/Script/Dominion.JournalSubCategoryByGroupData";
        // Current journal loader has these two placement contracts. Do not promise biome placement.
        if(!grouped&&cls!="/Script/Dominion.JournalSubCategoryNoBiomeData")continue;
        const auto path=RC::to_string(object->GetPathName());nlohmann::json name;
        CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(object->GetClassPrivate(),TEXT("Name")),object,name);
        const auto label=name.is_string()&&!PS::QuickDecorations::MissingDisplayName(name.get<std::string>())?name.get<std::string>():PS::QuickDecorations::ReadableAssetName(path);
        out.push_back({{"Id",path},{"Name",label},{"Path",path},{"Grouped",grouped}});
    }
    return out;
}
// Only native recipe placement structures accepted by the existing /recipes loader.
std::vector<std::string> HelpyRecipeArrays(UScriptStruct* type) {
    std::vector<std::string> fields;
    for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        auto* a=CastField<FArrayProperty>(p);if(!a||a->GetArrayDim()!=1)continue;
        if(RC::to_string(p->GetName())=="LabeledRecipes") {
            auto* category=CastField<FStructProperty>(a->GetInner());if(!category||!category->GetStruct())continue;
            auto* collection=CastField<FArrayProperty>(DragonWilds::PropertyHelper::GetPropertyByName(category->GetStruct().Get(),TEXT("Collection")));
            if(collection&&CastField<FSoftObjectProperty>(collection->GetInner())&&DragonWilds::PropertyHelper::GetPropertyByName(category->GetStruct().Get(),TEXT("Label")))fields.emplace_back("");
        }else if(auto* inner=CastField<FObjectProperty>(a->GetInner())) {
            auto* cls=inner->GetPropertyClass().Get();if(cls&&RC::to_string(cls->GetName())=="RecipeData")fields.push_back(RC::to_string(p->GetName()));
        }
    }return fields;
}
nlohmann::json HelpyStationChoices() {
    auto out=nlohmann::json::array();auto* tableClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.DataTable"),false);
    if(!tableClass)return out;TArray<UObject*> tables;UECustom::UObjectGlobals::GetObjectsOfClass(tableClass,tables,true);
    if(tables.Num()<0||tables.Num()>8192)return out;
    std::unordered_map<std::string,std::size_t> tableNameCounts;
    for(auto* object:tables)if(HelpyReady(object))++tableNameCounts[RC::to_string(object->GetName())];
    std::size_t rowsVisited=0;
    for(auto* object:tables) {
        if(!HelpyReady(object))continue;const auto path=RC::to_string(object->GetPathName());
        if(PS::QuickDecorations::Fold(path).find("vendor")!=std::string::npos)continue;
        auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();if(!type)continue;
        const auto arrays=HelpyRecipeArrays(type);if(arrays.empty())continue;
        if(tableNameCounts[RC::to_string(object->GetName())]!=1)continue;
        for(const auto& [rowName,row]:table->GetRowMap()) {
            if(++rowsVisited>65536)return out;
            if(!row)continue;nlohmann::json display;
            for(const auto* key:{TEXT("DisplayName"),TEXT("Name"),TEXT("StationName")}) {
                if(CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(type,key),row,display)&&display.is_string())break;
            }
            nlohmann::json stationIcon;
            for(const auto* key:{TEXT("DisplayIcon"),TEXT("Icon"),TEXT("StationIcon")})
                if(CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(type,key),row,stationIcon)&&stationIcon.is_string())break;
            const auto rowText=RC::to_string(rowName.ToString());
            const auto label=display.is_string()?display.get<std::string>():rowText;
            for(const auto& array:arrays) {
                if(out.size()>=4096)return out;
                out.push_back({{"Id",path+":"+rowText+":"+array},{"Name",label+" / "+RC::to_string(table->GetName())+(array.empty()?"":" / "+array)},
                    {"Path",path},{"Row",rowText},{"Array",array},{"Icon",stationIcon.is_string()?stationIcon.get<std::string>():std::string{}}});
            }
        }
    }return out;
}
nlohmann::json HelpyItemRelations(const std::string& itemPath) {
    nlohmann::json recipes=nlohmann::json::array(),journals=nlohmann::json::array();
    std::set<std::string> recipePaths;
    auto* recipeClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.RecipeData"),false);
    if(recipeClass) {
        TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(recipeClass,objects,true);
        if(objects.Num()>=0&&objects.Num()<=32768)for(auto* recipe:objects) {
            if(!HelpyReady(recipe)||recipes.size()>=512)continue;
            nlohmann::json outputs,ingredients;
            if(!CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(recipe->GetClassPrivate(),TEXT("ItemsCreated")),recipe,outputs)
                ||!outputs.is_array()||outputs.dump().find(itemPath)==std::string::npos)continue;
            CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(recipe->GetClassPrivate(),TEXT("ItemsConsumed")),recipe,ingredients);
            const auto path=RC::to_string(recipe->GetPathName());recipePaths.insert(path);
            recipes.push_back({{"Path",path},{"Name",PS::QuickDecorations::ReadableAssetName(path)},
                {"Ingredients",ingredients.is_array()?ingredients:nlohmann::json::array()},{"Outputs",std::move(outputs)},{"Stations",nlohmann::json::array()}});
        }
    }
    // Recipe assets do not own their station. Find loaded table rows whose native
    // recipe arrays reference each matching recipe and report the exact table/row.
    if(!recipePaths.empty()) {
        auto* tableClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.DataTable"),false);
        TArray<UObject*> tables;if(tableClass)UECustom::UObjectGlobals::GetObjectsOfClass(tableClass,tables,true);
        std::size_t visited=0;
        if(tables.Num()>=0&&tables.Num()<=8192)for(auto* object:tables) {
            if(!HelpyReady(object))continue;auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();if(!type)continue;
            const auto arrays=HelpyRecipeArrays(type);if(arrays.empty())continue;
            for(const auto& [rowName,row]:table->GetRowMap()) {
                if(!row||++visited>65536)break;
                nlohmann::json display;for(const auto* key:{TEXT("DisplayName"),TEXT("Name"),TEXT("StationName")})
                    if(CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(type,key),row,display)&&display.is_string())break;
                nlohmann::json stationIcon;for(const auto* key:{TEXT("DisplayIcon"),TEXT("Icon"),TEXT("StationIcon")})
                    if(CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(type,key),row,stationIcon)&&stationIcon.is_string())break;
                const auto rowText=RC::to_string(rowName.ToString());const auto station=display.is_string()?display.get<std::string>():rowText;
                for(const auto& array:arrays) {
                    const auto propertyName=array.empty()?RC::StringType(TEXT("LabeledRecipes")):RC::to_generic_string(array);
                    auto* property=DragonWilds::PropertyHelper::GetPropertyByName(type,propertyName);
                    nlohmann::json values;if(!CloneReadValue(property,row,values))continue;const auto encoded=values.dump();
                    for(auto& recipe:recipes)if(encoded.find(recipe.at("Path").get<std::string>())!=std::string::npos&&recipe["Stations"].size()<32)
                        recipe["Stations"].push_back({{"Name",station},{"Path",RC::to_string(table->GetPathName())},{"Table",RC::to_string(table->GetName())},{"Row",rowText},{"Array",array},{"Icon",stationIcon.is_string()?stationIcon.get<std::string>():std::string{}}});
                }
            }
            if(visited>65536)break;
        }
    }
    auto* journalClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalEntryData"),false);
    if(journalClass) {
        TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(journalClass,objects,true);
        if(objects.Num()>=0&&objects.Num()<=16384)for(auto* entry:objects) {
            if(!HelpyReady(entry)||journals.size()>=512)continue;bool linked=false;
            for(const auto* field:{TEXT("ItemData"),TEXT("JournalItemData"),TEXT("RecipeData")}) {
                nlohmann::json value;if(!CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),field),entry,value))continue;
                if(value.is_string()&&(value.get<std::string>()==itemPath||recipePaths.contains(value.get<std::string>())))linked=true;
            }
            if(!linked)continue;nlohmann::json name;
            CloneReadValue(DragonWilds::PropertyHelper::GetPropertyByName(entry->GetClassPrivate(),TEXT("DisplayName")),entry,name);
            const auto path=RC::to_string(entry->GetPathName());journals.push_back({{"Path",path},{"Name",name.is_string()?name.get<std::string>():PS::QuickDecorations::ReadableAssetName(path)}});
        }
    }
    return {{"Recipes",std::move(recipes)},{"JournalEntries",std::move(journals)}};
}
// Output generation is intentionally separate from placement execution. The existing
// journal and recipe loaders consume these files on restart/hot-reload; Helpy does
// not claim those loaders have successfully placed a document merely by saving it.
nlohmann::json HelpyRecipeDocument(const nlohmann::json& option,const std::string& target,const std::string& key) {
    const auto& station=option.at("Station");const auto path=station.at("Path").get<std::string>();
    auto* object=HelpyResolve(path);auto* cls=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.DataTable"),false);
    if(!cls||!object->IsA(cls))throw std::runtime_error("Recipe station must reference a DataTable");
    auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();
    TArray<UObject*> tables;UECustom::UObjectGlobals::GetObjectsOfClass(cls,tables,true);
    if(tables.Num()<0||tables.Num()>8192)throw std::runtime_error("Table roster exceeds the authoring limit");
    for(auto* other:tables)if(other!=object&&HelpyReady(other)&&other->GetFName()==object->GetFName())throw std::runtime_error("Ambiguous /recipes table name; no companion saved");
    const auto row=station.at("Row").get<std::string>(),array=station.value("Array",std::string{});
    if(!type||!table->FindRowUnchecked(FName(RC::to_generic_string(row),FNAME_Add)))throw std::runtime_error("Recipe station row is missing");
    const auto accepted=HelpyRecipeArrays(type);
    if(std::find(accepted.begin(),accepted.end(),array)==accepted.end())throw std::runtime_error("Recipe station placement type changed");
    const auto category=option.value("Category",std::string{});
    if(array.empty()&&(category.empty()||category.size()>128))throw std::runtime_error("Recipe category must contain 1..128 bytes");
    const auto& ingredients=option.at("Ingredients");if(!ingredients.is_array()||ingredients.empty()||ingredients.size()>16)throw std::runtime_error("Recipe needs 1..16 ingredient rows");
    std::set<std::string> seen;auto* itemClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.ItemData"),false);
    for(const auto& ingredient:ingredients) {
        const auto itemPath=ingredient.at("ItemData").get<std::string>();const auto n=ingredient.at("Count").get<int>();
        if(n<1||n>10000||!seen.insert(itemPath).second)throw std::runtime_error("Invalid or duplicate recipe ingredient");
        auto* item=HelpyResolve(itemPath);if(!itemClass||!item->IsA(itemClass))throw std::runtime_error("Recipe ingredient is not ItemData");
        const auto origin=PS::AssetProvenance::Lookup(item);
        if(origin.is_object()&&(!origin.value("Registered",false)||!PS::AssetMetadata::HasInstalledDefinition(item)))throw std::runtime_error("A saved recipe cannot use a session-only or unregistered ingredient");
    }
    const auto n=option.value("Count",1);if(n<1||n>10000)throw std::runtime_error("Invalid recipe output count");
    nlohmann::json placement={{"Table",RC::to_string(table->GetName())},{"Row",row}};
    if(array.empty())placement["Category"]=category;else placement["Array"]=array;
    return {{key,{{"Properties",{{"ItemsConsumed",ingredients},{"ItemsCreated",nlohmann::json::array({{{"ItemData",target},{"Count",n}}})},
        {"PersistenceID",key},{"InternalName","Recipe_"+key}}},{"Unlock",option.value("Unlock",false)},
        {"AddTo",nlohmann::json::array({placement})}}}};
}
nlohmann::json HelpyJournalDocument(const nlohmann::json& option,const std::string& target,const std::string& key,const std::string& icon) {
    const auto path=option.at("Target").at("Path").get<std::string>();auto* category=HelpyResolve(path);
    const auto classPath=RC::to_string(category->GetClassPrivate()->GetPathName());
    const bool grouped=classPath=="/Script/Dominion.JournalSubCategoryByGroupData";
    if(!grouped&&classPath!="/Script/Dominion.JournalSubCategoryNoBiomeData")throw std::runtime_error("Unsupported journal category placement");
    const auto title=option.value("Title",std::string{}),description=option.value("Text",std::string{});
    if(title.empty()||title.size()>256||description.size()>PS::Authoring::MaxCloneValueBytes)throw std::runtime_error("Invalid journal title/description length");
    nlohmann::json add={{"SubCategory",path},{"Key",key}};
    if(grouped) {
        const auto group=option.value("Group",std::string{}),name=option.value("GroupName",std::string{});
        if(group.empty()||group.size()>128||name.empty()||name.size()>128)throw std::runtime_error("Grouped journal entries need group ID and display name");
        add["Group"]={{"Id",group},{"DisplayName",name},{"CreateIfMissing",true}};
    }
    // Lore uses the native PageDescriptions[].Description contract.
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalEntryKnowLoreData"),false);
    if(!type)throw std::runtime_error("Native lore journal entry class is unavailable");
    nlohmann::json body={{"Type","Lore"},{"DisplayName",title},{"PageDescriptions",nlohmann::json::array({{{"Description",description}}})},{"Image",icon.empty()?nlohmann::json(nullptr):nlohmann::json(icon)},
        {"JournalItemData",target},{"PersistenceID",key},{"InternalName","Journal_"+key},{"AddTo",add},{"Unlock",true}};
    for(const auto& [field,value]:body.items()) {
        if(field=="Type"||field=="AddTo"||field=="Unlock"||field=="JournalItemData")continue;
        auto* p=DragonWilds::PropertyHelper::GetPropertyByName(type,RC::to_generic_string(field));
        CloneValidateOverride(p,value);
    }
    auto* itemField=CastField<FSoftObjectProperty>(DragonWilds::PropertyHelper::GetPropertyByName(type,TEXT("JournalItemData")));
    if(!itemField||!itemField->GetPropertyClass())throw std::runtime_error("JournalItemData reference contract is missing");
    return {{key,body}};
}
