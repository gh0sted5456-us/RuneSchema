// Included inside InGameQuickMenu.cpp's anonymous namespace. This same file is
// compiled by the portable adapter tests with a deliberately limited fake host.
// No addresses from a user log and no assumed UGameViewportClient layout.
bool LiveMenuObject(UObject* object) {
    return object && !object->HasAnyFlags(static_cast<EObjectFlags>(
        RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed));
}

void RequireWritableMenuSpan(void* address, std::size_t length) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!address || !VirtualQuery(address, &memory, sizeof(memory))
        || memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        || !(memory.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
        || !QuickInput::ContainsSpan(reinterpret_cast<std::uintptr_t>(memory.BaseAddress), memory.RegionSize,
                                   reinterpret_cast<std::uintptr_t>(address), length))
        throw std::runtime_error("Viewport/input field is not in a committed writable memory region");
}

void* CheckedMenuAddress(UObject* object, const QuickInput::BoolSlot& slot) {
    if (!LiveMenuObject(object) || !object->GetClassPrivate())
        throw std::runtime_error("Viewport/input owner is no longer live");
    const auto extent = object->GetClassPrivate()->GetPropertiesSize();
    if (extent <= 0 || !QuickInput::InBounds(slot.offset, slot.size, static_cast<std::size_t>(extent)))
        throw std::runtime_error("Viewport/input field lies outside its runtime class allocation");
    const auto base = reinterpret_cast<std::uintptr_t>(object);
    if (static_cast<std::size_t>(slot.offset) > std::numeric_limits<std::uintptr_t>::max() - base)
        throw std::runtime_error("Viewport/input field address overflows");
    auto* address = reinterpret_cast<void*>(base + static_cast<std::size_t>(slot.offset));
    RequireWritableMenuSpan(address, slot.size);
    return address;
}

bool FindReflectedMenuBool(UObject* object, const TCHAR* name, QuickInput::BoolSlot& out) {
    if (!LiveMenuObject(object) || !object->GetClassPrivate()) return false;
    auto* raw = PropertyHelper::GetPropertyByName(object->GetClassPrivate(), name);
    if (!raw) return false;
    auto* property = CastField<FBoolProperty>(raw);
    if (!property || property->GetArrayDim() != 1 || property->GetElementSize() <= 0)
        throw std::runtime_error("Reflected viewport/input field is not a scalar boolean");
    out = {QuickInput::BoolBackend::Reflected, property->GetOffset_Internal(),
           static_cast<std::size_t>(property->GetElementSize())};
    CheckedMenuAddress(object, out);
    return true;
}

FBoolProperty* RevalidateReflectedMenuBool(UObject* object, const TCHAR* name,
                                         const QuickInput::BoolSlot& slot) {
    auto* property = CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(), name));
    if (!property || property->GetArrayDim() != 1 || property->GetOffset_Internal() != slot.offset
        || property->GetElementSize() <= 0 || static_cast<std::size_t>(property->GetElementSize()) != slot.size)
        throw std::runtime_error("Reflected input layout changed before restoration");
    return property;
}

bool ReadMenuBool(UObject* object, const TCHAR* name, const QuickInput::BoolSlot& slot) {
    auto* address = CheckedMenuAddress(object, slot);
    if (slot.backend != QuickInput::BoolBackend::Reflected)
        throw std::runtime_error("Only reflected controller cursor flags are supported");
    return RevalidateReflectedMenuBool(object, name, slot)->GetPropertyValue(address);
}
void WriteMenuBool(UObject* object, const TCHAR* name, const QuickInput::BoolSlot& slot, bool value) {
    (void)ReadMenuBool(object, name, slot);
    RevalidateReflectedMenuBool(object, name, slot)->SetPropertyValue(CheckedMenuAddress(object, slot), value);
    if (ReadMenuBool(object, name, slot) != value)
        throw std::runtime_error("Controller cursor write did not survive readback");
}

struct MenuInputCall { UFunction* function = nullptr; FBoolProperty* argument = nullptr; };
MenuInputCall ValidateMenuInputCall(const TCHAR* path, const TCHAR* argument) {
    auto* function = CanvasFunction(path);
    if (!function || function->GetParmsSize() <= 0 || function->GetParmsSize() > 64)
        throw std::runtime_error("Controller input-suppression function is unavailable");
    auto* property = CastField<FBoolProperty>(function->FindProperty(FName(argument, FNAME_Find)));
    std::size_t count = 0;
    for (auto* p : TFieldRange<FProperty>(function, EFieldIterationFlags::Default))
        if (p->HasAnyPropertyFlags(CPF_Parm)) ++count;
    if (count != 1 || !property || property->GetArrayDim() != 1 || property->GetElementSize() <= 0
        || !property->HasAnyPropertyFlags(CPF_Parm) || property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)
        || !QuickInput::InBounds(property->GetOffset_Internal(), static_cast<std::size_t>(property->GetElementSize()),
                                static_cast<std::size_t>(function->GetParmsSize())))
        throw std::runtime_error("Controller input-suppression function has an unexpected parameter layout");
    return {function, property};
}
void InvokeMenuInputCall(UObject* controller, const MenuInputCall& call, bool value) {
    if (!LiveMenuObject(controller)) throw std::runtime_error("Controller input owner is no longer live");
    alignas(16) std::array<std::uint8_t,64> storage{};
    call.argument->SetPropertyValue(storage.data() + call.argument->GetOffset_Internal(), value);
    controller->ProcessEvent(call.function, storage.data());
}

