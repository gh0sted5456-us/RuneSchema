#pragma once
#include <array>
#include <string_view>
#include <cstddef>
namespace PS::HelpyUiAssets {
struct File {const char* Name;size_t Bytes;const char* SHA256;};
inline constexpr std::array Files{
    File{"source-base-game.png",4524400,"fad23ef33f8d1e4b26a8238853bc028075ab3f98abc210e80932e5986c224d81"},
    File{"source-umbral-sands.png",3067770,"4f0923926be681b9f7d72d31eb123b203324859d978981edcc47102c51531fe0"},
    File{"source-scorched-wilderness.png",4144594,"ea70adf2b7e5b0e745f2b92fc1232fb9728ad6ed6c7a0bea04090672aa5b395c"},
    File{"source-bastion.png",3675550,"c4ac544169b4e14603a33c4fa1d55014817d6f1774c75b1737faadfcd295fa97"},
    File{"source-colorfuldye.png",5221157,"8484c93472e9cb9aeb14fc73a48a9bd05b887950af0a6e7b115d4b722bb2bc56"},
    File{"source-gunslinger.png",3785147,"853298ee52c1bfea7b279f40debdf7ef6ba26721c22a195604731a611008e15b"},
    File{"source-shadowmancer.png",3748145,"af5602c5a0291aa3de97a4ad75c001f6d0edc3495958cf52bffe98022222f73c"},
    File{"source-spears-scythes.png",5180487,"0e99258a1f502242d44aea5f4fb763917b10db20de0e3058e5c8f671a19a17bb"},
    File{"curseforge-details.png",1524773,"d3b2406516c90d2f11e39a4f631dfb72c9ba3c59bfe9a62140a538256cdad6bd"},
    File{"curseforge.png",1801,"c3ee332f97df98b504e6a78620f3208acdc0d8f33faf15eefea6ed315b89e3b8"},
    File{"discord.png",4870,"7f4745c9af729592915a6f98b4b4c9911bef6675076a745a0e583369e9e0c0db"},
    File{"dragonwilds.png",418376,"0221cdbe547dc2a47bb9a5d9e973245de557204f69255b7eba957a634ef63df6"},
    File{"epic.png",39636,"4a88356d92310b703180cd770364d5045388efa3c1e87ec921ec943309bb26c7"},
    File{"github-details.png",64691,"3c22659369faae88b5e136ae9712a0f85c5b0b515f632ff62ba8b8a9df79e955"},
    File{"github.png",4444,"ee9e1a050ab2a0c0a68a8445b393fd2dcd36f57cf305ed2808d3a6f66673f809"},
    File{"home-banner.png",3717300,"0a860ad0cda9b3d80ba08094319d2ad6d3610000dfc73fc68453d56173d441a0"},
    File{"loading-ring.png",8943,"750283f96b281adbecf472e9dd55e90469ac4503126d34212b0fa1be01d4496e"},
    File{"nexus-details.png",1327872,"f6d73840a84ac78b8bb45d973ffcbe6d50c262bb639dda7ad47f08703082433b"},
    File{"nexus.png",17171,"51dbfb455d8794ff193ca56a8a97c4071e9e5c03d0e4340d49d1100f6a7a4adc"},
    File{"RuneSchema.png",1017354,"541bd50fe811c27e2a5df52ede0262c06c519207261538887b74fe98aeae8327"},
    File{"steam.png",21639,"fc9b6d65fe7216dcb2f629cc8fdc54f22f04389133e45e66194ed4a175d14c82"},
    File{"xbox.png",7094,"5b1927d43fcf2118bca48247d6bb7821585b5c7fdb927f469748d7eac1f6e3fa"}
};
inline constexpr const char* Version="1";
inline bool Known(std::string_view name){for(const auto& file:Files)if(name==file.Name)return true;return false;}
}
