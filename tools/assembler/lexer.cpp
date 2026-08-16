#include "lexer.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
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

// helper function to remove all whitespace from a string (line)
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
        TokenType type;

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
        } else {
            // for all other non zero indexed tokens, they can either be an identifer, immediate or string>=2 (in
            // quotes)
            if (token_sv.size() >= 2 && token_sv.starts_with('"') && token_sv.ends_with('"')) {
                type = TokenType::STRING;
            } else if (std::isdigit(static_cast<unsigned char>(token_sv.front())) ||
                       (token_sv.size() > 1 && (token_sv.front() == '-' || token_sv.front() == '+') &&
                        std::isdigit(static_cast<unsigned char>(token_sv[1])))) {
                // either start with a digit , or start with a '+' pr '-' then a digit
                type = TokenType::IMMEDIATE;
            } else {
                type = TokenType::IDENTIFIER;
            }
        }

        out.tokens.push_back(Token{.type = type, .raw = std::ranges::to<std::string>(raw_token)});
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