#include "metadata_pass.hpp"

#include <cassert>
#include <charconv>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <system_error>
#include <vector>

#include "asm_error.hpp"
#include "token.hpp"

std::string ConcatDirectiveArgs(const TokenizedLine &tl) {
    // create one single space joined string from the 2nd element of the tl vector to the end, joining its raw strings
    return tl.tokens | std::views::drop(1) | std::views::transform(&Token::raw) | std::views::join_with(' ') |
           std::ranges::to<std::string>();
}

std::pair<std::vector<TokenizedLine>, ProgramMetadata> ExtractMetadata(std::vector<TokenizedLine> lines) {
    std::vector<TokenizedLine> cleaned_lines;
    cleaned_lines.reserve(lines.size());

    ProgramMetadata metadata;
    bool name_found = false;
    bool clock_found = false;
    bool desc_found = false;
    bool date_found = false;

    for (const auto &tl : lines) {
        assert(!tl.tokens.empty() && "lexer guarantees non-empty token lists; all passes preserve this");

        if (tl.tokens[0].type == TokenType::META_DIRECTIVE) {
            if (tl.tokens.size() < 2) {
                throw AsmError(tl.lineno, "Meta directive must have at least 1 argument", tl.source_file);
            }

            // directive is the first token
            const std::string &dir = tl.tokens[0].raw;

            // for each directive type check if we have already found it before, if not collect its arguments and add to
            // metadata struct
            if (dir == ".NAME") {
                if (name_found) {
                    throw AsmError(tl.lineno, "Duplicate .NAME directive", tl.source_file);
                }
                name_found = true;
                metadata.name = ConcatDirectiveArgs(tl);
            } else if (dir == ".CLOCK") {
                if (clock_found) {
                    throw AsmError(tl.lineno, "Duplicate .CLOCK directive", tl.source_file);
                }
                clock_found = true;
                // clock directive is different in that it only wants ONE argument
                if (tl.tokens.size() > 2) {
                    throw AsmError(tl.lineno, ".CLOCK directive recieved too many arguments", tl.source_file);
                }
                // turn the second argument of the clock directive into a uint16_t
                std::uint16_t clock_hz = 0;
                std::string_view clock_hz_str = tl.tokens[1].raw;

                const char *first = clock_hz_str.data();
                const char *last = first + clock_hz_str.size();
                auto [ptr, ec] = std::from_chars(first, last, clock_hz);

                // ensure that the whole token is consumed such that stuff like "1000ABC" does not pass
                if (ec == std::errc{} && ptr == last) {
                    metadata.clock = clock_hz;
                } else {
                    throw AsmError(tl.lineno, ".CLOCK directive requires a valid positive integer", tl.source_file);
                }

            } else if (dir == ".DESC") {
                if (desc_found) {
                    throw AsmError(tl.lineno, "Duplicate .DESC directive", tl.source_file);
                }
                desc_found = true;
                metadata.description = ConcatDirectiveArgs(tl);
            } else if (dir == ".DATE") {
                if (date_found) {
                    throw AsmError(tl.lineno, "Duplicate .DATE directive", tl.source_file);
                }
                date_found = true;
                metadata.date = ConcatDirectiveArgs(tl);
            } else {
                throw AsmError(tl.lineno, "Unknown meta directive: " + dir, tl.source_file);
            }
        } else {
            // Keep instructions, macro definitions, and everything else
            cleaned_lines.push_back(tl);
        }
    }

    // validate mandatory metadata
    if (!name_found) {
        // just use the source file of the last line (gaurenteed to be user file, since includes are appeneded to top)
        std::string src = lines.empty() ? "" : lines.back().source_file.string();
        throw AsmError(0, "Missing mandatory .NAME directive", src);
    }

    if (!clock_found) {
        std::string src = lines.empty() ? "" : lines.back().source_file.string();
        throw AsmError(0, "Missing mandatory .CLOCK directive", src);
    }

    return {std::move(cleaned_lines), metadata};
}