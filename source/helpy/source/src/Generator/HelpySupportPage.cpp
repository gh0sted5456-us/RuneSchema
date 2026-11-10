#include "Generator/HelpySupportPage.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Runtime/HostServices.h"
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <d3d11.h>
#include <wincodec.h>
#include <md4c.h>
#include <md4c-html.h>
#include <array>
#include <fstream>
#include <memory>
#include <sstream>
#include <string_view>

namespace PS::HelpySupportPage {
namespace {
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
constexpr std::wstring_view Site=L"https://gh0sted5456-us.github.io/RuneSchema/";
constexpr wchar_t SupportUrl[]=L"https://gh0sted5456-us.github.io/RuneSchema/experimental/HELPY-SUPPORT/";
constexpr wchar_t LocalUrl[]=L"https://helpy-guide.invalid/index.html";
constexpr std::wstring_view LocalImages=L"https://helpy-guide.invalid/assets/helpy-support/";
constexpr wchar_t CodesUrl[]=L"https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/";
constexpr wchar_t BugsUrl[]=L"https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs";
constexpr int DocumentId=401;
constexpr std::array<std::string_view,6> Images{"help.png","items.png","notifications.png","scorned.png","topics.png","world.png"};
struct View {
    HWND Window{};
    HINSTANCE Module{};
    ID3D11Device* Device{};
    RECT Bounds{};
    bool Allowed=false,Requested=false;
};
thread_local View view;
std::string_view Resource(HINSTANCE module,int id){
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));
    if(!resource)return {};
    const auto data=LoadResource(module,resource);
    const auto* bytes=data?static_cast<const char*>(LockResource(data)):nullptr;
    return bytes?std::string_view(bytes,SizeofResource(module,resource)):std::string_view{};
}
int ImageIndex(std::string_view path){
    if(!path.starts_with("assets/helpy-support/"))return -1;
    const auto name=path.substr(std::string_view("assets/helpy-support/").size());
    const auto index=std::find(Images.begin(),Images.end(),name);
    return index==Images.end()?-1:static_cast<int>(index-Images.begin());
}
std::string Markdown(HINSTANCE module){
    std::istringstream lines{std::string(Resource(module,DocumentId))};
    std::string document,line;
    bool first=true,metadata=false,note=false;
    while(std::getline(lines,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(first){metadata=line=="---";first=false;if(metadata)continue;}
        if(metadata){if(line=="---")metadata=false;continue;}
        if(line.starts_with("!!! ")){
            note=true;
            const auto quote=line.find('"');
            document+="> ";
            if(quote!=line.npos)document+=line.substr(quote+1,line.size()-quote-2);
            document+="\n>\n";
            continue;
        }
        if(note&&line.starts_with("    "))document+="> "+line.substr(4)+'\n';
        else{note=false;document+=line+'\n';}
    }
    return document;
}
std::string Html(HINSTANCE module){
    const auto markdown=Markdown(module);
    std::string body;
    md_html(markdown.data(),static_cast<MD_SIZE>(markdown.size()),
        [](const MD_CHAR* text,MD_SIZE size,void* target){static_cast<std::string*>(target)->append(text,size);},
        &body,MD_FLAG_TABLES|MD_FLAG_NOHTML,0);
    for(size_t cursor=0;(cursor=body.find("href=\"",cursor))!=body.npos;){
        const auto begin=cursor+6,end=body.find('"',begin);
        if(end==body.npos)break;
        auto link=body.substr(begin,end-begin);
        const auto suffix=link.find(".md");
        if(suffix!=link.npos&&!link.starts_with("https://")){
            const bool parent=link.starts_with("../");
            link.replace(suffix,3,"/");
            if(parent)link.erase(0,3);
            link="https://gh0sted5456-us.github.io/RuneSchema/"+std::string(parent?"":"experimental/")+link;
            body.replace(begin,end-begin,link);
        }
        cursor=begin+link.size()+1;
    }
    return R"HTML(<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Helpy support</title>
<style>
html{color-scheme:dark}body{margin:0;background:#161410;color:#eee7d6;font:16px/1.55 'Segoe UI',sans-serif}
article{max-width:880px;margin:auto;padding:1rem clamp(1rem,5vw,3rem) 3rem}
h1,h2,h3{line-height:1.25;color:#efca7a}h2{margin-top:2rem;border-bottom:1px solid #4e4230;padding-bottom:.35rem}
a{color:#f4cc7d}img{display:block;max-width:100%;height:auto;border-radius:6px;border:1px solid #594c36}
blockquote{margin:1rem 0;border-left:3px solid #d4a95b;padding:.1rem 1rem;background:#242016}
table{border-collapse:collapse;width:100%}td,th{border:1px solid #514632;padding:.55rem;text-align:left}
code{background:#2c261a;border-radius:3px;padding:.15rem .3rem;overflow-wrap:anywhere}
</style><article>)HTML"+body+"</article></html>";
}
bool Docs(std::wstring_view url){return url.starts_with(Site);}
bool Local(std::wstring_view url){return url==LocalUrl||url.starts_with(std::wstring_view(L"https://helpy-guide.invalid/index.html#"));}
void OpenExternal(HWND parent,std::wstring_view uri){
    if(uri.starts_with(L"https://www.nexusmods.com/runescapedragonwilds/mods/570"))
        ShellExecuteW(parent,L"open",BugsUrl,nullptr,nullptr,SW_SHOWNORMAL);
}

struct Browser {
    HWND Window{};
    HINSTANCE Module{};
    ComPtr<ICoreWebView2Environment> Environment;
    ComPtr<ICoreWebView2Controller> Controller;
    ComPtr<ICoreWebView2> Page;
    bool Ready=false,Showing=false,Bundled=false,Failed=false;
    RECT Bounds{};
};
thread_local std::shared_ptr<Browser> browser;

void NavigateHome(Browser& browser){
    if(!browser.Page)return;
    browser.Bundled=false;
    browser.Ready=false;
    const wchar_t* url=SupportUrl;
#if defined(HELPY_PREVIEW)
    const auto* mode=std::getenv("RUNESCHEMA_HELPY_WEB_PREVIEW");
    if(mode&&std::string_view(mode)=="diagnostics")url=CodesUrl;
#endif
    if(FAILED(browser.Page->Navigate(url)))browser.Failed=true;
}
void Configure(const std::shared_ptr<Browser>& state){
    auto* page=state->Page.Get();
    const std::weak_ptr<Browser> weak=state;
    ComPtr<ICoreWebView2Settings> settings;
    if(SUCCEEDED(page->get_Settings(&settings))){
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_IsWebMessageEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
    }
    EventRegistrationToken token{};
    page->AddWebResourceRequestedFilter(L"https://helpy-guide.invalid/*",COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    page->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
        [weak](ICoreWebView2*,ICoreWebView2WebResourceRequestedEventArgs* args)->HRESULT{
            const auto state=weak.lock();if(!state)return S_OK;
            ComPtr<ICoreWebView2WebResourceRequest> request;
            if(FAILED(args->get_Request(&request)))return S_OK;
            LPWSTR text=nullptr;if(FAILED(request->get_Uri(&text))||!text)return S_OK;
            const std::wstring uri(text);CoTaskMemFree(text);
            std::string html;
            std::string_view bytes;
            const wchar_t* headers=L"Content-Type: image/png";
            if(Local(uri)){
                html=Html(state->Module);bytes=html;
                headers=L"Content-Type: text/html; charset=utf-8\r\nContent-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; img-src 'self'; base-uri 'none'; form-action 'none'";
            }else if(uri.starts_with(LocalImages)){
                const auto name=uri.substr(LocalImages.size());
                const auto index=std::find_if(Images.begin(),Images.end(),[&](auto image){return std::ranges::equal(name,image);});
                if(index!=Images.end())bytes=Resource(state->Module,DocumentId+static_cast<int>(index-Images.begin())+1);
            }
            ComPtr<IStream> stream;
            if(!bytes.empty())stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()),static_cast<UINT>(bytes.size())));
            ComPtr<ICoreWebView2WebResourceResponse> response;
            if(SUCCEEDED(state->Environment->CreateWebResourceResponse(stream.Get(),stream?200:404,stream?L"OK":L"Not Found",headers,&response)))
                args->put_Response(response.Get());
            return S_OK;
        }).Get(),&token);
    page->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [weak](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args)->HRESULT{
            const auto state=weak.lock();if(!state)return S_OK;
            LPWSTR text=nullptr;if(FAILED(args->get_Uri(&text))||!text){args->put_Cancel(TRUE);return S_OK;}
            const std::wstring uri(text);CoTaskMemFree(text);
            if(Docs(uri)||Local(uri)){state->Bundled=Local(uri);return S_OK;}
            args->put_Cancel(TRUE);
            BOOL user=FALSE;if(SUCCEEDED(args->get_IsUserInitiated(&user))&&user)OpenExternal(state->Window,uri);
            return S_OK;
        }).Get(),&token);
    page->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT{
            const auto state=weak.lock();if(!state)return S_OK;
            BOOL ok=FALSE;args->get_IsSuccess(&ok);
            COREWEBVIEW2_WEB_ERROR_STATUS error{};args->get_WebErrorStatus(&error);
            if(!ok&&error==COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)return S_OK;
            int status=0;ComPtr<ICoreWebView2NavigationCompletedEventArgs2> http;
            if(SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&http))))http->get_HttpStatusCode(&status);
            if(ok&&status<400){state->Ready=true;return S_OK;}
            if(!state->Bundled&&SUCCEEDED(state->Page->Navigate(LocalUrl)))return S_OK;
            state->Failed=true;state->Ready=false;
            return S_OK;
        }).Get(),&token);
    page->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
        [weak](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args)->HRESULT{
            args->put_Handled(TRUE);
            const auto state=weak.lock();if(!state)return S_OK;
            BOOL user=FALSE;args->get_IsUserInitiated(&user);
            if(!user)return S_OK;
            LPWSTR text=nullptr;if(FAILED(args->get_Uri(&text))||!text)return S_OK;
            const std::wstring uri(text);CoTaskMemFree(text);
            if(Docs(uri))state->Page->Navigate(uri.c_str());
            else OpenExternal(state->Window,uri);
            return S_OK;
        }).Get(),&token);
    page->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
        [](ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs* args)->HRESULT{
            return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
        }).Get(),&token);
    ComPtr<ICoreWebView2_4> downloads;
    if(SUCCEEDED(state->Page.As(&downloads)))downloads->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
        [](ICoreWebView2*,ICoreWebView2DownloadStartingEventArgs* args)->HRESULT{
            args->put_Handled(TRUE);return args->put_Cancel(TRUE);
        }).Get(),&token);
    state->Controller->add_MoveFocusRequested(Callback<ICoreWebView2MoveFocusRequestedEventHandler>(
        [weak](ICoreWebView2Controller*,ICoreWebView2MoveFocusRequestedEventArgs* args)->HRESULT{
            if(const auto state=weak.lock())SetFocus(state->Window);
            return args->put_Handled(TRUE);
        }).Get(),&token);
    NavigateHome(*state);
}
void StartBrowser(){
    browser=std::make_shared<Browser>();browser->Window=view.Window;browser->Module=view.Module;
    const std::weak_ptr<Browser> weak=browser;
    try{
        const auto profile=HostServices::StateDirectory()/L"help-browser";
        RuneSchemaData::Safe(profile);
        std::filesystem::create_directories(profile);
        const auto status=CreateCoreWebView2EnvironmentWithOptions(nullptr,profile.c_str(),nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [weak](HRESULT status,ICoreWebView2Environment* environment)->HRESULT{
                    const auto state=weak.lock();if(!state)return S_OK;
                    if(FAILED(status)||!environment){state->Failed=true;return S_OK;}
                    state->Environment=environment;
                    if(FAILED(environment->CreateCoreWebView2Controller(state->Window,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [weak](HRESULT status,ICoreWebView2Controller* controller)->HRESULT{
                                const auto state=weak.lock();
                                if(!state){if(controller)controller->Close();return S_OK;}
                                if(FAILED(status)||!controller){state->Failed=true;return S_OK;}
                                state->Controller=controller;controller->put_IsVisible(FALSE);
                                ComPtr<ICoreWebView2Controller2> color;
                                if(SUCCEEDED(state->Controller.As(&color)))color->put_DefaultBackgroundColor({255,22,20,16});
                                if(FAILED(controller->get_CoreWebView2(&state->Page))){state->Failed=true;return S_OK;}
                                Configure(state);
                                return S_OK;
                            }).Get())))state->Failed=true;
                    return S_OK;
                }).Get());
        if(FAILED(status))browser->Failed=true;
    }catch(...){browser->Failed=true;}
}
void ShowBrowser(bool visible){
    if(!browser||!browser->Controller||browser->Showing==visible)return;
    browser->Controller->put_IsVisible(visible);
    browser->Showing=visible;
    if(!visible&&IsChild(browser->Window,GetFocus()))SetFocus(browser->Window);
}

