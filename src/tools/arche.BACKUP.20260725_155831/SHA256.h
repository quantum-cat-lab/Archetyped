#pragma once
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

std::string sha256_hex(const std::string& data);
std::string sha256_file(const fs::path& p);
