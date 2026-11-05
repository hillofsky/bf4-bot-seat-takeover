#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bf4_deploy_client {
class ClientImage {
    std::uintptr_t base_=0;
    std::array<unsigned char,1536> headers_{};
public:
    std::uintptr_t base() const noexcept {return base_;}
    bool initialize() {
        base_=0;
        const auto module=GetModuleHandleW(nullptr);
        std::array<wchar_t,32768> path{};
        const DWORD length=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
        if(!length || length>=path.size()) return false;
        HANDLE file=CreateFileW(path.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) return false;
        struct Close {HANDLE h;~Close(){CloseHandle(h);}} close{file};
        LARGE_INTEGER size{};DWORD count=0;
        if(!GetFileSizeEx(file,&size) || size.QuadPart!=40419608 ||
            !ReadFile(file,headers_.data(),static_cast<DWORD>(headers_.size()),&count,nullptr) || count!=headers_.size()) return false;
        IMAGE_DOS_HEADER dos{};std::memcpy(&dos,headers_.data(),sizeof(dos));
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 ||
            static_cast<std::size_t>(dos.e_lfanew)>headers_.size()-sizeof(IMAGE_NT_HEADERS64)) return false;
        IMAGE_NT_HEADERS64 nt{};std::memcpy(&nt,headers_.data()+dos.e_lfanew,sizeof(nt));
        if(nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage!=47476736) return false;
        BCRYPT_ALG_HANDLE algorithm=nullptr;
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return false;
        struct CloseAlgorithm {BCRYPT_ALG_HANDLE h;~CloseAlgorithm(){BCryptCloseAlgorithmProvider(h,0);}} close_algorithm{algorithm};
        DWORD object_size=0,written=0;
        if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&written,0)<0 ||
            written!=sizeof(object_size) || !object_size) return false;
        std::vector<unsigned char> object(object_size);
        BCRYPT_HASH_HANDLE hash=nullptr;
        if(BCryptCreateHash(algorithm,&hash,object.data(),object_size,nullptr,0,0)<0) return false;
        struct CloseHash {BCRYPT_HASH_HANDLE h;~CloseHash(){BCryptDestroyHash(h);}} close_hash{hash};
        LARGE_INTEGER zero{};
        if(!SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)) return false;
        std::array<unsigned char,65536> buffer{};
        for(;;) {
            if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)) return false;
            if(!count) break;
            if(BCryptHashData(hash,buffer.data(),count,0)<0) return false;
        }
        std::array<unsigned char,32> digest{};
        if(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0) return false;
        constexpr std::array<unsigned char,32> expected={0x0f,0x3b,0x4c,0xa5,0xe4,0xb9,0x64,0x24,0x13,0xb8,0xdf,0x5d,0x86,0x9d,0x94,0x33,0x7a,0x8b,0xb6,0x90,0x23,0x8a,0xe2,0xb2,0xe5,0xa7,0x80,0xa6,0x89,0x42,0xae,0xb4};
        if(digest!=expected) return false;
        std::array<unsigned char,1536> loaded{};SIZE_T read=0;
        if(!ReadProcessMemory(GetCurrentProcess(),module,loaded.data(),loaded.size(),&read) || read!=loaded.size() || loaded!=headers_) return false;
        base_=reinterpret_cast<std::uintptr_t>(module);return true;
    }
    bool validate() const noexcept {
        if(!base_ || reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))!=base_) return false;
        std::array<unsigned char,1536> loaded{};SIZE_T count=0;
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base_),loaded.data(),loaded.size(),&count) &&
            count==loaded.size() && loaded==headers_;
    }
};
}
