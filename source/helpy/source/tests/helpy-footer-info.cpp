#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){
    if(argc!=2)throw std::runtime_error("Expected standalone UI source");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("UI source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto begin=source.find("void RenderRuntimeFooter("),end=source.find("void ApplyCaptionTheme(",begin);
    if(begin==source.npos||end==source.npos)throw std::runtime_error("Runtime footer implementation missing");
    const auto footer=source.substr(begin,end-begin);
    for(const auto* moved:{"RuneSchemaVersion","GameVersion","UE4SS","BuildLabel","BuildId"})
        if(footer.find(moved)!=footer.npos)throw std::runtime_error("Version/build information leaked back into the runtime footer");
    for(const auto* required:{"HelpyEnvironment::Label()","F2  close / reopen","EpicTexture"})
        if(footer.find(required)==footer.npos)throw std::runtime_error("Runtime badge or F2 hint missing");
    const auto info=source.substr(source.find("if(notice==Notice::Info)"),source.find("void RenderInformationBar(")-source.find("if(notice==Notice::Info)"));
    for(const auto* required:{"RuneSchemaVersion","GameVersion","UE4SS","BuildId"})
        if(info.find(required)==info.npos)throw std::runtime_error("Info does not contain runtime/build metadata");
    if(source.find("SoftButton(\"Info\")")==source.npos||source.find("SoftButton(\"Credits\")")==source.npos||source.find("SoftButton(\"Legal\")")==source.npos)
        throw std::runtime_error("Info/Credits/Legal links are missing");
}
