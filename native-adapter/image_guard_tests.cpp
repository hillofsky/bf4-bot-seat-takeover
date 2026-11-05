#include "server_image_guard.hpp"
#include <iostream>
#include <windows.h>
int wmain() {
    bf4_takeover::CurrentServerImageGuard prepared;
    if (prepared.initialized() || prepared.validate()) {
        std::cerr << "FAIL: uninitialized prepared guard accepted process\n"; return 1;
    }
    if (prepared.initialize().supported || prepared.initialized() || prepared.validate()) {
        std::cerr << "FAIL: prepared guard accepted helper main module\n"; return 1;
    }
    prepared.reset();
    if (prepared.validate()) { std::cerr << "FAIL: reset prepared guard accepted process\n"; return 1; }
    const auto server_path = []() { static wchar_t value[32768]{}; return GetEnvironmentVariableW(L"BF4_SERVER_EXE", value, 32768) < 32768 ? value : nullptr; }();
    if (server_path && *server_path) {
        const auto disk=bf4_takeover::inspect_server_image(server_path);
        if (!disk.supported || disk.sha256 !=
            "160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba") {
            std::cerr << "FAIL: supplied server disk image: " << disk.reason << '\n'; return 1;
        }
        std::cout << "PASS: supplied BF4 server disk image accepted\n";
    } else {
        std::cout << "SKIP: positive disk-image test (set BF4_SERVER_EXE)\n";
    }
    const auto self=bf4_takeover::inspect_current_server_image();
    if (self.supported || self.path.empty() || !self.module_base ||
        bf4_takeover::validate_current_server_image()) {
        std::cerr << "FAIL: helper process was not rejected\n"; return 1;
    }
    if (bf4_takeover::inspect_server_image(L"Z:\\nonexistent\\bf4_Server_Final.exe").supported) {
        std::cerr << "FAIL: missing image accepted\n"; return 1;
    }
    std::cout << "PASS: helper, missing image, uninitialized/reset prepared guard rejected\n";
}
