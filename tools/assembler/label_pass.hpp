#pragma once

#include "token.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

std::pair<std::vector<TokenizedLine>, std::map<std::string, size_t>>
GenerateSymbolMap(std::vector<TokenizedLine> lines);