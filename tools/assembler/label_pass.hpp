#pragma once

#include "token.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

std::vector<TokenizedLine> ResolveAndMapLabels(std::vector<TokenizedLine> lines);