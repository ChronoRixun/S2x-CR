#pragma once
#include <string>
namespace utils::cryptography {
 namespace sha256 { std::string compute(const std::string& data, bool hex = false); }
 namespace base64 { std::string encode(const std::string& data); std::string decode(const std::string& data); }
}
