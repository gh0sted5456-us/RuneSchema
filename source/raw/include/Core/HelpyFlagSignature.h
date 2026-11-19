#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#pragma comment(lib,"bcrypt.lib")
// ECDSA P-256 / SHA-256 verification for Helpy delivery flags (keep the helpy and raw copies identical).
// Keys are SubjectPublicKeyInfo DER (uncompressed P-256 point only); signatures are DER
// SEQUENCE{INTEGER r, INTEGER s}. Parsing is strict so one key and one signature have one encoding.
namespace PS::HelpyDeliveryFlags::Signature {
using Bytes=std::vector<uint8_t>;
using Verifier=std::function<bool(const Bytes& key,std::string_view message,const Bytes& signature)>;
inline Bytes FromHex(std::string_view text){
    if(text.size()%2||text.size()>2048)throw std::runtime_error("hex text has an invalid length.");
    const auto nibble=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    Bytes out;out.reserve(text.size()/2);
    for(size_t i=0;i<text.size();i+=2){const int high=nibble(text[i]),low=nibble(text[i+1]);if(high<0||low<0)throw std::runtime_error("hex text has an invalid digit.");out.push_back(static_cast<uint8_t>(high*16+low));}
    return out;
}
inline std::string Hex(const uint8_t* data,size_t size){
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(size*2);
    for(size_t i=0;i<size;++i){out+=digits[data[i]>>4];out+=digits[data[i]&15];}
    return out;
}
inline std::array<uint8_t,32> Sha256(const void* data,size_t size){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup{BCRYPT_ALG_HANDLE& A;BCRYPT_HASH_HANDLE& H;~Cleanup(){if(H)BCryptDestroyHash(H);if(A)BCryptCloseAlgorithmProvider(A,0);}}cleanup{algorithm,hash};
    std::array<uint8_t,32> digest{};
    if(size>ULONG_MAX||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0
        ||BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void*>(data)),static_cast<ULONG>(size),0)<0||BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)
        throw std::runtime_error("SHA-256 failed.");
    return digest;
}
inline std::string Fingerprint(const Bytes& key){const auto digest=Sha256(key.data(),key.size());return "sha256:"+Hex(digest.data(),digest.size());}
// The fixed SPKI header for id-ecPublicKey + prime256v1, followed by an uncompressed point (04 || X || Y).
inline std::optional<std::array<uint8_t,64>> P256Point(const Bytes& key){
    static constexpr uint8_t header[]={0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07,0x03,0x42,0x00,0x04};
    if(key.size()!=sizeof(header)+64||!std::equal(std::begin(header),std::end(header),key.begin()))return std::nullopt;
    std::array<uint8_t,64> point{};std::copy(key.begin()+sizeof(header),key.end(),point.begin());return point;
}
// DER ECDSA signature to the fixed 64-byte r || s form CNG expects. Rejects non-minimal integers,
// negative values, trailing bytes and values wider than 32 bytes.
inline std::optional<std::array<uint8_t,64>> RawSignature(const Bytes& der){
    if(der.size()<8||der.size()>72||der[0]!=0x30||der[1]!=der.size()-2)return std::nullopt;
    std::array<uint8_t,64> raw{};size_t at=2;
    for(int part=0;part<2;++part){
        if(at+2>der.size()||der[at]!=0x02)return std::nullopt;
        const size_t length=der[at+1];at+=2;
        if(length<1||length>33||at+length>der.size())return std::nullopt;
        const uint8_t* value=der.data()+at;size_t size=length;
        if(value[0]&0x80)return std::nullopt;                       // Negative.
        if(size>1&&value[0]==0&&!(value[1]&0x80))return std::nullopt; // Non-minimal.
        if(value[0]==0&&size>1){++value;--size;}
        if(size>32)return std::nullopt;
        std::copy(value,value+size,raw.begin()+part*32+(32-size));at+=length;
    }
    if(at!=der.size())return std::nullopt;
    return raw;
}
inline bool Verify(const Bytes& key,std::string_view message,const Bytes& signature){
    const auto point=P256Point(key);const auto raw=RawSignature(signature);
    if(!point||!raw||message.size()>ULONG_MAX)return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE handle{};
    struct Cleanup{BCRYPT_ALG_HANDLE& A;BCRYPT_KEY_HANDLE& K;~Cleanup(){if(K)BCryptDestroyKey(K);if(A)BCryptCloseAlgorithmProvider(A,0);}}cleanup{algorithm,handle};
    std::array<uint8_t,sizeof(BCRYPT_ECCKEY_BLOB)+64> blob{};
    const BCRYPT_ECCKEY_BLOB header{BCRYPT_ECDSA_PUBLIC_P256_MAGIC,32};
    std::memcpy(blob.data(),&header,sizeof(header));std::copy(point->begin(),point->end(),blob.begin()+sizeof(header));
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0)<0)return false;
    if(BCryptImportKeyPair(algorithm,nullptr,BCRYPT_ECCPUBLIC_BLOB,&handle,blob.data(),static_cast<ULONG>(blob.size()),0)<0)return false;
    auto digest=Sha256(message.data(),message.size());auto signed_=*raw;
    return BCryptVerifySignature(handle,nullptr,digest.data(),static_cast<ULONG>(digest.size()),signed_.data(),static_cast<ULONG>(signed_.size()),0)==0;
}
inline Verifier DefaultVerifier(){return Verify;}
}
