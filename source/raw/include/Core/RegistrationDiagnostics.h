#pragma once
#include <map>
#include <cstddef>
#include <utility>
#include <string>
#include <string_view>
namespace PS::RegistrationDiagnostics {
// A content root is a diagnostic source hint, not a claim about the owning PAK
// filename. Counts never influence enrollment, network identity or cleanup.
inline std::string Source(std::string_view path) {
    if(path.empty()||path.front()!='/')return "<unknown>";
    std::size_t end=path.find('/',1);
    if(path.starts_with("/Game/Mods/")||path.starts_with("/Game/RuneSchema/")){
        const auto owner=path.find('/',path.find('/',6)+1);
        if(owner!=path.npos)end=owner;
    }
    auto name=std::string(path.substr(1,end==path.npos?path.size()-1:end-1));
    if(name.empty()||name.size()>96||name.find_first_of("\r\n\t")!=name.npos)return "<unknown>";
    return name;
}
struct Count {std::size_t Added=0,Existing=0;};
class Sources {
    std::map<std::string,Count> m_rows;
public:
    void Add(std::string_view path,bool added) noexcept {
        // Optional observations must never reject a successfully enrolled asset.
        try{auto source=Source(path);if(!m_rows.contains(source)&&m_rows.size()>=128)source="<other>";
            auto& count=m_rows[source];if(added)++count.Added;else ++count.Existing;
        }catch(...){/* No enrollment/cleanup state is changed by diagnostics. */}
    }
    const auto& Rows() const {return m_rows;}
    auto Take(){auto rows=std::move(m_rows);m_rows.clear();return rows;}
};
}