// Native engine input-mode calls, invoked through reflection on the game thread.
// No GameViewportClient data layout, byte offset, DLL export, or address scan.
class MissingNativeMenuInput : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};

void RequireMenuParameters(UFunction* function, std::initializer_list<FProperty*> expected) {
    if (!function || function->GetParmsSize() <= 0 || function->GetParmsSize() > 256)
        throw std::runtime_error("Native input function has an unsupported parameter size");
    std::size_t count = 0;
    for (auto* field : TFieldRange<FProperty>(function, EFieldIterationFlags::Default)) {
        if (!field->HasAnyPropertyFlags(CPF_Parm)) continue;
        ++count;
        if (std::find(expected.begin(), expected.end(), field) == expected.end())
            throw std::runtime_error("Native input function has an unexpected parameter");
    }
    if (count != expected.size()) throw std::runtime_error("Native input parameter count changed");
    for (auto* field : expected) {
        if (!field || field->GetArrayDim() != 1 || field->GetElementSize() <= 0
            || !field->HasAnyPropertyFlags(CPF_Parm)
            || !QuickInput::InBounds(field->GetOffset_Internal(), field->GetElementSize(), function->GetParmsSize()))
            throw std::runtime_error("Native input parameter is missing or outside its call buffer");
    }
    for (auto* field : expected) {
        for (auto* other : expected) {
            if (other == field || !other) continue;
            if (field->GetOffset_Internal() < other->GetOffset_Internal() + other->GetElementSize()
                && other->GetOffset_Internal() < field->GetOffset_Internal() + field->GetElementSize())
                throw std::runtime_error("Native input parameters overlap");
        }
    }
}

// Only temporary, by-value UObject* UFUNCTION arguments use this path. The
// UE4SS build in UE4SS(9).log has no FObjectPropertyBase virtual setter slot.
// Resolve the parameter offset from reflection, then copy the scalar argument,
// as ActorHelper::FunctionCall::Arg does. Never use this for UObject members,
// arrays, soft/weak/class references, or TObjectPtr/wrapper parameters.
FObjectProperty* RequireMenuObjectParameter(UFunction* function, FProperty* field,
                                          std::size_t capacity) {
    auto* object = CastField<FObjectProperty>(field);
    if (!function || function->GetParmsSize() <= 0 || function->GetParmsSize() > 256
        || capacity < static_cast<std::size_t>(function->GetParmsSize()) || !object
        || object->GetClass().GetName() != STR("ObjectProperty")
        || object->GetArrayDim() != 1 || object->GetElementSize() != sizeof(UObject*)
        || !object->HasAnyPropertyFlags(CPF_Parm)
        || object->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm | CPF_UObjectWrapper)
        || !object->GetPropertyClass().Get()
        || !QuickInput::InBounds(object->GetOffset_Internal(), sizeof(UObject*), function->GetParmsSize()))
        throw std::runtime_error("Native input object argument is not a bounded by-value UObject* parameter");
    bool belongs = false;
    for (auto* parameter : TFieldRange<FProperty>(function, EFieldIterationFlags::Default))
        if (parameter == field) { belongs = true; break; }
    if (!belongs) throw std::runtime_error("Native input object argument belongs to another function");
    return object;
}
UObject* ReadMenuObjectParameter(UFunction* function, FProperty* field,
                                const void* parameters, std::size_t capacity) {
    auto* object = RequireMenuObjectParameter(function, field, capacity);
    if (!parameters) throw std::runtime_error("Native input argument buffer is null");
    UObject* value = nullptr;
    std::memcpy(&value, static_cast<const std::uint8_t*>(parameters) + object->GetOffset_Internal(), sizeof(value));
    return value;
}
void WriteMenuObjectParameter(UFunction* function, FProperty* field,
                              void* parameters, std::size_t capacity, UObject* value) {
    auto* object = RequireMenuObjectParameter(function, field, capacity);
    if (!parameters || (value && (!LiveMenuObject(value) || !value->IsA(object->GetPropertyClass().Get()))))
        throw std::runtime_error("Native input object argument has a null buffer or incompatible live object");
    std::memcpy(static_cast<std::uint8_t*>(parameters) + object->GetOffset_Internal(), &value, sizeof(value));
    if (ReadMenuObjectParameter(function, field, parameters, capacity) != value)
        throw std::runtime_error("Native input object argument failed readback");
}

