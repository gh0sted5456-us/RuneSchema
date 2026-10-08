#pragma once
// Test double only. This does NOT establish compatibility with the real UE SDK.
#include <algorithm>
#include <cassert>
#include <codecvt>
#include <cstring>
#include <functional>
#include <locale>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#define TEXT(x) L##x
namespace RC {
using StringType = std::wstring;
inline StringType to_generic_string(const std::string& s) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(s);
}
inline std::string to_string(const wchar_t* s) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(s);
}
namespace Unreal {
inline int liveStrings = 0;
inline int liveTexts = 0;
inline int conversionCalls = 0;
inline bool corruptText = false;
struct FString {
    std::wstring value;
    FString() { ++liveStrings; }
    explicit FString(const wchar_t* s) : value(s) { ++liveStrings; }
    FString(const FString& s) : value(s.value) { ++liveStrings; }
    FString& operator=(const FString&) = default;
    ~FString() { --liveStrings; }
    const wchar_t* operator*() const { return value.c_str(); }
};
struct TextData { std::wstring value; int references = 1; };
inline TextData*& TextPointer(void* p) { return *static_cast<TextData**>(p); }
inline void DropText(void* p) {
    auto*& value = TextPointer(p);
    if (value && --value->references == 0) { delete value; --liveTexts; }
    value = nullptr;
}
inline void NewText(void* p, const std::wstring& value) {
    DropText(p); TextPointer(p) = new TextData{value, 1}; ++liveTexts;
}
struct FProperty {
    std::wstring name;
    int offset, size, dimension = 1;
    bool failInitialize = false;
    FProperty(std::wstring n, int o, int s) : name(std::move(n)), offset(o), size(s) {}
    virtual ~FProperty() = default;
    int GetArrayDim() const { return dimension; }
    int GetOffset_Internal() const { return offset; }
    int GetElementSize() const { return size; }
    template<class T> T* ContainerPtrToValuePtr(void* p) const {
        return reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset);
    }
    virtual void InitializeValue(void*) = 0;
    virtual void DestroyValue(void*) = 0;
    virtual void CopyCompleteValue(void*, const void*) = 0;
    void InitializeValue_InContainer(void* p) {
        if (failInitialize) throw std::runtime_error("Injected initialization failure");
        InitializeValue(ContainerPtrToValuePtr<void>(p));
    }
    void DestroyValue_InContainer(void* p) { DestroyValue(ContainerPtrToValuePtr<void>(p)); }
};
struct FStrProperty : FProperty {
    FStrProperty(std::wstring n, int o) : FProperty(std::move(n), o, sizeof(FString)) {}
    void InitializeValue(void* p) override { new(p) FString(); }
    void DestroyValue(void* p) override { static_cast<FString*>(p)->~FString(); }
    void CopyCompleteValue(void* d, const void* s) override {
        *static_cast<FString*>(d) = *static_cast<const FString*>(s);
    }
    void SetPropertyValue(void* p, const FString& value) { *static_cast<FString*>(p) = value; }
};
class FTextProperty : public FProperty {
public:
    FTextProperty(std::wstring n, int o, int s) : FProperty(std::move(n), o, s) {}
    void InitializeValue(void* p) override { std::memset(p, 0, static_cast<size_t>(size)); }
    void DestroyValue(void* p) override { DropText(p); }
    void CopyCompleteValue(void* d, const void* s) override {
        if (d == s) return;
        auto* source = *static_cast<TextData* const*>(s);
        if (source) ++source->references;
        DropText(d);
        TextPointer(d) = source;
    }
};
template<class T> T* CastField(FProperty* value) { return dynamic_cast<T*>(value); }
inline constexpr int FNAME_Find = 0;
struct FName { std::wstring name; FName(const wchar_t* n, int) : name(n) {} };
struct UFunction {
    std::vector<FProperty*> fields;
    FProperty* input;
    FProperty* output;
    int paramsSize;
    bool toText;
    FProperty* FindProperty(const FName& name) {
        for (auto* f : fields) if (f->name == name.name) return f;
        return nullptr;
    }
    FProperty* GetReturnProperty() { return output; }
    int GetParmsSize() const { return paramsSize; }
};
struct UObject {
    void ProcessEvent(UFunction* fn, void* params) {
        ++conversionCalls;
        void* in = fn->input->ContainerPtrToValuePtr<void>(params);
        void* out = fn->output->ContainerPtrToValuePtr<void>(params);
        if (fn->toText) {
            const auto value = static_cast<FString*>(in)->value;
            NewText(out, corruptText ? std::wstring{} : value);
        } else {
            auto* text = TextPointer(in);
            static_cast<FString*>(out)->value = text ? text->value : std::wstring{};
        }
    }
};
enum class EFieldIterationFlags { Default };
template<class T> struct TFieldRange {
    UFunction* function;
    TFieldRange(UFunction* f, EFieldIterationFlags) : function(f) {}
    auto begin() const { return function->fields.begin(); }
    auto end() const { return function->fields.end(); }
};
inline UObject* library = nullptr;
inline UFunction* stringToText = nullptr;
inline UFunction* textToString = nullptr;
}
}
namespace UECustom::UObjectGlobals {
template<class T> T StaticFindObject(void*, void*, const wchar_t* path, bool = false) {
    const std::wstring name(path);
    if (name.ends_with(L"Default__KismetTextLibrary"))
        return reinterpret_cast<T>(RC::Unreal::library);
    if (name.ends_with(L":Conv_StringToText"))
        return reinterpret_cast<T>(RC::Unreal::stringToText);
    if (name.ends_with(L":Conv_TextToString"))
        return reinterpret_cast<T>(RC::Unreal::textToString);
    return nullptr;
}
}
