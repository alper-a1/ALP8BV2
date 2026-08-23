#include "lexer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "asm_error.hpp"
#include "token.hpp"

// convert file to raw line array
std::vector<RawLine> SourceToRawLine(const std::filesystem::path &source_path) {
    std::vector<RawLine> source_lines;
    source_lines.reserve(128); // reserve space like a boss

    // open and check that the file has opened
    // no need to bother catching this as its a harsh error
    std::ifstream file(source_path);
    if (!file.is_open()) {
        throw AsmError(0, "Failed to open file.", source_path);
    }

    // loop through all the lines and put them into the output
    std::string curr_line;
    // one based indexing
    size_t line_counter = 1;
    while (std::getline(file, curr_line)) {
        source_lines.push_back(RawLine{.raw = curr_line, .lineno = line_counter, .source_file = source_path});

        line_counter++;
    }

    return source_lines;
}

// helper function to remove all whitespace from the ends of a string (line)
void TrimWhitespace(std::string &s) {
    constexpr std::string_view WS = " \t\r\n\f\v";
    const auto start = s.find_first_not_of(WS);
    if (start == std::string::npos) {
        s.clear(); // Entirely whitespace
        return;
    }
    const auto end = s.find_last_not_of(WS);
    s = s.substr(start, end - start + 1);
}

// attempt to parse as int and if it fails we know we have a bad immediate
/*
┌────────────────────┬───────────┬─────────────┐
│ Pattern            │ Example   │ → Immediate │
├────────────────────┼───────────┼─────────────┤
│ 0x + hex digits    │ 0xFF, 0x0 │ ✅          │
├────────────────────┼───────────┼─────────────┤
│ 0b + binary digits │ 0b1010    │ ✅          │
├────────────────────┼───────────┼─────────────┤
│ - + digits         │ -128      │ ✅          │
├────────────────────┼───────────┼─────────────┤
│ digits             │ 0, 255    │ ✅          │
└────────────────────┴───────────┴─────────────┘
*/
bool IsValidImmediate(std::string_view rtok) {
    int base = 10;
    bool signedv = false;

    if (rtok.starts_with("0x")) {
        base = 16;
        rtok.remove_prefix(2);
    } else if (rtok.starts_with("0b")) {
        base = 2;
        rtok.remove_prefix(2);
    } else if (rtok.starts_with('-')) {
        signedv = true;
    }

    if (signedv) {
        std::int8_t parsed = 0;
        auto [ptr, ec] = std::from_chars(rtok.data(), rtok.data() + rtok.size(), parsed, base);

        if (ec == std::errc{} && ptr == rtok.data() + rtok.size() && parsed < INT8_MAX && parsed > INT8_MIN) {
            return true;
        }

    } else {
        std::uint8_t parsed = 0;

        auto [ptr, ec] = std::from_chars(rtok.data(), rtok.data() + rtok.size(), parsed, base);

        if (ec == std::errc{} && ptr == rtok.data() + rtok.size()) {
            return true;
        }
    }

    return false;
}

// remove comments, empty lines etc
void CleanSourceRawLines(std::vector<RawLine> &source_lines) {
    // remove all the comments from lines
    // ANYTHING after a ';' is a comment
    for (auto &raw_line : source_lines) {
        // find the position of the first ';' char, if exists, remove everything >= that pos
        // since its a comment
        if (size_t pos = raw_line.raw.find(DELIM_COMMENT); pos != std::string::npos) {
            raw_line.raw.erase(pos);
        }

        // replace tabs with spaces for easier parsing downstream
        std::ranges::replace(raw_line.raw, '\t', ' ');

        // remove all whitespace (post comment deletion)
        TrimWhitespace(raw_line.raw);
    }

    // remove all empty lines
    std::erase_if(source_lines, [](const auto &l) {
        // returns true if the line is empty or is entirely spaces/tabs
        return l.raw.empty();
    });
}