struct Block {std::string Text;int Heading=0,Image=-1;};
struct Document {std::vector<Block> Blocks;};
const std::vector<Block>& Blocks(HINSTANCE module){
    static const auto blocks=[module]{
        Document document;
        const auto markdown=Markdown(module);
        MD_PARSER parser{};
        parser.flags=MD_FLAG_TABLES|MD_FLAG_NOHTML;
        parser.enter_block=[](MD_BLOCKTYPE kind,void* detail,void* context){
            auto& blocks=static_cast<Document*>(context)->Blocks;
            if(kind==MD_BLOCK_H)blocks.push_back({{},static_cast<int>(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level)});
            else if(kind==MD_BLOCK_LI)blocks.push_back({"• "});
            else if(kind==MD_BLOCK_P||kind==MD_BLOCK_TR){
                if(blocks.empty()||blocks.back().Text!="• ")blocks.emplace_back();
            }else if((kind==MD_BLOCK_TD||kind==MD_BLOCK_TH)&&!blocks.empty()&&!blocks.back().Text.empty())blocks.back().Text+="  |  ";
            return 0;
        };
        parser.leave_block=[](MD_BLOCKTYPE,void*,void*){return 0;};
        parser.enter_span=[](MD_SPANTYPE kind,void* detail,void* context){
            auto& document=*static_cast<Document*>(context);
            if(kind==MD_SPAN_IMG){
                const auto& src=static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src;
                const auto index=ImageIndex({src.text,src.size});
                if(index>=0)document.Blocks.push_back({{},0,index});
            }
            return 0;
        };
        parser.leave_span=[](MD_SPANTYPE,void*,void*){return 0;};
        parser.text=[](MD_TEXTTYPE kind,const MD_CHAR* text,MD_SIZE size,void* context){
            auto& blocks=static_cast<Document*>(context)->Blocks;
            if(blocks.empty())blocks.emplace_back();
            if(kind==MD_TEXT_SOFTBR)blocks.back().Text+=' ';
            else if(kind==MD_TEXT_BR)blocks.back().Text+='\n';
            else blocks.back().Text.append(text,size);
            return 0;
        };
        md_parse(markdown.data(),static_cast<MD_SIZE>(markdown.size()),&parser,&document);
        return document.Blocks;
    }();
    return blocks;
}
thread_local std::array<ComPtr<ID3D11ShaderResourceView>,Images.size()> textures;
bool Upload(int index,std::string_view bytes,UINT width,UINT height){
    ComPtr<IWICImagingFactory> wic;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic))))return false;
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()),static_cast<UINT>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    if(!stream||FAILED(wic->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||
       FAILED(decoder->GetFrame(0,&frame))||FAILED(wic->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    std::vector<BYTE> pixels(static_cast<size_t>(width)*height*4);
    if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return false;
    D3D11_TEXTURE2D_DESC info{};
    info.Width=width;info.Height=height;info.MipLevels=info.ArraySize=1;info.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    info.SampleDesc.Count=1;info.Usage=D3D11_USAGE_IMMUTABLE;info.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};
    ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(view.Device->CreateTexture2D(&info,&data,&texture))&&
        SUCCEEDED(view.Device->CreateShaderResourceView(texture.Get(),nullptr,&textures[index]));
}
void RenderImage(int index,const std::string& alt){
    const auto bytes=Resource(view.Module,DocumentId+index+1);
    if(bytes.size()<24||!view.Device){ImGui::TextWrapped("%s",alt.c_str());return;}
    const auto dimension=[&](size_t at){
        UINT result=0;
        for(size_t i=at;i<at+4;++i)result=(result<<8)|static_cast<BYTE>(bytes[i]);
        return result;
    };
    const UINT width=dimension(16),height=dimension(20);
    if(!width||!height||width>4096||height>4096){ImGui::TextWrapped("%s",alt.c_str());return;}
    const auto display=std::max(1.f,std::min(float(width),ImGui::GetContentRegionAvail().x));
    const ImVec2 size{display,display*height/width};
    if(ImGui::IsRectVisible(size)&&(textures[index]||Upload(index,bytes,width,height)))
        ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(textures[index].Get())),size);
    else ImGui::Dummy(size);
}
void RenderDocument(){
    const auto& blocks=Blocks(view.Module);
    if(blocks.empty())ImGui::TextWrapped("The bundled support guide is unavailable.");
    for(const auto& block:blocks){
        if(block.Image>=0){RenderImage(block.Image,block.Text);continue;}
        if(block.Text.empty())continue;
        if(block.Heading==1)continue;
        if(block.Heading){ImGui::Spacing();ImGui::SeparatorText(block.Text.c_str());}
        else ImGui::TextWrapped("%s",block.Text.c_str());
    }
}
}

