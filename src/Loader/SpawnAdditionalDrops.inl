void DragonWildsSpawnLoader::ClearBonusRows() {
    for(const auto& saved:m_bonusHandles)if(auto* component=saved.Component.Get())try {
        auto* handle=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("EnemyTableRowHandle")));
        auto* name=handle?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(handle->GetStruct().Get(),TEXT("RowName"))):nullptr;
        if(name && name->GetElementSize()==sizeof(FName) && name->GetArrayDim()==1) {
            auto* address=name->ContainerPtrToValuePtr<void>(handle->ContainerPtrToValuePtr<void>(component));
            if(name->GetPropertyValue(address)==FName(saved.Applied,FNAME_Find))name->SetPropertyValue(address,FName(saved.Original,FNAME_Add));
        }
    }catch(...){}
    m_bonusHandles.clear();
    for(auto it=m_bonusRows.rbegin();it!=m_bonusRows.rend();++it)if(auto* object=it->Table.Get()) {
        auto* table=static_cast<UDataTable*>(object);const FName name(it->Name,FNAME_Find);
        if(table->FindRowUnchecked(name)==it->Data)table->RemoveRow(name);
        if(it->Rooted)table->ClearRootSet();
    }
    m_bonusRows.clear();m_bonusRowCache.clear();m_bonusApplied.clear();
}

namespace {
struct ResourceDropLayout {
    UObject* Owner{};
    FArrayProperty* Items{};
    FStructProperty* Entry{};
    FProperty* Item{};
    FNumericProperty* Min{};
    FNumericProperty* Max{};
    FNumericProperty* Chance{};
    FNumericProperty* MaximumGrouping{};
    bool ChanceIsProbability=false;
};

FProperty* FindFirstProperty(UScriptStruct* type,std::initializer_list<const TCHAR*> names) {
    if(!type)return nullptr;
    for(const auto* name:names)if(auto* property=PropertyHelper::GetPropertyByName(type,name))return property;
    return nullptr;
}

FNumericProperty* FindFirstNumeric(UScriptStruct* type,std::initializer_list<const TCHAR*> names) {
    return CastField<FNumericProperty>(FindFirstProperty(type,names));
}

bool BuildResourceDropLayout(UObject* owner,ResourceDropLayout& layout) {
    if(!owner)return false;
    auto* items=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(owner->GetClassPrivate(),TEXT("ItemsToDrop")));
    auto* entry=items?CastField<FStructProperty>(items->GetInner()):nullptr;
    auto* type=entry?entry->GetStruct().Get():nullptr;
    if(!items || !entry || !type)return false;

    auto* item=FindFirstProperty(type,{TEXT("ItemDataClass"),TEXT("ItemData"),TEXT("SpawnedItemData"),TEXT("ItemToDrop"),TEXT("Item")});
    const bool itemSupported=item && (CastField<FObjectProperty>(item) || CastField<FSoftObjectProperty>(item));
    auto* min=FindFirstNumeric(type,{TEXT("MinToDrop"),TEXT("MinimumDropAmount")});
    auto* max=FindFirstNumeric(type,{TEXT("MaxToDrop"),TEXT("MaximumDropAmount")});
    auto* probability=FindFirstNumeric(type,{TEXT("ProbabilityOfDrop"),TEXT("ChanceOfDrop")});
    auto* percent=FindFirstNumeric(type,{TEXT("DropChance"),TEXT("ChancePercent")});
    auto* grouping=FindFirstNumeric(type,{TEXT("MaximumGrouping")});
    if(!itemSupported || !min || !max || !min->IsInteger() || !max->IsInteger())return false;
    if(probability && !probability->IsFloatingPoint())return false;
    if(percent && !(percent->IsFloatingPoint() || percent->IsInteger()))return false;
    if(grouping && !grouping->IsInteger())grouping=nullptr;

    layout={owner,items,entry,item,min,max,probability?probability:percent,grouping,probability!=nullptr};
    return true;
}

