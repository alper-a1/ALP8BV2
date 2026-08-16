#pragma once

#include <string>
#include <utility>
#include <vector>

#include "token.hpp"

struct ProgramMetadata {
    std::string name;
    std::string description;
    std::string date;
    std::string clock; // stored as string, but is a valid number!
};

std::pair<std::vector<TokenizedLine>, ProgramMetadata> ExtractMetadata(const std::vector<TokenizedLine> &lines);