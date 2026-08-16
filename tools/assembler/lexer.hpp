#pragma once

#include "token.hpp"
#include <vector>

// parsing tokens
constexpr char DELIM_TOKEN = ' ';
constexpr char DELIM_COMMENT = ';';

// classifier tokens
constexpr char PREFIX_META = '.';
constexpr char PREFIX_BOOK = '$';
constexpr char PREFIX_DATA = '@';
constexpr char PREFIX_MACRO = '%';
constexpr char SUFFIX_LABEL = ':';

// tokenizes one file
std::vector<TokenizedLine> TokenizeSourceFile(const std::string &source_path);

// tokenizes the source file and concats with the builtin macros
std::vector<TokenizedLine> LoadSourceWithBuiltins(const std::filesystem::path &source_path,
                                                  const std::filesystem::path &builtins_path = "__built-ins.asm");