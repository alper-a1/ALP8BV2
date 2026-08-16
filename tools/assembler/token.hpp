#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

enum class TokenType {
    IDENTIFIER,      // raw text, no prefix - mnemonics, register names, macro names, labels-in-operand-position
    IMMEDIATE,       // 0x0A, 12, 0b01
    LABEL_DEF,       // ends with ': '    (colon space)
    META_DIRECTIVE,  // starts with '.'   (.NAME, .DESC, .DATE, .CLOCK)
    MACRO_DIRECTIVE, // starts with '%'   (%MACRO, %ENDMACRO, %DEFINE, %INCLUDE)
    BOOK_DIRECTIVE,  // starts with '$'   ($PCSET)
    DATA_DIRECTIVE,  // starts with '@'   (@INISAFE, @INIUNSAFE)
    STRING,          // "any thing in quotes,"
};

struct Token {
    TokenType type;
    std::string raw;
};

struct TokenizedLine {
    std::vector<Token> tokens;
    size_t lineno;
    std::filesystem::path source_file;
};

struct RawLine {
    std::string raw;
    size_t lineno;
    std::filesystem::path source_file;
};