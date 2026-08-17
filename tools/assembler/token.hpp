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
    std::string raw;
    TokenType type;

    [[nodiscard]] bool MatchesType(const Token &other) const noexcept { return this->type == other.type; }

    [[nodiscard]] bool MatchesRaw(const Token &other) const noexcept { return this->raw == other.raw; }

    auto operator<=>(const Token &) const = default;
};

struct TokenizedLine {
    std::vector<Token> tokens;
    size_t lineno;
    std::filesystem::path source_file;

    // view into the first token cpp23 goated style
    [[nodiscard]] auto &GetFirstToken(this auto &self) noexcept { return self.tokens.front(); }
};

struct RawLine {
    std::string raw;
    size_t lineno;
    std::filesystem::path source_file;
};