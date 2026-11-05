#pragma once
#include <cstdint>
#include <array>
#include <string>

namespace bf4_takeover {
struct ServerImageResult {
    bool supported = false;
    std::wstring path;
    std::string sha256;
    std::uintptr_t module_base = 0;
    std::string reason;
};

// Windows-only; reads an exact file and validates its SHA-256 and PE headers.
ServerImageResult inspect_server_image(const std::wstring& path);
// Obtains the current process main module itself. Does not accept a substitute
// module handle or external path. It validates disk identity and loaded headers,
// not all runtime code pages or the ABI of any proposed function.
ServerImageResult inspect_current_server_image();
bool validate_current_server_image();

// Prepare once on a verified gameplay initialization callback. Hashing the disk
// image is deliberately NOT repeated by validate(). Keep this object alive for
// the entire lifetime of any Bindings callback capturing it.
class CurrentServerImageGuard {
public:
    ServerImageResult initialize();
    bool validate() const;
    void reset();
    bool initialized() const { return initialized_; }
    std::uintptr_t module_base() const { return initialized_ ? module_base_ : 0; }
private:
    std::uintptr_t module_base_ = 0;
    std::array<unsigned char,1536> headers_{};
    bool initialized_ = false;
};
}
