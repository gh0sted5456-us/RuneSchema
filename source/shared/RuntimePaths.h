#pragma once
#include <filesystem>
#include <string_view>

namespace PS::RuntimePaths {
namespace fs=std::filesystem;
inline fs::path Helpy(const fs::path& root){return fs::is_directory(root/"Helpy")?root/"Helpy":root/"plugins/RuneSchema.Helpy";}
inline fs::path ModFromHelpy(const fs::path& helpy){return helpy.filename()=="Helpy"&&helpy.parent_path().filename()!="plugins"?helpy.parent_path():helpy.parent_path().parent_path();}
inline fs::path HelpyFromBinary(const fs::path& image){return image.parent_path().filename()=="dll"?image.parent_path().parent_path():image.parent_path();}
inline fs::path ModFromBinary(const fs::path& image){return ModFromHelpy(HelpyFromBinary(image));}
inline fs::path HelpyBinary(const fs::path& root,std::string_view name){
 const auto folder=Helpy(root);const bool current=folder==root/"Helpy";
 return (current?folder:folder/"dll")/(name=="Helpy.dll"&&!current?"RuneSchema.Helpy.dll":name);
}
inline fs::path Plugin(const fs::path& root,std::string_view id){
 if(id=="RuneSchema.Helpy")return Helpy(root);
 for(const auto& base:{root/"dlls/plugins",root/"Helpy/plugins"})if(fs::is_directory(base/id))return base/id;
 return root/"plugins"/id;
}
inline fs::path PluginOrder(const fs::path& root){return fs::is_regular_file(root/"settings/plugins.txt")?root/"settings":root/"plugins";}
}