// An initialized reflected packet, not a guessed C++ mirror of UFUNCTION args.
class MenuParameterBuffer {
public:
    explicit MenuParameterBuffer(std::initializer_list<FProperty*> fields) : m_fields(fields) {
        try {
            for (auto* field : m_fields) {
                field->InitializeValue_InContainer(m_storage.data());
                ++m_initialized;
            }
        } catch (...) { Clear(); throw; }
    }
    ~MenuParameterBuffer() { Clear(); }
    MenuParameterBuffer(const MenuParameterBuffer&) = delete;
    MenuParameterBuffer& operator=(const MenuParameterBuffer&) = delete;
    void* Data() { return m_storage.data(); }
    void* At(FProperty* field) { return m_storage.data() + field->GetOffset_Internal(); }
private:
    void Clear() noexcept {
        while (m_initialized) {
            try { m_fields[--m_initialized]->DestroyValue_InContainer(m_storage.data()); } catch (...) {}
        }
    }
    alignas(16) std::array<std::uint8_t, 256> m_storage{};
    std::vector<FProperty*> m_fields;
    std::size_t m_initialized = 0;
};

struct NativeMenuModeCall {
    UObject* library = nullptr;
    UFunction* function = nullptr;
    FObjectPropertyBase* controller = nullptr;
    FObjectPropertyBase* focus = nullptr;
    FProperty* mouseLock = nullptr;
    FNumericProperty* mouseLockNumber = nullptr;
    FBoolProperty* flush = nullptr;
    std::int64_t doNotLock = 0;
    bool uiOnly = false;
};

NativeMenuModeCall PrepareNativeMenuMode(bool uiOnly) {
    NativeMenuModeCall call;
    call.uiOnly = uiOnly;
    call.library = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr,
        TEXT("/Script/UMG.Default__WidgetBlueprintLibrary"), false);
    call.function = CanvasFunction(uiOnly
        ? TEXT("/Script/UMG.WidgetBlueprintLibrary:SetInputMode_UIOnlyEx")
        : TEXT("/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameOnly"));
    auto* libraryClass = ActorHelper::ResolveClass(TEXT("/Script/UMG.WidgetBlueprintLibrary"));
    if (!call.library || !call.function || !libraryClass)
        throw MissingNativeMenuInput(uiOnly ? "Native SetInputMode_UIOnlyEx is unavailable"
                                           : "Native SetInputMode_GameOnly is unavailable");
    if (!call.library->IsA(libraryClass) || !call.library->HasAnyFlags(RF_ClassDefaultObject)
        || call.library->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed))
        || !(call.function->GetFunctionFlags() & FUNC_Native)
        || !(call.function->GetFunctionFlags() & FUNC_Static))
        throw std::runtime_error("Native input-mode library/function contract changed");
    call.controller = CastField<FObjectPropertyBase>(call.function->FindProperty(FName(TEXT("PlayerController"), FNAME_Find)));
    call.flush = CastField<FBoolProperty>(call.function->FindProperty(FName(TEXT("bFlushInput"), FNAME_Find)));
    if (uiOnly) {
        call.focus = CastField<FObjectPropertyBase>(call.function->FindProperty(FName(TEXT("InWidgetToFocus"), FNAME_Find)));
        call.mouseLock = call.function->FindProperty(FName(TEXT("InMouseLockMode"), FNAME_Find));
        RequireMenuParameters(call.function, {call.controller, call.focus, call.mouseLock, call.flush});
    } else RequireMenuParameters(call.function, {call.controller, call.flush});
    for (auto* field : TFieldRange<FProperty>(call.function, EFieldIterationFlags::Default))
        if (field->HasAnyPropertyFlags(CPF_Parm) && field->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
            throw std::runtime_error("Native input-mode function unexpectedly has output parameters");
    RequireMenuObjectParameter(call.function, call.controller, call.function->GetParmsSize());
    if (uiOnly) RequireMenuObjectParameter(call.function, call.focus, call.function->GetParmsSize());
    auto* controllerClass = ActorHelper::ResolveClass(TEXT("/Script/Engine.PlayerController"));
    if (!controllerClass || call.controller->GetPropertyClass().Get() != controllerClass
        || call.controller->GetElementSize() != sizeof(UObject*) || call.flush->GetElementSize() != 1)
        throw std::runtime_error("Native input-mode controller/flush types changed");
    if (uiOnly) {
        auto* widgetClass = ActorHelper::ResolveClass(TEXT("/Script/UMG.Widget"));
        if (!widgetClass || call.focus->GetPropertyClass().Get() != widgetClass
            || call.focus->GetElementSize() != sizeof(UObject*))
            throw std::runtime_error("Native input-mode focus type changed");
        UEnum* enumeration = nullptr;
        if (auto* field = CastField<FEnumProperty>(call.mouseLock)) {
            enumeration = field->GetEnum(); call.mouseLockNumber = field->GetUnderlyingProperty();
        } else if (auto* field = CastField<FNumericProperty>(call.mouseLock)) {
            enumeration = field->GetIntPropertyEnum(); call.mouseLockNumber = field;
        }
        if (!enumeration || !call.mouseLockNumber || !call.mouseLockNumber->IsInteger()
            || call.mouseLock->GetElementSize() != 1 || call.mouseLockNumber->GetElementSize() != 1)
            throw std::runtime_error("Native mouse-lock enum contract changed");
        bool found = false;
        for (const auto& entry : enumeration->GetEnumNames()) {
            const auto name = entry.Key.ToString();
            if (name == TEXT("EMouseLockMode::DoNotLock") || name == TEXT("DoNotLock")) {
                if (found || entry.Value < 0 || entry.Value > 255)
                    throw std::runtime_error("Native DoNotLock enum is ambiguous or invalid");
                call.doNotLock = entry.Value; found = true;
            }
        }
        if (!found) throw std::runtime_error("Native mouse-lock enum has no DoNotLock value");
    }
    return call;
}