// turn a raw line of a string (+lineno/src metadata) into its tokenized form
TokenizedLine TokenizeRawLine(const RawLine &in) {
    TokenizedLine out{.tokens = {}, .lineno = in.lineno, .source_file = in.source_file};

    // prepare the view to iterate over
    // split on DELIM_TOKEN (space), ignoring tokens that are empty (e.g. "ADD  R2      R3" will ignore spaces)
    auto token_views = std::views::split(in.raw, DELIM_TOKEN) |
                       std::views::filter([](auto &&r) { return !r.empty(); }) | std::views::enumerate;

    // iterate through all raw space seperated tokens in the line, classifying them as we go
    for (const auto &[index, raw_token] : token_views) {
        std::string_view token_sv(raw_token);
        // shouldnt happen but sanity check (since cleaned beforehand)
        if (token_sv.empty()) {
            continue;
        }

        // assume its an identifier
        TokenType type{};

        // only check index zero for types that are not identifier (they cannot be anywehre else)
        if (index == 0) {
            if (token_sv.starts_with(PREFIX_META)) {
                type = TokenType::META_DIRECTIVE;
            } else if (token_sv.starts_with(PREFIX_MACRO)) {
                type = TokenType::MACRO_DIRECTIVE;
            } else if (token_sv.starts_with(PREFIX_BOOK)) {
                type = TokenType::BOOK_DIRECTIVE;
            } else if (token_sv.starts_with(PREFIX_DATA)) {
                type = TokenType::DATA_DIRECTIVE;
            } else if (token_sv.ends_with(SUFFIX_LABEL)) {
                type = TokenType::LABEL_DEF;
            } else {
                type = TokenType::IDENTIFIER;
            }

            // ensure that there are no empty directive lines or labels
            if (std::ranges::contains(std::initializer_list<TokenType>{TokenType::META_DIRECTIVE,
                                                                       TokenType::MACRO_DIRECTIVE,
                                                                       TokenType::BOOK_DIRECTIVE,
                                                                       TokenType::DATA_DIRECTIVE, TokenType::LABEL_DEF},
                                      type) &&
                token_sv.size() == 1) {
                throw AsmError(out.lineno, "Empty directive / label - name required", out.source_file);
            }

        } else {
            // for all other non zero indexed tokens, they can either be an identifer or immediate
            type = TokenType::IDENTIFIER;

            // check if it could be a valid immediate
            constexpr std::array imm8_candidate = {'-', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
            for (const auto &c : imm8_candidate) {
                if (token_sv.starts_with(c) && IsValidImmediate(token_sv)) {
                    type = TokenType::IMMEDIATE;
                }
            }
        }

        out.tokens.push_back(Token{.raw = std::ranges::to<std::string>(raw_token), .type = type});
    }

    return out;
}

// tokenize one path - no expansion or anything else.
std::vector<TokenizedLine> TokenizeSourceFile(const std::filesystem::path &source_path) {
    // read and clean the lines, populate with linecount
    std::vector<RawLine> source_lines = SourceToRawLine(source_path);
    CleanSourceRawLines(source_lines);

    // tokenize each line one by one
    std::vector<TokenizedLine> tokenized_lines;
    tokenized_lines.reserve(source_lines.size());

    for (const auto &raw_line : source_lines) {
        tokenized_lines.push_back(TokenizeRawLine(raw_line));
    }

    return tokenized_lines;
}

std::vector<TokenizedLine> LoadSourceWithBuiltins(const std::filesystem::path &source_path,
                                                  const std::filesystem::path &builtins_path) {
    std::vector<TokenizedLine> full_stream;

    // Look for built-ins next to the assembler binary
    // NOTE: will only work on linux, for now fine - but in future  resolving the path of builtins should probably be
    // fixed along with adding %include
    std::filesystem::path builtins_full_path =
        std::filesystem::canonical("/proc/self/exe").parent_path() / builtins_path;

    if (std::filesystem::exists(builtins_full_path)) {
        auto builtins = TokenizeSourceFile(builtins_full_path);
        full_stream.insert(full_stream.end(), std::make_move_iterator(builtins.begin()),
                           std::make_move_iterator(builtins.end()));
    } else {
        throw AsmError(0, "Built-in macros could not be loaded.", "ASSEMBLER ERROR");
    }

    // load the user's target source file
    // insert it to the end of the current stream (which will have builtins)
    // since each file is tokenized individually, linenos and source files will stay the same
    auto user_lines = TokenizeSourceFile(source_path);
    full_stream.insert(full_stream.end(), std::make_move_iterator(user_lines.begin()),
                       std::make_move_iterator(user_lines.end()));

    return full_stream;
}