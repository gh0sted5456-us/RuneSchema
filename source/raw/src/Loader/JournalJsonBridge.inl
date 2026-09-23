namespace {
struct JournalJsonBridge {
    struct Shared {void* Object=nullptr;void* Controller=nullptr;};
    struct Array {Shared* Data=nullptr;int32_t Num=0,Max=0;};
    struct StringView {const wchar_t* Data=nullptr;int32_t Length=0,Padding=0;};
    static_assert(sizeof(Shared)==16 && sizeof(Array)==16 && sizeof(StringView)==16);
    using GetArray=bool(*)(void*,const StringView*,const Array**);
    using SetArray=void(*)(void*,const FString*,const Array*);
    using MakeString=void*(*)(const FString*);
    using AsString=FString*(*)(void*,FString*);
    using Copy=void(*)(Shared*,const Shared*,int32_t);
    using Release=void(*)(Shared*,int32_t);
    using KeyHash=uint32_t(*)(int32_t,const wchar_t*);
    using FindField=void*(*)(void*,uint32_t,const StringView*);
    GetArray getArray{};SetArray setArray{};MakeString makeString{};AsString asString{};
    Copy copy{};Release release{};
    KeyHash keyHash{};FindField findField{};
    uintptr_t Reader=0,Writer=0;
    JournalJsonBridge() {
        const auto* image=reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if(!image || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000)
            throw std::runtime_error("Journal persistence image header invalid");
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64
            || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)
            || !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96)
            throw std::runtime_error("Journal persistence executable layout unsupported");
        const auto size=nt->OptionalHeader.SizeOfImage;
        const auto* first=IMAGE_FIRST_SECTION(nt);
        if(reinterpret_cast<const uint8_t*>(first)-image+sizeof(*first)*nt->FileHeader.NumberOfSections>size)
            throw std::runtime_error("Journal persistence section table invalid");
        std::vector<PS::AppearanceResolver::Section> sections;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            if(!(first[i].Characteristics&IMAGE_SCN_MEM_READ) || (first[i].Characteristics&IMAGE_SCN_MEM_DISCARDABLE))continue;
            sections.push_back({first[i].VirtualAddress,first[i].Misc.VirtualSize,(first[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0});
        }
        std::array<uintptr_t,8> address{};
        const auto base=reinterpret_cast<uintptr_t>(image);
        for(size_t i=0;i<address.size();++i) {
            const auto& contract=JournalPersistenceContract::Definitions[i];
            const auto rva=i==7?PS::AppearanceResolver::ResolveCalled({image,size},sections,JournalPersistenceContract::Definitions[1],0x1ed,contract)
                :PS::AppearanceResolver::Resolve({image,size},sections,contract);
            address[i]=base+rva;
        }
        Reader=address[0];Writer=address[1];
        getArray=reinterpret_cast<GetArray>(address[2]);setArray=reinterpret_cast<SetArray>(address[3]);
        makeString=reinterpret_cast<MakeString>(address[4]);asString=reinterpret_cast<AsString>(address[5]);
        copy=reinterpret_cast<Copy>(address[6]);release=reinterpret_cast<Release>(address[7]);
        keyHash=reinterpret_cast<KeyHash>(base+PS::AppearanceResolver::ResolveCalled({image,size},sections,
            JournalPersistenceContract::Definitions[2],0x3b,JournalJsonFieldContract::Definitions[0]));
        findField=reinterpret_cast<FindField>(base+PS::AppearanceResolver::ResolveCalled({image,size},sections,
            JournalPersistenceContract::Definitions[2],0x4a,JournalJsonFieldContract::Definitions[1]));
    }
    // Hold a native shared reference across a reader/writer call that consumes
    // its incoming reference. Copy/release use the game's own verified helpers.
    class Hold {
        Shared value{};const JournalJsonBridge& api;
    public:
        Hold(const JournalJsonBridge& bridge,const Shared& source):api(bridge) {
            if(!source.Object || !source.Controller)throw std::runtime_error("Journal JSON reference missing");
            api.copy(&value,&source,1);
        }
        ~Hold(){api.release(&value,1);}
        Hold(const Hold&)=delete;
        Hold& operator=(const Hold&)=delete;
        void* Object()const{return value.Object;}
    };
    std::optional<std::vector<std::string>> ReadArray(void* object,const wchar_t* name) const {
        if(!object || !name)throw std::runtime_error("Journal JSON object/key missing");
        const FString key(name);
        const StringView view{*key,key.GetCharArray().Num()-1,0};
        const Array* array=nullptr;
        if(!getArray(object,&view,&array)) {
            if(findField(object,keyHash(view.Length,view.Data),&view))
                throw std::runtime_error("Journal JSON field exists but is not an array");
            return std::nullopt;
        }
        if(!array || array->Num<0 || array->Num>65535 || array->Max<array->Num || (array->Num && !array->Data))
            throw std::runtime_error("Journal native JSON array invalid");
        std::vector<std::string> result;result.reserve(array->Num);
        size_t total=0;
        for(int32_t i=0;i<array->Num;++i) {
            const auto& value=array->Data[i];
            if(!value.Object || !value.Controller)throw std::runtime_error("Journal JSON value missing");
            // The verified FJsonValueString constructor stores EJson::String
            // (2) at value+8. Reject other kinds before invoking AsString.
            int32_t type=0;std::memcpy(&type,static_cast<const uint8_t*>(value.Object)+8,4);
            if(type!=2)throw std::runtime_error("Journal JSON array contains a non-string");
            FString text;
            if(asString(value.Object,&text)!=&text)throw std::runtime_error("Journal native string result invalid");
            const auto& chars=text.GetCharArray();
            if(chars.Num()<0 || chars.Num()>1024*1024 || (chars.Num() && (!chars.GetData() || chars.GetData()[chars.Num()-1]!=0)))
                throw std::runtime_error("Journal native JSON string invalid");
            auto converted=chars.Num()?RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1)):std::string{};
            total+=converted.size();
            if(total>16*1024*1024)throw std::runtime_error("Journal JSON array text budget exceeded");
            result.push_back(std::move(converted));
        }
        return result;
    }
    void WriteArray(void* object,const wchar_t* name,const std::vector<std::string>& strings) const {
        if(!object || !name || strings.size()>65535)throw std::runtime_error("Journal JSON write bounds invalid");
        std::vector<Shared> values;values.reserve(strings.size());
        struct Cleanup {const JournalJsonBridge& Api;std::vector<Shared>& Values;
            ~Cleanup(){if(!Values.empty())Api.release(Values.data(),static_cast<int32_t>(Values.size()));}} cleanup{*this,values};
        size_t total=0;
        for(const auto& text:strings) {
            if(text.size()>1024*1024 || text.find('\0')!=text.npos)throw std::runtime_error("Journal JSON string too large or contains NUL");
            total+=text.size();
            if(total>16*1024*1024)throw std::runtime_error("Journal JSON array text budget exceeded");
            const FString value(RC::to_generic_string(text).c_str());
            auto* control=makeString(&value);
            if(!control)throw std::bad_alloc();
            values.push_back({static_cast<uint8_t*>(control)+16,control});
        }
        const Array array{values.data(),static_cast<int32_t>(values.size()),static_cast<int32_t>(values.size())};
        const FString key(name);
        setArray(object,&key,&array);
        const auto read=ReadArray(object,name);
        if(!read || *read!=strings)throw std::runtime_error("Journal native JSON write verification failed");
    }
};
}
