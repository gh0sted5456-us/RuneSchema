#include "Core/HelpyIconPack.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <iostream>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;using Microsoft::WRL::ComPtr;
void Check(HRESULT value){if(FAILED(value))throw std::runtime_error("Banner image could not be decoded");}
Json Pixels(const fs::path& file){
    HelpyIconPack::Safe(file);if(fs::file_size(file)>20*1024*1024)throw std::runtime_error("Banner exceeds 20 MiB");
    ComPtr<IWICImagingFactory> factory;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;Check(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;Check(decoder->GetFrame(0,&frame));UINT width=0,height=0;Check(frame->GetSize(&width,&height));if(!width||!height||width>4096||height>4096)throw std::runtime_error("Banner dimensions exceed limit");
    ComPtr<IWICBitmapScaler> scaler;Check(factory->CreateBitmapScaler(&scaler));Check(scaler->Initialize(frame.Get(),SourceBanner::Width,SourceBanner::Height,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter;Check(factory->CreateFormatConverter(&converter));Check(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<unsigned char> pixels(SourceBanner::Width*SourceBanner::Height*4);Check(converter->CopyPixels(nullptr,SourceBanner::Width*4,UINT(pixels.size()),pixels.data()));return SourceBanner::Encode(pixels);
}
int main(int argc,char** argv)try{
    if(argc!=4&&argc!=5)throw std::runtime_error("Usage: source-banner-packs verified-input.rsmk-cache artwork-folder NEW-output-folder [mods/owner=registered/source]");
    Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));const fs::path input=argv[1],assets=argv[2],output=argv[3];if(fs::exists(output))throw std::runtime_error("Output must be a new folder; existing packs are never overwritten");
    HelpyIconPack::Safe(input);std::ifstream stream(input,std::ios::binary);uint64_t start=0;const auto original=HelpyIconPack::Header(stream,start,fs::file_size(input));
    if(original.Version!=2)throw std::runtime_error("A typed content cache is required; icons alone cannot prove owner routing");
    std::map<std::string,std::string> sources;for(const auto& row:original.Records){if(!SourceBanner::Builtin(row.Owner)[0])continue;
        if(row.Owner.starts_with("mods/")&&(row.Source=="game"||row.Source=="engine"||row.Source=="umbralsands"||row.Source=="scornedwilderness"||row.Source=="dowdunreach"))continue;
        if(!sources.contains(row.Owner))sources[row.Owner]=row.Source;
    }
    if(argc==5){const std::string route=argv[4];const auto at=route.find('=');if(at==route.npos)throw std::runtime_error("Explicit author route needs owner=source");const auto owner=route.substr(0,at),source=route.substr(at+1);if(!owner.starts_with("mods/")||!ContentSource::Valid(owner)||!original.Sources.contains(source)||!SourceBanner::Builtin(owner)[0])throw std::runtime_error("Explicit author route is not in the supplied source snapshot");sources[owner]=source;}
    Json report=Json::array();std::map<std::string,Json> images;
    for(const auto& [owner,source]:sources){const auto name=SourceBanner::Builtin(owner);if(!images.contains(name))images[name]=Pixels(assets/name);
        const auto path="/"+source+"/__Helpy__/SourceBanner_"+HelpySourceCache::Hex(HelpySourceCache::Hash(owner))+".SourceBanner";
        const auto row=CacheBundle::Parse({{"Kind","art"},{"Owner",owner},{"Path",path},{"Data",{{"Name","Source banner"},{"SourceBannerPixels",images[name]}}}});
        const Json manifest={{"Version",2},{"Kind","RuneSchemaCacheBundle"},{"Locale",original.Locale},{"Records",Json::array({CacheBundle::Document(row)})},{"Icons",Json::array()},{"Sources",{{source,original.Sources.at(source)}}},{"RecordCoverage",{{"Complete",false},{"Scope","Source banner only; no discovery or gameplay registration"}}}};
        (void)HelpyIconPack::Parse(manifest,0);const auto text=manifest.dump();std::vector<char> bytes(HelpyIconPack::BundleMagic,HelpyIconPack::BundleMagic+8);const uint32_t length=uint32_t(text.size());const auto* encoded=reinterpret_cast<const char*>(&length);bytes.insert(bytes.end(),encoded,encoded+4);bytes.insert(bytes.end(),text.begin(),text.end());
        const auto file=output/owner/"cache"/"source-banner.rsmk-cache";HelpyIconPack::Publish(file,bytes,false);
        std::ifstream verify(file,std::ios::binary);uint64_t offset=0;const auto parsed=HelpyIconPack::Header(verify,offset,fs::file_size(file));if(parsed.Records.size()!=1||SourceBanner::Decode(parsed.Records.front().Data.at("SourceBannerPixels")).size()!=256*144*4)throw std::runtime_error("Banner round-trip failed");
        report.push_back({{"Owner",owner},{"Source",source},{"Artwork",name},{"File",file.generic_string()},{"SHA256",HelpyIconPack::Hash(bytes)},{"Bytes",bytes.size()}});
    }
    const auto receipt=report.dump(2);HelpyIconPack::Publish(output/"banner-pack-report.json",{receipt.begin(),receipt.end()},false);std::cout<<receipt<<'\n';CoUninitialize();return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
