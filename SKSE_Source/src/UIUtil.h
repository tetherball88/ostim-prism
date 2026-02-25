#pragma once

#include <fstream>
#include <vector>
#include <string>

namespace UIUtil {
    inline std::string base64Encode(const std::vector<uint8_t>& data) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((data.size() + 2) / 3 * 4);

        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t n = static_cast<uint32_t>(data[i]) << 16;
            if (i + 1 < data.size()) n |= static_cast<uint32_t>(data[i + 1]) << 8;
            if (i + 2 < data.size()) n |= static_cast<uint32_t>(data[i + 2]);

            result += chars[(n >> 18) & 0x3F];
            result += chars[(n >> 12) & 0x3F];
            result += (i + 1 < data.size()) ? chars[(n >> 6) & 0x3F] : '=';
            result += (i + 2 < data.size()) ? chars[n & 0x3F] : '=';
        }
        return result;
    }

    inline std::string getIconBase64(const std::string& iconPath) {
        std::string fullPath = "Data\\Interface\\" + iconPath;

        // Replace forward slashes with backslashes for Windows
        for (char& c : fullPath) {
            if (c == '/') c = '\\';
        }

        std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            SKSE::log::warn("Failed to open icon file: {}", fullPath);
            return "";
        }

        std::streamsize size = file.tellg();
        if (size <= 0 || size > 10 * 1024 * 1024) { // Max 10MB
            SKSE::log::warn("Invalid icon file size: {} bytes for {}", size, fullPath);
            return "";
        }
        
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            SKSE::log::warn("Failed to read icon file: {}", fullPath);
            return "";
        }

        return base64Encode(buffer);
    }
}
