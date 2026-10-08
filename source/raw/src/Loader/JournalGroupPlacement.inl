namespace {
bool SameJournalPath(const UECustom::FSoftObjectPath& path,UObject* entry) {
    const UECustom::FSoftObjectPath expected(entry->GetPathName());
    return path.GetLongPackageFName()==expected.GetLongPackageFName()
        && path.GetAssetFName()==expected.GetAssetFName()
        && RC::StringType(*path.SubPathString)==RC::StringType(*expected.SubPathString);
}
struct JournalFieldCopy {
    FProperty* property;
    void* data;
    JournalFieldCopy(FProperty* field,void* source):property(field),data(FMemory::Malloc(field->GetElementSize())) {
        if(!data)throw std::bad_alloc();
        property->InitializeValue(data);
        try {property->CopyCompleteValue(data,source);}
        catch(...) {property->DestroyValue(data);FMemory::Free(data);throw;}
    }
    JournalFieldCopy(const JournalFieldCopy&)=delete;
    ~JournalFieldCopy(){property->DestroyValue(data);FMemory::Free(data);}
    void Commit(void* destination) noexcept {
        auto* a=static_cast<uint8*>(data);auto* b=static_cast<uint8*>(destination);
        for(int32 i=0;i<property->GetElementSize();++i)std::swap(a[i],b[i]);
    }
};
void ValidateJournalEntryMap(FMapProperty* map) {
    if(!map || map->GetArrayDim()!=1 || !CastField<FNameProperty>(map->GetKeyProp())
       || map->GetKeyProp()->GetElementSize()!=sizeof(FName)
       || !CastField<FSoftObjectProperty>(map->GetValueProp())
       || map->GetValueProp()->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))
        throw std::runtime_error("Journal entry map layout was unsupported");
}
bool AddJournalMapEntry(FMapProperty* property,void* data,FName key,UObject* entry) {
    ValidateJournalEntryMap(property);
    UECustom::FScriptMapHelper map(property,data);
    unsigned matches=0;
    map.ForEachPair([&](void* k,void* v) {
        if(*static_cast<FName*>(k)!=key)return;
        if(++matches>1 || !SameJournalPath(static_cast<UECustom::FSoftObjectPtr*>(v)->ObjectID,entry))
            throw std::runtime_error("Journal entry key conflicts with an existing entry");
    });
    if(matches)return false;
    UECustom::FManagedValue pair;map.InitializePair(pair);
    *static_cast<FName*>(map.GetKeyPtr(pair.GetData()))=key;
    auto* soft=static_cast<UECustom::FSoftObjectPtr*>(map.GetValuePtr(pair.GetData()));
    soft->ObjectID=UECustom::FSoftObjectPath(entry->GetPathName());
    map.Add(pair);map.Rehash();return true;
}
bool PlaceJournalGroup(UObject* category,UObject* entry,const PS::JournalPlacement::Placement& placement) {
    if(!placement.TargetGroup)throw std::runtime_error("This journal category requires AddTo.Group.Id");
    const auto& requested=*placement.TargetGroup;
    auto* type=category->GetClassPrivate();
    auto* groups=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type,TEXT("Groups")));
    auto* group=groups?CastField<FStructProperty>(groups->GetInner()):nullptr;
    auto* aggregate=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(type,TEXT("Data")));
    auto* byGroup=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type,TEXT("DataByGroup")));
    if(!groups || groups->GetArrayDim()!=1 || !group || !group->GetStruct()
       || group->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.JournalSubCategoryEntryGroup")
       || !aggregate || !aggregate->GetStruct() || !byGroup || byGroup->GetArrayDim()!=1
       || !CastField<FNameProperty>(byGroup->GetKeyProp()) || byGroup->GetKeyProp()->GetElementSize()!=sizeof(FName))
        throw std::runtime_error("Journal group category layout was unsupported");
    auto* container=CastField<FStructProperty>(byGroup->GetValueProp());
    if(!container || container->GetStruct()!=aggregate->GetStruct())
        throw std::runtime_error("Journal group map container did not match Data");
    auto* entries=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(group->GetStruct().Get(),TEXT("Entries")));
    auto* id=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(group->GetStruct().Get(),TEXT("InternalName")));
    auto* name=CastField<FTextProperty>(PropertyHelper::GetPropertyByName(group->GetStruct().Get(),TEXT("DisplayName")));
    auto* entryMap=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(aggregate->GetStruct().Get(),TEXT("DataMap")));
    ValidateJournalEntryMap(entryMap);
    if(!entries || entries->GetArrayDim()!=1 || !CastField<FSoftObjectProperty>(entries->GetInner())
       || entries->GetInner()->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr)
       || !id || id->GetElementSize()!=sizeof(FName) || !name)
        throw std::runtime_error("Journal group fields were unsupported");
    void* liveGroups=groups->ContainerPtrToValuePtr<void>(category);
    void* liveData=aggregate->ContainerPtrToValuePtr<void>(category);
    void* liveMap=byGroup->ContainerPtrToValuePtr<void>(category);
    JournalFieldCopy stagedGroups(groups,liveGroups),stagedData(aggregate,liveData),stagedMap(byGroup,liveMap);
    UECustom::FScriptArrayHelper list(stagedGroups.data,groups);
    const FName groupId(RC::to_generic_string(requested.Id),FNAME_Add);
    if(groupId==FName())throw std::runtime_error("Journal group Id cannot be None");
    void* selected=nullptr;
    list.ForEachElement([&](void* value) {
        if(*id->ContainerPtrToValuePtr<FName>(value)==groupId) {
            if(selected)throw std::runtime_error("Duplicate journal group Id");selected=value;
        }
    });
    bool changed=false;
    const bool creating=selected==nullptr;
    if(!selected) {
        if(!requested.CreateIfMissing)throw std::runtime_error("Journal group Id was not found");
        UECustom::FManagedValue value;list.InitializeValue(value);
        try {
            *id->ContainerPtrToValuePtr<FName>(value.GetData())=groupId;
            PropertyHelper::SetTextPropertyValueFromJsonValue(name->ContainerPtrToValuePtr<void>(value.GetData()),name,requested.DisplayName);
        } catch(...) {group->DestroyValue(value.GetData());throw;}
        list.Add(value);changed=true;
        list.ForEachElement([&](void* value) {if(*id->ContainerPtrToValuePtr<FName>(value)==groupId)selected=value;});
    } else if(!requested.DisplayName.empty()
        && PropertyHelper::GetTextAsString(name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(selected)))!=RC::to_generic_string(requested.DisplayName))
        throw std::runtime_error("Existing journal group has a different DisplayName; it was not overwritten");
    UECustom::FScriptArrayHelper refs(entries->ContainerPtrToValuePtr<void>(selected),entries);
    unsigned occurrences=0;
    refs.ForEachElement([&](void* value) {if(SameJournalPath(static_cast<UECustom::FSoftObjectPtr*>(value)->ObjectID,entry))++occurrences;});
    if(occurrences>1)throw std::runtime_error("Duplicate journal entry in group");
    if(!occurrences) {
        UECustom::FManagedValue value;refs.InitializeValue(value);
        auto* soft=static_cast<UECustom::FSoftObjectPtr*>(value.GetData());
        soft->ObjectID=UECustom::FSoftObjectPath(entry->GetPathName());refs.Add(value);changed=true;
    }
    FName key(RC::to_generic_string(placement.Key),FNAME_Add);
    changed=AddJournalMapEntry(entryMap,entryMap->ContainerPtrToValuePtr<void>(stagedData.data),key,entry)||changed;
    UECustom::FScriptMapHelper groupMap(byGroup,stagedMap.data);
    void* groupContainer=nullptr;
    groupMap.ForEachPair([&](void* k,void* v) {if(*static_cast<FName*>(k)==groupId) {
        if(groupContainer)throw std::runtime_error("Duplicate journal group map key");groupContainer=v;
    }});
    if(groupContainer)changed=AddJournalMapEntry(entryMap,entryMap->ContainerPtrToValuePtr<void>(groupContainer),key,entry)||changed;
    else {
        if(!creating)throw std::runtime_error("Existing journal group lookup is not initialized; placement deferred");
        UECustom::FManagedValue pair;groupMap.InitializePair(pair);
        *static_cast<FName*>(groupMap.GetKeyPtr(pair.GetData()))=groupId;
        try {AddJournalMapEntry(entryMap,entryMap->ContainerPtrToValuePtr<void>(groupMap.GetValuePtr(pair.GetData())),key,entry);}
        catch(...) {container->DestroyValue(groupMap.GetValuePtr(pair.GetData()));throw;}
        groupMap.Add(pair);groupMap.Rehash();changed=true;
    }
    if(changed) {stagedGroups.Commit(liveGroups);stagedData.Commit(liveData);stagedMap.Commit(liveMap);}
    return changed;
}
}
