#pragma once
#include "Core/RuneSchemaDataRoot.h"
#include <algorithm>
#include <fstream>
#include <string>

namespace PS::DeveloperToolsKey {
inline constexpr char LicenseHash[]="c305102727d92699e78aa77b1769f65ddf4ad12468f950bc05300e32ce7e2309";
inline std::string Normalize(std::string text){
 std::erase(text,'\r');
 if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);
 while(text.ends_with('\n'))text.pop_back();
 return text;
}
inline bool ValidText(const std::string& text){return Normalize(text)==LicenseHash;}
inline bool Present(const std::filesystem::path& pluginRoot)noexcept{
 try{
  if(pluginRoot.empty())return false;
  const auto file=pluginRoot/"devmenu.key";RuneSchemaData::Safe(file);
  if(!std::filesystem::is_regular_file(file))return false;
  std::ifstream stream(file,std::ios::binary);char text[257]{};
  stream.read(text,sizeof(text));
  return !stream.bad()&&stream.gcount()<=256&&ValidText(std::string(text,size_t(stream.gcount())));
 }catch(...){return false;}
}
}
