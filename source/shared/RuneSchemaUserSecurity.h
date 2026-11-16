#pragma once
#include <windows.h>
#include <sddl.h>
#include <string>
#include <vector>
namespace PS::UserSecurity {
inline bool SameUserPipeSecurity(PSECURITY_DESCRIPTOR& descriptor){
    HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;DWORD bytes=0;
    GetTokenInformation(token,TokenUser,nullptr,0,&bytes);std::vector<char> data(bytes);
    const bool ok=bytes&&GetTokenInformation(token,TokenUser,data.data(),bytes,&bytes);CloseHandle(token);if(!ok)return false;
    LPWSTR sid{};if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid))return false;
    const auto sddl=std::wstring(L"D:P(A;;GA;;;")+sid+L")";LocalFree(sid);
    return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)!=FALSE;
}
}