ResourceDropLayout FindResourceDropLayout(UObject* actor) {
    ResourceDropLayout layout{};
    // Prefer a one-shot depletion/destruction source. Fall back to the node's
    // ordinary drop component, then split drops, then an actor-owned array.
    for(const auto* name:{TEXT("ItemDropOnDestructionComponent"),TEXT("ItemDropComponent"),TEXT("ItemDropOnSplitComponent")}) {
        UObject* component=nullptr;
        try {component=ActorHelper::GetObjectRef(actor,name);} catch(...) {}
        if(BuildResourceDropLayout(component,layout))return layout;
    }
    BuildResourceDropLayout(actor,layout);
    return layout;
}

void SetResourceNumeric(FNumericProperty* property,void* container,double value) {
    if(!property)return;
    auto* target=property->ContainerPtrToValuePtr<void>(container);
    if(property->IsInteger())property->SetIntPropertyValue(target,static_cast<int64>(std::llround(value)));
    else if(property->IsFloatingPoint())property->SetFloatingPointPropertyValue(target,value);
    else throw std::runtime_error("Unsupported resource drop numeric field");
}

bool ApplyAdditionalResourceDrops(UObject* actor,const nlohmann::json& drops) {
    auto layout=FindResourceDropLayout(actor);
    if(!layout.Owner || !layout.Items || !layout.Entry || !layout.Item || !layout.Min || !layout.Max)return false;

    // Resolve every item and materialize every entry before mutating the live
    // component. Unsupported layouts therefore fail without a partial append.
    std::vector<std::unique_ptr<UECustom::FManagedValue>> prepared;
    prepared.reserve(drops.size());
    for(const auto& drop:drops) {
        const auto path=drop.at("Item").get<std::string>();
        if(!ActorHelper::ResolveObject(RC::to_generic_string(path)))
            throw std::runtime_error("Additional resource drop item unavailable: "+path);
        if(!layout.Chance && drop.at("ChancePercent").get<double>()!=100.0)
            throw std::runtime_error("Resource drop layout has no chance field; only 100% additional drops are supported");

        auto value=std::make_unique<UECustom::FManagedValue>();
        UECustom::FScriptArrayHelper helper(layout.Items->ContainerPtrToValuePtr<FScriptArray>(layout.Owner),layout.Items);
        helper.InitializeValue(*value);
        auto* data=value->GetData();
        PropertyHelper::CopyJsonValueToContainer(data,layout.Item,path);
        SetResourceNumeric(layout.Min,data,drop.at("Min").get<int>());
        SetResourceNumeric(layout.Max,data,drop.at("Max").get<int>());
        if(layout.Chance)SetResourceNumeric(layout.Chance,data,layout.ChanceIsProbability
            ? drop.at("ChancePercent").get<double>()/100.0
            : drop.at("ChancePercent").get<double>());
        if(layout.MaximumGrouping)SetResourceNumeric(layout.MaximumGrouping,data,drop.at("Max").get<int>());
        prepared.push_back(std::move(value));
    }

    UECustom::FScriptArrayHelper helper(layout.Items->ContainerPtrToValuePtr<FScriptArray>(layout.Owner),layout.Items);
    for(auto& value:prepared)helper.Add(*value);
    PS::Log<LogLevel::Verbose>(TEXT("Appended {} additional resource drop(s) through {} on {}.\n"),
        prepared.size(),layout.Owner->GetClassPrivate()->GetName(),actor->GetClassPrivate()->GetName());
    return true;
}
}

