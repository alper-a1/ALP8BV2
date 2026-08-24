#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "token.hpp"

// implicit 32 char limit for name/date
// 128 char limit for description (same size as the program data binary XD)
struct ProgramMetadata {
    std::string name;
    std::string description;
    std::string date;
    std::uint16_t clock; // potentially too high ?
};

std::pair<std::vector<TokenizedLine>, ProgramMetadata> ExtractMetadata(std::vector<TokenizedLine> lines);