thread_local unsigned s_nativeMenuModeDepth = 0;
struct OwnMenuModeScope {
    OwnMenuModeScope() { ++s_nativeMenuModeDepth; }
    ~OwnMenuModeScope() { --s_nativeMenuModeDepth; }
};
void PopulateNativeMenuMode(UObject* controller, const NativeMenuModeCall& call, MenuParameterBuffer& buffer) {
    if (!LiveMenuObject(controller) || !controller->IsA(call.controller->GetPropertyClass().Get()))
        throw std::runtime_error("Native input-mode controller is no longer live");
    WriteMenuObjectParameter(call.function, call.controller, buffer.Data(), 256, controller);
    call.flush->SetPropertyValue(buffer.At(call.flush), true);
    if (call.uiOnly) {
        // Canvas input still uses the existing same-process message relay.
        WriteMenuObjectParameter(call.function, call.focus, buffer.Data(), 256, nullptr);
        call.mouseLockNumber->SetIntPropertyValue(buffer.At(call.mouseLock), static_cast<std::uint64_t>(call.doNotLock));
        if (call.mouseLockNumber->GetSignedIntPropertyValue(buffer.At(call.mouseLock)) != call.doNotLock)
            throw std::runtime_error("Native mouse-lock argument failed readback");
    }
    if (ReadMenuObjectParameter(call.function, call.controller, buffer.Data(), 256) != controller
        || !call.flush->GetPropertyValue(buffer.At(call.flush)))
        throw std::runtime_error("Native input-mode arguments failed readback");
}
void PreflightNativeMenuMode(UObject* controller, const NativeMenuModeCall& call) {
    MenuParameterBuffer buffer(call.uiOnly
        ? std::initializer_list<FProperty*>{call.controller, call.focus, call.mouseLock, call.flush}
        : std::initializer_list<FProperty*>{call.controller, call.flush});
    PopulateNativeMenuMode(controller, call, buffer); // No ProcessEvent, cursor change or input lock.
}
void InvokeNativeMenuMode(UObject* controller, const NativeMenuModeCall& call, bool* dispatchOwned = nullptr) {
    MenuParameterBuffer buffer(call.uiOnly
        ? std::initializer_list<FProperty*>{call.controller, call.focus, call.mouseLock, call.flush}
        : std::initializer_list<FProperty*>{call.controller, call.flush});
    PopulateNativeMenuMode(controller, call, buffer);
    OwnMenuModeScope scope;
    // Argument preparation cannot leave an imaginary input-mode lease behind.
    // A dispatch that throws may have changed engine state, so retain that lease.
    if (dispatchOwned) *dispatchOwned = true;
    call.library->ProcessEvent(call.function, buffer.Data());
    // These APIs return void; returned dispatch is not a live input-focus test.
}

bool QueryControllerInputLock(UObject* controller, const TCHAR* path) {
    auto* function = CanvasFunction(path);
    if (!function) throw MissingNativeMenuInput("Native controller input-state query is unavailable");
    auto* result = CastField<FBoolProperty>(function->GetReturnProperty());
    RequireMenuParameters(function, {result});
    if (!result->HasAnyPropertyFlags(CPF_ReturnParm) || result->GetElementSize() != 1)
        throw std::runtime_error("Native controller input-state query contract changed");
    MenuParameterBuffer buffer({result});
    controller->ProcessEvent(function, buffer.Data());
    return result->GetPropertyValue(buffer.At(result));
}
