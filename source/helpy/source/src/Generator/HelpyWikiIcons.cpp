#include "Runtime/HelpyWikiIcons.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyIconPack.h"
#include <winhttp.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <imgui_internal.h>
#include <future>
#include <atomic>
#include <set>
namespace PS::HelpyWikiIcons {
namespace {
using Json=nlohmann::json;using Bytes=std::vector<char>;namespace fs=std::filesystem;
struct Entry {std::string Name,Alias,Key,Page,Image,Journal;ImTextureData* Texture=nullptr;int Wanted=-10000;};
struct Result {std::vector<Entry> Entries;std::string Status;};
std::vector<Entry> Entries;std::vector<ImTextureData*> Retired;std::future<Result> Worker;
std::atomic<bool> Cancelled=false;std::atomic<unsigned> Done=0,Total=0;
bool Initialized=false;std::string Message;
fs::path Folder(){return HostServices::CacheDirectory()/"wiki-icons";}
std::string Fold(std::string s){for(auto& c:s){if(c=='_')c=' ';else c=char(std::tolower(static_cast<unsigned char>(c)));}return s;}
std::string Key(const std::string& title){return HelpySourceCache::Hex(HelpySourceCache::Hash(Fold(title)));}
std::string Portrait(const std::string& key){return "/HelpyWiki/"+key+"."+key;}
std::string Encode(const std::string& value){constexpr char h[]="0123456789ABCDEF";std::string out;for(unsigned char c:value){if(std::isalnum(c)||c=='-'||c=='_')out+=c;else{out+='%';out+=h[c>>4];out+=h[c&15];}}return out;}
struct Handle{HINTERNET Value{};~Handle(){if(Value)WinHttpCloseHandle(Value);}operator HINTERNET()const{return Value;}};
Bytes Fetch(const std::string& path,size_t limit){
    if(Cancelled)throw std::runtime_error("Wiki cache cancelled; previous cache retained.");
    if(path.size()>8192||(!path.starts_with("/api.php?")&&!path.starts_with("/images/"))||path.find_first_of("\r\n\\")!=path.npos)throw std::runtime_error("Wiki request path rejected.");
    Handle session{WinHttpOpen(L"RuneSchema-Helpy-WikiCache/1 (https://github.com/gh0sted5456-us/RuneSchema)",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session||!WinHttpSetTimeouts(session,3000,3000,5000,5000))throw std::runtime_error("Wiki network session unavailable.");
    Handle connection{WinHttpConnect(session,L"dragonwilds.runescape.wiki",INTERNET_DEFAULT_HTTPS_PORT,0)};const std::wstring wide(path.begin(),path.end());
    Handle request{connection?WinHttpOpenRequest(connection,L"GET",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!request||!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("Wiki request failed; cached artwork is retained.");
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("Wiki returned HTTP "+std::to_string(status)+"; cached artwork is retained.");
    Bytes out;char buffer[16384];const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    for(;;){if(Cancelled||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Wiki request cancelled or timed out.");DWORD count=0;if(!WinHttpReadData(request,buffer,sizeof(buffer),&count))throw std::runtime_error("Wiki transfer interrupted.");if(!count)break;if(out.size()+count>limit)throw std::runtime_error("Wiki response exceeds its limit.");out.insert(out.end(),buffer,buffer+count);}return out;
}
Bytes Decode(const fs::path& file){
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);struct Com{bool Active;~Com(){if(Active)CoUninitialize();}} com{SUCCEEDED(initialized)};
    using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> convert;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame)))throw std::runtime_error("Wiki image decoder rejected its payload.");
    UINT w=0,h=0;if(FAILED(frame->GetSize(&w,&h))||!w||!h||w>2048||h>2048)throw std::runtime_error("Wiki image dimensions exceed bounds.");
    const auto factor=std::min(1.0,128.0/std::max(w,h));w=std::max(1u,UINT(w*factor));h=std::max(1u,UINT(h*factor));
    if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),w,h,WICBitmapInterpolationModeFant))||FAILED(factory->CreateFormatConverter(&convert))||FAILED(convert->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))throw std::runtime_error("Wiki image conversion failed.");
    const uint32_t bytes=w*h*4;Bytes out(16+bytes);std::memcpy(out.data(),"RSI3",4);std::memcpy(out.data()+4,&w,4);std::memcpy(out.data()+8,&h,4);std::memcpy(out.data()+12,&bytes,4);
    if(FAILED(convert->CopyPixels(nullptr,w*4,bytes,reinterpret_cast<BYTE*>(out.data()+16))))throw std::runtime_error("Wiki pixels could not be read.");return out;
}
Result Read(){Result result;const auto file=Folder()/"manifest.json";try{
    const auto bytes=HelpyIconPack::Read(file,16*1024*1024);const auto json=HelpyIconPack::Json(std::string(bytes.begin(),bytes.end()));
    if(json.value("Version",0)!=1||json.value("Source",std::string{})!="RuneScape: Dragonwilds Wiki"||!json.contains("Entries")||!json["Entries"].is_array()||json["Entries"].size()>1024)return result;
    for(const auto& row:json["Entries"]){const auto name=row.value("Name",std::string{}),key=row.value("Key",std::string{});if(name.empty()||name.size()>256||key!=Key(name))continue;
        const auto page=row.value("Page",std::string{});auto image=row.value("Image",std::string{}),journal=row.value("Journal",std::string{});const auto alias=row.value("Alias",name);
        if(!page.starts_with("https://dragonwilds.runescape.wiki/w/")||(!image.empty()&&!image.starts_with("https://dragonwilds.runescape.wiki/images/"))||journal.size()>32768||alias.size()>256)continue;
        if(row.value("JournalTruncated",false))journal+="\n\n[Cached entry truncated; open the original wiki page for the full text.]";
        std::error_code ec;if(!fs::is_regular_file(Folder()/(key+".rsicon"),ec))image.clear();result.Entries.push_back({name,alias,key,page,image,journal});}
    result.Status=std::to_string(result.Entries.size())+" wiki entries cached locally.";
}catch(...){}return result;}
Result Download(std::vector<std::string> names,const std::string& build){
    const auto folder=Folder();HelpyIconPack::Safe(folder);fs::create_directories(folder);auto document=Json{{"Version",1},{"Source","RuneScape: Dragonwilds Wiki"},{"Build",build},
        {"TextLicence","CC BY-NC-SA 3.0"},{"TextLicenceURL","https://creativecommons.org/licenses/by-nc-sa/3.0/"},
        {"ArtworkNotice","Game artwork copyright Jagex Ltd. Wiki inventory/journal media is used with permission; not relicensed under RuneSchema MIT or the map-data licence."},
        {"ArtworkSource","https://dragonwilds.runescape.wiki/w/Template:License"},{"Entries",Json::array()}};
    Total=unsigned(names.size());Done=0;unsigned warnings=0;
    for(const auto& alias:names){if(Cancelled)throw std::runtime_error("Wiki cache cancelled; existing manifest retained.");
        try{const auto bytes=Fetch("/api.php?action=query&format=json&redirects=1&prop=pageimages%7Cextracts%7Cinfo&piprop=thumbnail%7Coriginal&pithumbsize=256&inprop=url&explaintext=1&exsectionformat=plain&exlimit=1&titles="+Encode(alias),1024*1024);
            const auto reply=HelpyIconPack::Json(std::string(bytes.begin(),bytes.end()));if(!reply.contains("query")||!reply["query"].contains("pages"))throw std::runtime_error("Wiki entry not found.");
            for(const auto& [id,page]:reply["query"]["pages"].items()){
                if(page.contains("missing"))continue;const auto name=page.value("title",std::string{}),url=page.value("fullurl",std::string{});if(name.empty()||name.size()>256||!url.starts_with("https://dragonwilds.runescape.wiki/w/"))continue;
                const auto key=Key(name);auto journal=page.value("extract",std::string{});const bool truncated=journal.size()>32768;if(truncated)journal.resize(32768);
                std::string image;if(page.contains("thumbnail")||page.contains("original")){const auto& original=page.contains("thumbnail")?page["thumbnail"]:page["original"];image=original.value("source",std::string{});if(!image.starts_with("https://dragonwilds.runescape.wiki/images/")||original.value("width",0)>2048||original.value("height",0)>2048)image.clear();}
                std::string sourceHash;
                if(!image.empty())try{const auto pixelsFile=folder/(key+".rsicon");
                    // The explicit refresh rechecks source artwork; startup only
                    // reads committed local pixels and never contacts the wiki.
                    const auto png=Fetch(image.substr(std::string("https://dragonwilds.runescape.wiki").size()),1024*1024);sourceHash=HelpyIconPack::Hash(png);const auto original=folder/(key+".image");HelpyIconPack::Publish(original,png);const auto pixels=Decode(original);HelpyIconPack::Publish(pixelsFile,pixels);
                }catch(...){++warnings;}
                document["Entries"].push_back({{"Name",name},{"Alias",alias},{"Key",key},{"Page",url},{"Image",image},{"SourceSHA256",sourceHash},{"Journal",journal},{"JournalTruncated",truncated}});
            }
        }catch(const std::exception&){++warnings;}++Done;
    }
    if(Cancelled||document["Entries"].empty())throw std::runtime_error("No wiki entries committed; previous cache retained.");
    const auto text=document.dump(2);if(text.size()>16*1024*1024)throw std::runtime_error("Wiki manifest exceeds its bound.");HelpyIconPack::Publish(folder/"manifest.json",{text.begin(),text.end()});auto result=Read();result.Status+=" "+std::to_string(warnings)+" warning(s); unavailable pictures use original glyphs.";return result;
}
Entry* Find(const std::string& name){const auto folded=Fold(name);for(auto& entry:Entries)if(Fold(entry.Name)==folded||Fold(entry.Alias)==folded||Portrait(entry.Key)==name)return &entry;return nullptr;}
}
void Initialize(){if(Initialized)return;Initialized=true;Worker=std::async(std::launch::async,Read);}
bool Busy(){return Worker.valid();}
const std::string& Status(){if(Busy()&&Total)Message="Caching wiki artwork / journals: "+std::to_string(Done.load())+" / "+std::to_string(Total.load());return Message;}
bool Start(const std::vector<std::string>& input,const std::string& build){
#if defined(HELPY_PREVIEW)
    return false;
#else
    if(Busy())return false;std::set<std::string> seen;std::vector<std::string> names;for(const auto& name:input)if(!name.empty()&&name.size()<=160&&seen.insert(Fold(name)).second){names.push_back(name);if(names.size()==1024)break;}
    if(names.empty())return false;Cancelled=false;Done=Total=0;Message="Downloading wiki artwork and journals (explicit opt-in)...";Worker=std::async(std::launch::async,[names,build]{return Download(names,build);});return true;
#endif
}
void Poll(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready)try{auto result=Worker.get();for(auto& old:Entries)if(old.Texture){old.Texture->SetStatus(ImTextureStatus_WantDestroy);Retired.push_back(old.Texture);}Entries=std::move(result.Entries);Message=result.Status;}catch(const std::exception& e){Message=e.what();}
    for(auto it=Retired.begin();it!=Retired.end();)if((*it)->Status==ImTextureStatus_Destroyed){ImGui::UnregisterUserTexture(*it);IM_DELETE(*it);it=Retired.erase(it);}else ++it;
    unsigned uploaded=0;const int frame=ImGui::GetFrameCount();for(auto& entry:Entries)if(!entry.Texture&&!entry.Image.empty()&&entry.Wanted>=frame-2&&uploaded<1)try{
        if(std::count_if(Entries.begin(),Entries.end(),[](const auto& e){return e.Texture!=nullptr;})>=128){auto victim=Entries.end();for(auto it=Entries.begin();it!=Entries.end();++it)if(it->Texture&&it->Texture->Status==ImTextureStatus_OK&&it->Wanted<frame-8&&(victim==Entries.end()||it->Wanted<victim->Wanted))victim=it;if(victim==Entries.end())break;victim->Texture->SetStatus(ImTextureStatus_WantDestroy);Retired.push_back(victim->Texture);victim->Texture=nullptr;}
        const auto bytes=HelpyIconPack::Read(Folder()/(entry.Key+".rsicon"),16+128*128*4);HelpyIconPack::Pixels(bytes);uint32_t w,h;std::memcpy(&w,bytes.data()+4,4);std::memcpy(&h,bytes.data()+8,4);
        auto* texture=IM_NEW(ImTextureData)();texture->Create(ImTextureFormat_RGBA32,w,h);std::memcpy(texture->GetPixels(),bytes.data()+16,bytes.size()-16);texture->UseColors=true;texture->RefCount=1;texture->SetStatus(ImTextureStatus_WantCreate);ImGui::RegisterUserTexture(texture);entry.Texture=texture;++uploaded;
    }catch(...){entry.Image.clear();}
    for(auto& entry:Entries)if(entry.Texture&&entry.Texture->Status==ImTextureStatus_OK&&entry.Texture->Pixels)entry.Texture->DestroyPixels();
}
bool Draw(const std::string& name,ImVec2 a,ImVec2 b){if(!ImGui::IsRectVisible(a,b))return false;auto* entry=Find(name);if(!entry)return false;entry->Wanted=ImGui::GetFrameCount();if(!entry->Texture||entry->Texture->Status!=ImTextureStatus_OK)return false;ImGui::GetWindowDrawList()->AddImage(entry->Texture->GetTexRef(),a,b);return true;}
std::vector<std::string> Portraits(){std::vector<std::string> out;for(const auto& entry:Entries)if(!entry.Image.empty())out.push_back(Portrait(entry.Key));return out;}
std::vector<std::string> Names(){std::vector<std::string> out;for(const auto& entry:Entries)out.push_back(entry.Name);return out;}
std::string Label(const std::string& key){const auto* entry=Find(key);return entry?entry->Name:key;}
std::string Journal(const std::string& name){const auto* entry=Find(name);return entry?entry->Journal:std::string{};}
std::string Page(const std::string& name){const auto* entry=Find(name);return entry?entry->Page:std::string{};}
void Cancel()noexcept{Cancelled=true;}
void Shutdown()noexcept{Cancelled=true;if(Worker.valid())try{Worker.get();}catch(...){}
    if(ImGui::GetCurrentContext()){for(auto& entry:Entries)if(entry.Texture){ImGui::UnregisterUserTexture(entry.Texture);IM_DELETE(entry.Texture);}for(auto* texture:Retired){ImGui::UnregisterUserTexture(texture);IM_DELETE(texture);}}
    Entries.clear();Retired.clear();Initialized=false;}
}
