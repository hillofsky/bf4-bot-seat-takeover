#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "server_image_guard.hpp"
#include <array>
#include <cstring>
#include <vector>

namespace bf4_takeover {
namespace {
constexpr char supported_sha[] =
    "160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba";
constexpr std::size_t header_bytes = 1536;
struct File {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~File() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
struct Algorithm {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~Algorithm() { if (h) BCryptCloseAlgorithmProvider(h,0); }
};
struct Hash {
    BCRYPT_HASH_HANDLE h = nullptr;
    ~Hash() { if (h) BCryptDestroyHash(h); }
};
bool supported_headers(const unsigned char* bytes, std::size_t size) {
    IMAGE_DOS_HEADER dos{};
    if (size < sizeof(dos)) return false;
    std::memcpy(&dos,bytes,sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew != 368) return false;
    IMAGE_NT_HEADERS64 pe{};
    const auto offset = static_cast<std::size_t>(dos.e_lfanew);
    if (offset > size || sizeof(pe) > size-offset) return false;
    std::memcpy(&pe,bytes+offset,sizeof(pe));
    const auto& file = pe.FileHeader;
    const auto& opt = pe.OptionalHeader;
    return pe.Signature == IMAGE_NT_SIGNATURE &&
        file.Machine == IMAGE_FILE_MACHINE_AMD64 && file.NumberOfSections == 12 &&
        file.TimeDateStamp == 1525918474 && file.SizeOfOptionalHeader == sizeof(opt) &&
        opt.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
        opt.AddressOfEntryPoint == 19640680 && opt.SizeOfImage == 51347456 &&
        opt.SizeOfHeaders == header_bytes && opt.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI &&
        opt.MajorLinkerVersion == 11 && opt.MinorLinkerVersion == 0 &&
        opt.MajorOperatingSystemVersion == 6 && opt.MinorOperatingSystemVersion == 0 &&
        opt.MajorImageVersion == 0 && opt.MinorImageVersion == 0 &&
        opt.MajorSubsystemVersion == 6 && opt.MinorSubsystemVersion == 0;
}
}

ServerImageResult inspect_server_image(const std::wstring& path) {
    ServerImageResult r; r.path = path;
    File file;
    // Deny writes/deletes while hashing. A preexisting writer makes this fail.
    file.h = CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
                         OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file.h == INVALID_HANDLE_VALUE) { r.reason="cannot open image read-only"; return r; }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.h,&size) || size.QuadPart != 43610624) {
        r.reason="unsupported image size"; return r;
    }
    std::array<unsigned char,header_bytes> headers{};
    DWORD count = 0;
    if (!ReadFile(file.h,headers.data(),static_cast<DWORD>(headers.size()),&count,nullptr) ||
        count != headers.size() || !supported_headers(headers.data(),headers.size())) {
        r.reason="unsupported PE machine/header version fingerprint"; return r;
    }
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(file.h,zero,nullptr,FILE_BEGIN)) { r.reason="cannot rewind image"; return r; }
    Algorithm algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.h,BCRYPT_SHA256_ALGORITHM,nullptr,0) < 0) {
        r.reason="SHA-256 provider unavailable"; return r;
    }
    DWORD object_length = 0, written = 0;
    if (BCryptGetProperty(algorithm.h,BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_length),sizeof(object_length),&written,0) < 0 ||
        written != sizeof(object_length) || object_length == 0) {
        r.reason="SHA-256 property unavailable"; return r;
    }
    std::vector<unsigned char> hash_object(object_length);
    Hash hash;
    if (BCryptCreateHash(algorithm.h,&hash.h,hash_object.data(),object_length,nullptr,0,0) < 0) {
        r.reason="cannot create SHA-256 hash"; return r;
    }
    std::array<unsigned char,65536> buffer{};
    for (;;) {
        if (!ReadFile(file.h,buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)) {
            r.reason="image read failed"; return r;
        }
        if (!count) break;
        if (BCryptHashData(hash.h,buffer.data(),count,0) < 0) {
            r.reason="image hash update failed"; return r;
        }
    }
    std::array<unsigned char,32> digest{};
    if (BCryptFinishHash(hash.h,digest.data(),static_cast<ULONG>(digest.size()),0) < 0) {
        r.reason="image hash finish failed"; return r;
    }
    static constexpr char hex[]="0123456789abcdef";
    for (const auto value : digest) { r.sha256 += hex[value>>4]; r.sha256 += hex[value&15]; }
    r.supported = r.sha256 == supported_sha;
    r.reason = r.supported ? "supported disk image" : "SHA-256 mismatch";
    return r;
}

ServerImageResult inspect_current_server_image() {
    const auto module = GetModuleHandleW(nullptr);
    ServerImageResult r;
    if (!module) { r.reason="main module unavailable"; return r; }
    r.module_base = reinterpret_cast<std::uintptr_t>(module);
    std::array<wchar_t,32768> path{};
    const DWORD length = GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) { r.reason="main module path unavailable"; return r; }
    r = inspect_server_image(std::wstring(path.data(),length));
    r.module_base = reinterpret_cast<std::uintptr_t>(module);
    if (!r.supported) return r;
    std::array<unsigned char,header_bytes> headers{};
    SIZE_T count = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),module,headers.data(),headers.size(),&count) ||
        count != headers.size() || !supported_headers(headers.data(),headers.size())) {
        r.supported=false; r.reason="loaded main module header fingerprint mismatch"; return r;
    }
    r.reason="supported current server disk image and loaded headers";
    return r;
}
bool validate_current_server_image() { return inspect_current_server_image().supported; }

void CurrentServerImageGuard::reset() {
    initialized_ = false; module_base_ = 0; headers_.fill(0);
}
ServerImageResult CurrentServerImageGuard::initialize() {
    reset();
    auto result = inspect_current_server_image(); // Disk hash only at initialization.
    if (!result.supported) return result;
    SIZE_T count = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(result.module_base),
        headers_.data(),headers_.size(),&count) || count != headers_.size() ||
        !supported_headers(headers_.data(),headers_.size())) {
        reset(); result.supported=false; result.reason="cannot retain loaded header fingerprint";
        return result;
    }
    module_base_ = result.module_base;
    initialized_ = true;
    return result;
}
bool CurrentServerImageGuard::validate() const {
    if (!initialized_ || !module_base_ ||
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != module_base_) return false;
    std::array<unsigned char,header_bytes> current{};
    SIZE_T count = 0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(module_base_),
        current.data(),current.size(),&count) && count == current.size() && current == headers_;
}
}
