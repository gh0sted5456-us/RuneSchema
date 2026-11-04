#pragma once
#include "Core/CacheBundleIO.h"
#include <string>

namespace PS::ModMenu2Key {
inline constexpr char Hash[]="8ae005b3077dca88a80801b3a66c94028d203a2efde1117a55fa1cb93f35ae8d";
inline constexpr char LicenseDigest[]="3c94f3ffc9dcedcaab881d1449715dbc2d12c55e6fe884111f2fe4b24f57bf02";
inline std::string Normalize(std::string text){std::erase(text,'\r');if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);while(text.ends_with('\n'))text.pop_back();return text;}
inline bool ValidText(const std::string& text){const auto normalized=Normalize(text);const auto split=normalized.find("\n\n");return split!=normalized.npos&&normalized.substr(0,split)==Hash&&CacheBundleIO::Hash(normalized.substr(split+2))==LicenseDigest;}
inline bool Present(const std::filesystem::path& root){for(const auto* folder:{"RuneSchema.Helpy","Helpy"}){const auto file=root/"plugins"/folder/"modmenu2.key";CacheBundleIO::Safe(file);if(std::filesystem::is_regular_file(file)&&ValidText(CacheBundleIO::Read(file,8192)))return true;}return false;}
}
