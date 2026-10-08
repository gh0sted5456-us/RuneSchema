#pragma once
// Bounded local preference I/O. This layer does not understand item identities or JSON.
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace PS::F2PreferenceFile {
inline constexpr std::size_t MaxBytes=1024*1024;
// Normal preferences replace their previous value. Migration may only publish
// a new file; it must not clobber a destination created by another writer.
enum class WriteMode { ReplaceExisting, CreateOnly };
inline std::optional<std::string> Read(const std::filesystem::path& file,std::size_t maxBytes=MaxBytes) {
    if(std::filesystem::is_symlink(file))throw std::runtime_error("F2 settings/cache target is a symlink");
    if(!std::filesystem::exists(file))return std::nullopt;
    if(!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)>maxBytes)
        throw std::runtime_error("F2 settings/cache file is not a bounded regular file");
    std::ifstream input(file,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open F2 settings/cache file");
    std::string text;char block[4096];
    while(input.read(block,sizeof(block))||input.gcount()) {
        text.append(block,static_cast<std::size_t>(input.gcount()));
        if(text.size()>maxBytes)throw std::runtime_error("F2 settings/cache file exceeds the configured limit");
    }
    if(input.bad())throw std::runtime_error("Cannot read F2 settings/cache file");
    return text;
}
template<class Verify>
void Write(const std::filesystem::path& file,const std::string& text,Verify&& verify,
    std::size_t maxBytes=MaxBytes,WriteMode mode=WriteMode::ReplaceExisting) {
    if(text.size()>maxBytes)throw std::runtime_error("F2 settings/cache exceed the configured limit");
    if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
    if(std::filesystem::is_symlink(file))throw std::runtime_error("F2 settings/cache target is a symlink");
    auto staging=file;staging += ".pending";
    // Do not overwrite/delete another writer's staging file. The finally/catch
    // cleanup below begins only after this operation has created its own staging file.
#ifdef _WIN32
    HANDLE handle=CreateFileW(staging.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create F2 settings/cache staging file");
    DWORD count=0;const bool flushed=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&count,nullptr)
        &&count==text.size()&&FlushFileBuffers(handle);
    const bool closed=CloseHandle(handle)!=FALSE;
#else
    const int descriptor=::open(staging.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(descriptor<0)throw std::runtime_error("Cannot create F2 settings/cache staging file");
    std::size_t written=0;bool complete=true;
    while(written<text.size()) {
        const auto n=::write(descriptor,text.data()+written,text.size()-written);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){complete=false;break;}written+=static_cast<std::size_t>(n);
    }
    const bool flushed=complete&&::fsync(descriptor)==0;
    const bool closed=::close(descriptor)==0;
#endif
    try {
        if(!flushed||!closed)throw std::runtime_error("Could not flush F2 settings/cache");
        verify(staging);
        // Check again after readback before replacing the target.
        if(std::filesystem::is_symlink(file))throw std::runtime_error("F2 settings/cache target became a symlink");
#ifdef _WIN32
        const DWORD flags=mode==WriteMode::CreateOnly ? MOVEFILE_WRITE_THROUGH
            : MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH;
        if(!MoveFileExW(staging.c_str(),file.c_str(),flags))
            throw std::runtime_error("Cannot publish F2 settings/cache file (destination retained)");
#else
        if(mode==WriteMode::CreateOnly) {
            // Same-directory hard-link publication is atomic and fails if the
            // destination exists. A check followed by rename would have a race.
            if(::link(staging.c_str(),file.c_str())!=0)
                throw std::runtime_error("Cannot publish new F2 settings/cache file (destination retained)");
            std::filesystem::remove(staging);
        }else std::filesystem::rename(staging,file);
#endif
    }catch(...) {std::error_code ignored;std::filesystem::remove(staging,ignored);throw;}
}
} // namespace PS::F2PreferenceFile
