#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/HelpyCacheSources.h"
#include <functional>
#include <limits>
#include <span>

// Directory metadata only. Container presence is not proof of a successful
// mount, live item availability, load-order precedence, or save ownership.
namespace PS::PakDiscovery {
inline constexpr size_t MaxEntries=524288,MaxIndex=64*1024*1024;
struct Result {std::set<std::string> Packages;std::string IndexKey,Error;bool Complete=false,MetadataOnly=false;};
struct Reader {
    std::string_view Data;size_t At=0;
    std::string_view Take(size_t count){if(count>Data.size()-At)throw std::runtime_error("Truncated container index");const auto out=Data.substr(At,count);At+=count;return out;}
    uint64_t Number(size_t bytes=4){const auto data=Take(bytes);uint64_t out=0;for(size_t i=0;i<bytes;++i)out|=uint64_t(static_cast<unsigned char>(data[i]))<<(8*i);return out;}
    size_t Count(){const auto count=Number();if(count>MaxEntries)throw std::runtime_error("Container index entry limit exceeded");return static_cast<size_t>(count);}
    bool Flag(){const auto value=Number();if(value>1)throw std::runtime_error("Invalid container index flag");return value!=0;}
    std::string Text(){const auto size=static_cast<int32_t>(Number());if(size==0)return {};if(size==INT32_MIN||std::abs(int64_t(size))>4096)throw std::runtime_error("Container path limit exceeded");
        const auto raw=Take(size<0?size_t(-size)*2:size_t(size));std::string value;
        if(size<0){for(size_t i=0;i<raw.size();i+=2){const auto c=uint16_t(static_cast<unsigned char>(raw[i]))|(uint16_t(static_cast<unsigned char>(raw[i+1]))<<8);
            if(i+2==raw.size()){if(c)throw std::runtime_error("Unterminated container path");break;}
            if(c<32||c>=127)throw std::runtime_error("Unsupported non-ASCII container path");value+=char(c);}}
        else {if(raw.back()!=0)throw std::runtime_error("Unterminated container path");value=std::string(raw.substr(0,raw.size()-1));}
        if(value.find_first_of("\\:\r\n\t")!=value.npos||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid container path");return value;
    }
};
inline std::string Package(std::string name,const std::string& game){
    name=HelpyCacheSources::Lower(name);while(name.starts_with("../"))name.erase(0,3);
    if(name.empty()||name.front()=='/'||name.find("..")!=name.npos||name.find("//")!=name.npos)return {};
    if(!name.ends_with(".uasset")&&!name.ends_with(".umap"))return {};
    const auto content=name.find("/content/");if(content==name.npos)return {};
    const auto prefix=name.substr(0,content),tail=name.substr(content+9,name.find_last_of('.')-content-9);
    std::string root;if(prefix==HelpyCacheSources::Lower(game))root="game";else if(prefix=="engine")root="engine";
    else if(prefix.find("/plugins/")!=prefix.npos||prefix.find('/')==prefix.npos)root=prefix.substr(prefix.find_last_of('/')+1);else return {};
    const auto path="/"+root+"/"+tail;return HelpyCacheSources::AssetKey(path)==path?path:std::string{};
}
struct File {
    std::filesystem::path Path;uintmax_t Size;std::filesystem::file_time_type Stamp;std::ifstream Input;
    explicit File(const std::filesystem::path& path):Path(path){
        // Installed containers may live behind Xbox/Vortex junctions; this reader never writes or extracts them.
        if(!std::filesystem::is_regular_file(path))throw std::runtime_error("Container is not a regular file");
        Size=std::filesystem::file_size(path);Stamp=std::filesystem::last_write_time(path);Input.open(path,std::ios::binary);if(!Input)throw std::runtime_error("Container is unreadable");
    }
    std::string Read(uint64_t offset,uint64_t bytes){if(offset>Size||bytes>Size-offset||bytes>MaxIndex)throw std::runtime_error("Container index range exceeds its bound");
        std::string out(static_cast<size_t>(bytes),'\0');Input.seekg(static_cast<std::streamoff>(offset));Input.read(out.data(),static_cast<std::streamsize>(bytes));if(!Input)throw std::runtime_error("Container index read failed");return out;}
    void Stable(){if(std::filesystem::file_size(Path)!=Size||std::filesystem::last_write_time(Path)!=Stamp)throw std::runtime_error("Container changed during discovery");}
};
inline void Sha1(std::string_view bytes,std::string_view expected){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup {BCRYPT_ALG_HANDLE& A;BCRYPT_HASH_HANDLE& H;~Cleanup(){if(H)BCryptDestroyHash(H);if(A)BCryptCloseAlgorithmProvider(A,0);}} cleanup{algorithm,hash};
    std::array<unsigned char,20> digest{};
    if(expected.size()!=digest.size()||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0
        ||BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0)<0||BCryptFinishHash(hash,digest.data(),digest.size(),0)<0
        ||!std::equal(digest.begin(),digest.end(),reinterpret_cast<const unsigned char*>(expected.data())))throw std::runtime_error("Container index SHA-1 mismatch");
}
inline Result LegacyPak(File& file,const std::string& game){
    constexpr size_t footerSize=44;const auto footer=file.Read(file.Size-footerSize,footerSize);Reader info{footer};
    if(info.Number()!=0x5a6f12e1||info.Number()!=3)throw std::runtime_error("Unsupported legacy PAK footer");
    const auto offset=info.Number(8),size=info.Number(8);const auto expected=info.Take(20);
    if(offset>file.Size-footerSize||size>file.Size-footerSize-offset)throw std::runtime_error("Invalid legacy PAK index range");
    const auto primary=file.Read(offset,size);Sha1(primary,expected);Reader index{primary};const auto mount=index.Text();Result result;
    for(size_t n=index.Count();n;--n){
        const auto name=index.Text();if(name.empty()||name.find("..")!=name.npos||name.find("//")!=name.npos||name.front()=='/')throw std::runtime_error("Invalid legacy PAK filename");
        const auto entryOffset=index.Number(8),stored=index.Number(8);index.Number(8);const auto compression=index.Number();index.Take(20);
        if(entryOffset>offset||stored>offset-entryOffset)throw std::runtime_error("Invalid legacy PAK entry range");
        if(compression)for(size_t blocks=index.Count();blocks;--blocks){const auto start=index.Number(8),end=index.Number(8);
            if(start>end||end>offset)throw std::runtime_error("Invalid legacy PAK compression range");}
        if(index.Number(1)>1)throw std::runtime_error("Invalid legacy PAK encryption flag");index.Number();
        const auto package=Package(mount+name,game);if(!package.empty())result.Packages.insert(package);
    }
    if(index.At!=primary.size())throw std::runtime_error("Unexpected legacy PAK index data");
    result.IndexKey=CacheBundleIO::Hash(primary);result.Complete=true;return result;
}
inline Result Pak(File& file,const std::string& game){
    if(file.Size>=44){const auto tail=file.Read(file.Size-44,8);Reader legacy{tail};
        if(legacy.Number()==0x5a6f12e1&&legacy.Number()==3)return LegacyPak(file,game);}
    // shortcut: v3 and v10/v11 full indexes only; reject other layouts rather than guess ownership.
    constexpr size_t footerSize=221;if(file.Size<footerSize)throw std::runtime_error("Missing PAK footer");
    const auto footer=file.Read(file.Size-footerSize,footerSize);Reader info{footer};info.Take(16);if(info.Number(1))throw std::runtime_error("Encrypted PAK index unavailable");
    if(info.Number()!=0x5a6f12e1)throw std::runtime_error("Unsupported PAK footer");const auto version=info.Number();if(version!=10&&version!=11)throw std::runtime_error("Unsupported PAK version");
    const auto offset=info.Number(8),size=info.Number(8);const auto expected=info.Take(20);if(offset>file.Size-footerSize||size>file.Size-footerSize-offset)throw std::runtime_error("Invalid PAK index range");
    const auto primary=file.Read(offset,size);Sha1(primary,expected);Reader index{primary};const auto mount=index.Text();const auto total=index.Count();index.Take(8);
    if(index.Flag())index.Take(36);if(!index.Flag())throw std::runtime_error("PAK full directory index unavailable");
    const auto directoryOffset=index.Number(8),directorySize=index.Number(8);const auto directoryHash=index.Take(20);
    if(directoryOffset>file.Size-footerSize||directorySize>file.Size-footerSize-directoryOffset)throw std::runtime_error("Invalid PAK directory range");
    const auto directory=file.Read(directoryOffset,directorySize);Sha1(directory,directoryHash);Reader rows{directory};Result result;size_t seen=0;
    for(size_t n=rows.Count();n;--n){const auto folder=rows.Text();if(folder.find("..")!=folder.npos)throw std::runtime_error("Invalid PAK directory path");
        for(size_t m=rows.Count();m;--m){if(++seen>total||seen>MaxEntries)throw std::runtime_error("PAK directory count mismatch");const auto name=rows.Text();const auto location=static_cast<int32_t>(rows.Number());
            if(name.empty()||name.find('/')!=name.npos)throw std::runtime_error("Invalid PAK filename");if(location==INT32_MIN||location==INT32_MAX)continue; // Native invalid/delete record.
            const auto package=Package(mount+(folder=="/"?"":folder)+name,game);if(!package.empty())result.Packages.insert(package);}}
    if(rows.At!=directory.size()||seen!=total)throw std::runtime_error("Unexpected PAK directory data");
    result.IndexKey=CacheBundleIO::Hash(primary+directory);result.Complete=true;return result;
}
inline Result IoStore(File& file,const std::string& game){
    const auto header=file.Read(0,144);Reader h{header};if(h.Take(16)!="-==--==--==--==-")throw std::runtime_error("Unsupported IoStore magic");
    const auto version=h.Number(1);if(version<3||version>8)throw std::runtime_error("Unsupported IoStore version");h.Take(3);if(h.Number()!=144)throw std::runtime_error("Unsupported IoStore header");
    const auto chunks=h.Count(),blocks=h.Number(),blockSize=h.Number(),methods=h.Number(),width=h.Number();h.Take(4);const auto directorySize=h.Number();
    h.Take(4);const auto containerId=h.Number(8);h.Take(16);const auto flags=h.Number(1);
    if(flags&2)throw std::runtime_error("Encrypted IoStore index unavailable");
    h.Take(3);const auto seeds=h.Number();h.Take(8);const auto overflow=h.Number();
    if(blocks>16*1024*1024||blockSize!=12||methods>32||width>128||seeds>MaxEntries||overflow>MaxEntries)throw std::runtime_error("IoStore table limit exceeded");
    uint64_t offset=144+uint64_t(chunks)*22+(version>=4?seeds*4:0)+(version>=5?overflow*4:0)+blocks*blockSize+methods*width;
    if(flags&4){const auto signatureBytes=file.Read(offset,4);Reader signature{signatureBytes};const auto count=signature.Number();if(count>4096)throw std::runtime_error("IoStore signature limit exceeded");offset+=4+count*2+blocks*20;}
    if(!(flags&8)||!directorySize){
        if(!(flags&8)&&!directorySize&&containerId==UINT64_MAX&&chunks==1&&offset<=file.Size
            &&HelpyCacheSources::Lower(file.Path.filename().string())=="global.utoc"){
            Result result;result.Complete=true;result.MetadataOnly=true;result.IndexKey=CacheBundleIO::Hash(header);return result;
        }
        throw std::runtime_error("IoStore directory index unavailable");
    }
    const auto directory=file.Read(offset,directorySize);Reader rows{directory};const auto mount=rows.Text();
    using Dir=std::array<uint32_t,4>;using Entry=std::array<uint32_t,3>;std::vector<Dir> dirs;std::vector<Entry> files;std::vector<std::string> strings;
    for(size_t n=rows.Count();n;--n){Dir value;for(auto& x:value)x=static_cast<uint32_t>(rows.Number());dirs.push_back(value);}
    for(size_t n=rows.Count();n;--n){Entry value;for(auto& x:value)x=static_cast<uint32_t>(rows.Number());files.push_back(value);}
    for(size_t n=rows.Count();n;--n)strings.push_back(rows.Text());if(rows.At!=directory.size()||dirs.empty())throw std::runtime_error("Invalid IoStore directory data");
    constexpr uint32_t none=UINT32_MAX;std::set<uint32_t> visitedDirs,visitedFiles;Result result;
    if(dirs[0][2]!=none)throw std::runtime_error("Invalid IoStore root sibling");
    const auto name=[&](uint32_t id){if(id>=strings.size()||strings[id].empty()||strings[id].find('/')!=strings[id].npos||strings[id]=="."||strings[id]=="..")throw std::runtime_error("Invalid IoStore filename");return strings[id];};
    std::function<void(uint32_t,std::string,size_t)> walk=[&](uint32_t id,std::string prefix,size_t depth){
        if(depth>128||id>=dirs.size()||!visitedDirs.insert(id).second)throw std::runtime_error("Invalid IoStore directory cycle");const auto& dir=dirs[id];
        if(dir[0]!=none)prefix+=name(dir[0])+"/";if(prefix.size()>4096)throw std::runtime_error("IoStore path limit exceeded");
        auto entry=dir[3];while(entry!=none){if(entry>=files.size()||!visitedFiles.insert(entry).second)throw std::runtime_error("Invalid IoStore file cycle");const auto& row=files[entry];if(row[2]>=chunks)throw std::runtime_error("Invalid IoStore chunk index");
            const auto package=Package(mount+prefix+name(row[0]),game);if(!package.empty())result.Packages.insert(package);entry=row[1];}
        auto child=dir[1];while(child!=none){walk(child,prefix,depth+1);child=dirs[child][2];}};
    walk(0,{},0);if(visitedDirs.size()!=dirs.size()||visitedFiles.size()!=files.size())throw std::runtime_error("Unreachable IoStore directory entries");
    result.IndexKey=CacheBundleIO::Hash(header+directory);result.Complete=true;return result;
}
inline Result Read(const std::filesystem::path& path,const std::string& game){
    try{File file(path);const auto extension=HelpyCacheSources::Lower(path.extension().string());auto result=extension==".pak"?Pak(file,game):extension==".utoc"?IoStore(file,game):Result{};file.Stable();return result;}
    catch(const std::exception& error){Result result;result.Error=error.what();return result;}
}
struct Root {std::filesystem::path Path;std::string Owner;};
inline std::mutex RootsMutex;
inline std::vector<Root> Roots;
inline void ResetRoots() noexcept {try{std::lock_guard lock(RootsMutex);Roots.clear();}catch(...){}}
inline void RememberRoot(const std::filesystem::path& path,const std::string& owner={}) noexcept {try{std::lock_guard lock(RootsMutex);if(Roots.size()<4096)Roots.push_back({path,HelpyCacheSources::Lower(owner)});}catch(...){}}
inline std::vector<Root> ReadRoots(){std::lock_guard lock(RootsMutex);return Roots;}
}
