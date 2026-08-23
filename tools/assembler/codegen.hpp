#pragma once

#include "token.hpp"
#include <array>
#include <cstdint>

// return a full compiled 256 program memory
std::array<uint8_t, 256> ConvertToMachineCode(const std::vector<TokenizedLine> &lines);