void DragonWildsSpawnLoader::ApplyAdditionalDrops(UObject* actor,const nlohmann::json& drops) {
    if(drops.empty() || !actor || !GetGameMode(actor->GetWorld()))return;
    if(auto found=m_bonusApplied.find(actor);found!=m_bonusApplied.end() && found->second.Get()==actor)return;
    try {
        SpawnFields::Drops(drops);
        UObject* component=nullptr;
        try {component=ActorHelper::GetObjectRef(actor,TEXT("LootDrop"));} catch(...) {}
        if(!component)try {component=ActorHelper::GetObjectRef(actor,TEXT("LootDrop_GEN_VARIABLE"));} catch(...) {}
        if(!component) {
            if(m_bonusApplied.size()>=4096)std::erase_if(m_bonusApplied,[](auto& entry){return !entry.second.Get();});
            if(m_bonusApplied.size()>=4096)throw std::runtime_error("Additional loot live actor limit reached (4096)");
            if(!ApplyAdditionalResourceDrops(actor,drops))
                throw std::runtime_error("Actor exposes neither native AI LootDrop nor a supported resource ItemsToDrop layout");
            m_bonusApplied.insert_or_assign(actor,PS::WeakObject(actor));
            return;
        }
        auto* handleProperty=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("EnemyTableRowHandle")));
        auto* type=handleProperty?handleProperty->GetStruct().Get():nullptr;
        auto* tableProperty=type?CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(type,TEXT("DataTable"))):nullptr;
        auto* nameProperty=type?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("RowName"))):nullptr;
        if(!handleProperty || !tableProperty || !nameProperty || tableProperty->GetElementSize()!=sizeof(UObject*) || nameProperty->GetElementSize()!=sizeof(FName)
            || tableProperty->GetArrayDim()!=1 || nameProperty->GetArrayDim()!=1 || tableProperty->GetOffset_Internal()<0 || nameProperty->GetOffset_Internal()<0
            || tableProperty->GetOffset_Internal()+sizeof(UObject*)>handleProperty->GetElementSize() || nameProperty->GetOffset_Internal()+sizeof(FName)>handleProperty->GetElementSize())throw std::runtime_error("Unsupported enemy loot handle layout");
        void* handle=handleProperty->ContainerPtrToValuePtr<void>(component);
        UObject* object=nullptr;std::memcpy(&object,tableProperty->ContainerPtrToValuePtr<void>(handle),sizeof(object));
        if(!object || !object->IsA<UDataTable>())throw std::runtime_error("Enemy loot table unavailable");
        auto* table=static_cast<UDataTable*>(object);
        auto original=nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(handle));
        const std::string prefix="RS_SpawnBonus_";
        const auto restored=RC::to_string(original.ToString());
        if(restored.starts_with(prefix) && restored.size()>prefix.size()+34 && restored.substr(prefix.size()+32,2)=="__"
            && restored.substr(prefix.size(),32).find_first_not_of("0123456789abcdef")==std::string::npos) {
            original=FName(RC::to_generic_string(restored.substr(prefix.size()+34)),FNAME_Add);
            nameProperty->SetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(handle),original);
        }
        const auto cacheKey=RC::to_string(table->GetPathName())+":"+RC::to_string(original.ToString())+":"+drops.dump();
        auto cached=m_bonusRowCache.find(cacheKey);
        if(cached==m_bonusRowCache.end()) {
            if(m_bonusRowCache.size()>=256)throw std::runtime_error("Additional loot definition limit reached (256)");
            auto* source=table->FindRowUnchecked(original);auto* rowType=table->GetRowStruct().Get();
            auto* levels=rowType?CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(rowType,TEXT("TablesByPowerLevel"))):nullptr;
            auto* levelStruct=levels?CastField<FStructProperty>(levels->GetInner()):nullptr;
            auto* handles=levelStruct?CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(levelStruct->GetStruct().Get(),TEXT("TableHandles"))):nullptr;
            if(!source || !rowType || !levels || !handles)throw std::runtime_error("Enemy loot power-level layout unavailable");
            auto* lootObject=ActorHelper::ResolveObject(TEXT("/Game/Gameplay/Items/LootDropTables/DT_LootDropTable.DT_LootDropTable"));
            if(!lootObject || !lootObject->IsA<UDataTable>())throw std::runtime_error("Native item loot table unavailable");
            auto* lootTable=static_cast<UDataTable*>(lootObject);auto* lootType=lootTable->GetRowStruct().Get();
            if(!lootType)throw std::runtime_error("Native item loot row layout unavailable");
            FManagedStruct bonus(lootType),combined(rowType);
            rowType->CopyScriptStruct(combined.GetData(),source);
            auto items=nlohmann::json::array();
            for(const auto& drop:drops) {
                auto* item=ActorHelper::ResolveObject(RC::to_generic_string(drop.at("Item").get<std::string>()));
                if(!item)throw std::runtime_error("Additional drop item unavailable: "+drop.at("Item").get<std::string>());
                items.push_back({{"SpawnedItemData",RC::to_string(item->GetPathName())},{"MinimumDropAmount",drop.at("Min")},{"MaximumDropAmount",drop.at("Max")},{"DropChance",drop.at("ChancePercent")},
                    {"bOneInstancePerPlayerOnlyVisibleToThem",false},{"bAutoAddToInventory",false},{"bOnlyForPlayersThatInflictedDamage",false}});
            }
            const nlohmann::json rowFields={{"Conditions",nlohmann::json::array()},{"DropChance",100},{"Resources",items},{"Recipes",nlohmann::json::array()}};
            for(const auto& [field,value]:rowFields.items()) {
                auto* property=PropertyHelper::GetPropertyByName(lootType,RC::to_generic_string(field));
                if(!property)throw std::runtime_error("Native loot row missing field: "+field);
                PropertyHelper::CopyJsonValueToContainer(bonus.GetData(),property,value);
            }
            const auto identity=StableGuid(cacheKey);std::string digest;
            for(size_t i=0;i<sizeof(identity);++i)digest+=std::format("{:02x}",reinterpret_cast<const unsigned char*>(&identity)[i]);
            const auto name=RC::to_generic_string(prefix+digest+"__"+RC::to_string(original.ToString()));const FName row(name,FNAME_Add);
            if(table->FindRowUnchecked(row) || lootTable->FindRowUnchecked(row))throw std::runtime_error("Generated additional loot row collision; original loot preserved");
            const auto extra=nlohmann::json::array({{{"DataTable",RC::to_string(lootTable->GetPathName())},{"RowName",RC::to_string(name)}}});
            UECustom::FScriptArrayHelper powers(levels->ContainerPtrToValuePtr<FScriptArray>(combined.GetData()),levels);
            size_t count=0;
            powers.ForEachElement([&](void* element){PropertyHelper::CopyJsonValueToContainer(element,handles,PropertyHelper::BuildAppendValue(handles,extra));++count;});
            if(!count)throw std::runtime_error("Enemy loot has no power-level entries; no instance change made");
            const auto remember=[&](UDataTable* target,void* data) {
                const bool root=!target->IsRootSet();if(root)target->SetRootSet();
                try {target->AddRow(row,*reinterpret_cast<FTableRowBase*>(data));m_bonusRows.push_back({PS::WeakObject(target),name,target->FindRowUnchecked(row),root});}
                catch(...){if(root)target->ClearRootSet();throw;}
            };
            const auto before=m_bonusRows.size();
            try {remember(lootTable,bonus.GetData());remember(table,combined.GetData());}
            catch(...) {
                while(m_bonusRows.size()>before){const auto saved=m_bonusRows.back();if(auto* target=saved.Table.Get()){static_cast<UDataTable*>(target)->RemoveRow(FName(saved.Name,FNAME_Find));if(saved.Rooted)target->ClearRootSet();}m_bonusRows.pop_back();}
                throw;
            }
            cached=m_bonusRowCache.emplace(cacheKey,name).first;
        }
        if(m_bonusHandles.size()>=4096)std::erase_if(m_bonusHandles,[](const auto& entry){return !entry.Component.Get();});
        if(m_bonusHandles.size()>=4096)throw std::runtime_error("Additional loot live component limit reached (4096)");
        m_bonusHandles.push_back({PS::WeakObject(component),original.ToString(),cached->second});
        nameProperty->SetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(handle),FName(cached->second,FNAME_Find));
        if(m_bonusApplied.size()>=4096)std::erase_if(m_bonusApplied,[](auto& entry){return !entry.second.Get();});
        m_bonusApplied.insert_or_assign(actor,PS::WeakObject(actor));
    }catch(const std::exception& error) {
        if(m_lootRowWarningActors.insert(actor).second)PS::Log<LogLevel::Warning>(TEXT("Additional spawn drops unavailable; original loot preserved: {}\n"),PS::ToWideSafe(error.what()));
    }
}
