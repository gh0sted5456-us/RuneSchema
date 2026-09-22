#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
static void Require(bool value,const char* what){if(!value)throw std::runtime_error(std::string("Item icon regression: ")+what);}

int main(int argc,char** argv) {
    if(argc!=9)throw std::runtime_error("Expected panel, picker, cache, thumbnail, asset search, spawn tools, icon subloader and tool request sources");
    const auto panel=Read(argv[1]);const auto picker=Read(argv[2]);const auto cache=Read(argv[3]);const auto thumbnail=Read(argv[4]);
    const auto assets=Read(argv[5]);const auto spawns=Read(argv[6]);const auto icons=Read(argv[7]);const auto requests=Read(argv[8]);

    Require(assets.find("{\"Icon\",ReferencePath(object,\"Icon\")}")!=std::string::npos,
        "ItemRoster no longer captures the real Icon reference");
    Require(panel.find("ItemGridPicker::Render(\"inventory\"")!=std::string::npos
        && picker.find("ItemIconCache::Update()")!=std::string::npos && picker.find("ItemIconCache::Draw(iconPath")!=std::string::npos,
        "Settings grid is not wired through the shared picker to the icon cache");
    Require(picker.find("predates icon capture")!=std::string::npos,
        "old saved catalogs no longer explain why icons are missing");
    Require(cache.find("ImGui::RegisterUserTexture")!=std::string::npos && cache.find("ImTextureStatus_WantCreate")!=std::string::npos,
        "ImGui dynamic texture bridge is missing");
    Require(cache.find("state.Textures.size()>=512")!=std::string::npos,
        "icon texture cache no longer has a hard bound");
    Require(cache.find("std::chrono::milliseconds(100)")!=std::string::npos,
        "GPU readback requests are no longer throttled");
    Require(cache.find("texture->DestroyPixels()")!=std::string::npos,
        "CPU thumbnail pixels are not released after GPU upload");
    Require(thumbnail.find("ThumbnailSize=64")!=std::string::npos,
        "thumbnail size is no longer bounded to 64x64");
    Require(thumbnail.find("CreateRenderTarget2D")!=std::string::npos
        && thumbnail.find("BeginDrawCanvasToRenderTarget")!=std::string::npos
        && thumbnail.find("K2_DrawTexture")!=std::string::npos
        && thumbnail.find("ReadRenderTarget")!=std::string::npos,
        "game-thread Texture2D to RGBA bridge is incomplete");
    Require(thumbnail.find("RTF_RGBA8_SRGB")!=std::string::npos && thumbnail.find("BLEND_Translucent")!=std::string::npos,
        "transparent sRGB thumbnail contract regressed");
    Require(thumbnail.find("SetObjectPropertyValue")==std::string::npos
        && thumbnail.find("std::memcpy(address,&value,sizeof(value))")!=std::string::npos,
        "item-icon object parameters still depend on unavailable FObjectPropertyBase virtual setters");
    Require(thumbnail.find("TEXT(\"R\")")!=std::string::npos && thumbnail.find("TEXT(\"G\")")!=std::string::npos
        && thumbnail.find("TEXT(\"B\")")!=std::string::npos && thumbnail.find("TEXT(\"A\")")!=std::string::npos,
        "FColor channels are no longer read reflectively");
    Require(spawns.find("ItemIconRequests::Take()")==std::string::npos
        && icons.find("ItemIconRequests::Take()")!=std::string::npos
        && icons.find("ItemIconThumbnail::Render(m_readyWorld, icon)")!=std::string::npos,
        "icon requests are not isolated in the /spawns item-icon subloader");
    Require(requests.find("using ItemIconRequests=ToolRequest<ItemIconTag>")!=std::string::npos,
        "item icon requests are no longer isolated from spawn-tool status");

    std::cout<<"PASS: Shared item picker captures real icon references, renders bounded game-thread thumbnails and uploads them lazily to ImGui.\n";
}