Frame::Frame(HWND window,HINSTANCE module,ID3D11Device* device,bool browserAllowed){
    view={window,module,device,{},browserAllowed,false};
}
Frame::~Frame(){
    const bool show=view.Allowed&&view.Requested&&IsWindowVisible(view.Window)&&!IsIconic(view.Window)&&
        !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    if(!show){ShowBrowser(false);return;}
    if(!browser)StartBrowser();
    if(!browser->Controller||browser->Failed)return;
    if(!EqualRect(&view.Bounds,&browser->Bounds)){
        browser->Controller->put_Bounds(view.Bounds);browser->Bounds=view.Bounds;
    }
    ShowBrowser(browser->Ready);
}
void Close(){
    if(browser&&browser->Controller)browser->Controller->Close();
    browser.reset();
}
void ReleaseImages(){for(auto& texture:textures)texture.Reset();}
void Render(){
    const bool canBrowse=view.Allowed&&!HelpyImGuiTheme::ControllerNavigation&&(!browser||!browser->Failed)&&
        !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    const bool ready=canBrowse&&browser&&browser->Ready;
    ImGui::SeparatorText("Helpy support");
    if(ready){
        BOOL canBack=FALSE;browser->Page->get_CanGoBack(&canBack);
        ImGui::BeginDisabled(!canBack||browser->Bundled);
        if(HelpyImGuiTheme::SoftButton("Back"))browser->Page->GoBack();
        ImGui::EndDisabled();ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Support home"))NavigateHome(*browser);
        ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Diagnostic codes"))browser->Page->Navigate(CodesUrl);
        if(browser->Bundled){ImGui::SameLine();ImGui::TextDisabled("Offline copy");}
    }else{
        if(HelpyImGuiTheme::SoftButton("Diagnostic codes"))ShellExecuteW(view.Window,L"open",CodesUrl,nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::SameLine();
        if(HelpyImGuiTheme::SoftButton("Report a bug"))ShellExecuteW(view.Window,L"open",BugsUrl,nullptr,nullptr,SW_SHOWNORMAL);
    }
    if(canBrowse){
        const auto at=ImGui::GetCursorScreenPos();
        const auto size=ImGui::GetContentRegionAvail();
        const auto clip=ImGui::GetWindowDrawList()->GetClipRectMax();
        const auto clipStart=ImGui::GetWindowDrawList()->GetClipRectMin();
        RECT client{};
        if(GetClientRect(view.Window,&client)){
            view.Bounds={std::max(client.left,LONG(std::max(clipStart.x,at.x))),std::max(client.top,LONG(std::max(clipStart.y,at.y))),
                std::min(client.right,LONG(std::min(clip.x,at.x+size.x))),
                std::min(client.bottom,LONG(std::min(clip.y,at.y+size.y)))};
            view.Requested=view.Bounds.right>view.Bounds.left+48&&view.Bounds.bottom>view.Bounds.top+48;
        }
        if(ready&&view.Requested){ImGui::Dummy(size);return;}
    }
    RenderDocument();
}

#if defined(HELPY_PREVIEW)
bool BrowserReady(){return browser&&browser->Ready&&browser->Showing;}
bool ProbeBrowser(const std::filesystem::path& result){
    if(!BrowserReady())return false;
    const auto png=result.parent_path()/L"embedded-guide.png";
    ComPtr<IStream> stream;
    if(FAILED(SHCreateStreamOnFileEx(png.c_str(),STGM_CREATE|STGM_WRITE|STGM_SHARE_DENY_WRITE,
                                     FILE_ATTRIBUTE_NORMAL,TRUE,nullptr,&stream)))return false;
    const auto* mode=std::getenv("RUNESCHEMA_HELPY_WEB_PREVIEW");
    const bool diagnostics=mode&&std::string_view(mode)=="diagnostics";
    const wchar_t* check=diagnostics
        ? L"location.pathname.endsWith('/ERROR-CODES/') && document.title.includes('Diagnostic codes')"
        : L"document.title.includes('Helpy support') && document.querySelectorAll('article img').length === 6 && [...document.querySelectorAll('article img')].every(i => i.complete && i.naturalWidth > 0)";
    const std::weak_ptr<Browser> weak=browser;
    return SUCCEEDED(browser->Page->ExecuteScript(check,Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [weak,stream,result](HRESULT status,LPCWSTR value)->HRESULT{
            const auto state=weak.lock();
            if(!state||FAILED(status)||!value||std::wstring_view(value)!=L"true"){
                std::ofstream(result)<<"FAIL embedded page content";
                return S_OK;
            }
            if(FAILED(state->Page->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
                Callback<ICoreWebView2CapturePreviewCompletedHandler>(
                    [stream,result](HRESULT status)->HRESULT{
                        stream->Commit(STGC_DEFAULT);
                        std::ofstream(result)<<(SUCCEEDED(status)?"PASS embedded page content":"FAIL capturing embedded page");
                        return S_OK;
                    }).Get())))std::ofstream(result)<<"FAIL starting embedded capture";
            return S_OK;
        }).Get()));
}
#endif
}
