#pragma once

#include "metadata_pass.hpp"
#include <array>
#include <cstdint>
#include <filesystem>

void WriteBinary(const std::array<std::uint8_t, 256> &data, const ProgramMetadata &meta,
                 const std::filesystem::path &location);