/**
   if live_addresses.contains(addr): throw  // overlaps real instruction
   else: write byte
 ```

 A 2-byte instruction at PC 0x05 marks 0x05 and 0x06 as live. @INISAFE 0x07 value is fine even
 though 0x07 < program_end — it's in a gap.

 This is precise, cheap (set lookup), and handles arbitrary jump patterns. The label pass stays
 clean — just line → pc mapping. Codegen owns all @ logic including the safety check.

 Does this match your mental model?


 lgtm. this also means that if I in future add something like @FILL , I can just check it with the
 live addresses.

 would live address be a 256 long bool vector on what is occupied with real instructions and what
 isn't?

 Exactly. 256 addresses max — a std::bitset<256> is perfect:

 ```cpp
   std::bitset<256> live;

   // marking
   live.set(line.pc);
   live.set(line.pc + 1); // for 2-byte instructions

   // check
   if (live.test(addr))
       throw; // @INISAFE collision
 ```

 No hashing, no allocation, compile-time size. vector<bool> works too but bitset is the right tool
 for a fixed known size.

─────────────────────────────────────────────────────────────────────────────────────────────────────

*/

#include "label_pass.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "asm_error.hpp"
#include "isa.hpp"
#include "token.hpp"

std::pair<std::vector<TokenizedLine>, std::map<std::string, size_t>>
GenerateSymbolMap(std::vector<TokenizedLine> lines) {
    std::vector<TokenizedLine> parsed; // with no label defs or '$' directives.
    parsed.reserve(lines.size());
    std::map<std::string, size_t> symbol_map;

    size_t program_counter = 0;

    // at this stage there is only data/book directives, and also label defs
    for (auto &tl : lines) {
        auto first = tl.GetFirstToken();

        switch (first.type) {
        case TokenType::DATA_DIRECTIVE: {
            // data directives must pass through to next stage (no pc required for them)
            parsed.push_back(std::move(tl));
            break;
        }
        case TokenType::BOOK_DIRECTIVE: {
            if (first.raw != "$PCSET") {
                throw AsmError(tl.lineno, std::format("Unknown '$' directive: {}", first.raw), tl.source_file);
            }

            if (tl.tokens.size() != 2) {
                throw AsmError(tl.lineno, "$PCSET directive requries an [addr] argument", tl.source_file);
            }

            // parse the book directives raw value, MUST be hex literal 0x...
            std::string_view addr_lit = tl.tokens[1].raw;
            if (addr_lit.size() < 3) {
                throw AsmError(tl.lineno, "invalid literal to $PCSET directive: requires 0x{int8}", tl.source_file);
            }

            // remove the '0x' literal marker
            addr_lit.remove_prefix(2);

            unsigned int addr = 0;
            auto [ptr, ec] = std::from_chars(addr_lit.data(), addr_lit.data() + addr_lit.size(), addr, 16);
            if (ec == std::errc{} && ptr == addr_lit.data() + addr_lit.size()) {
                if (addr > UINT8_MAX) {
                    throw AsmError(tl.lineno, std::format("$PCSET contains invalid address: {:#X}", addr),
                                   tl.source_file);
                }

                // addr is valided to be a sane PC number, set it.
                program_counter = addr;

            } else {
                throw AsmError(
                    tl.lineno,
                    std::format("Failed to parse $PCSET argument: {} (must be hex literal e.g. 0xA1)", addr_lit),
                    tl.source_file);
            }
            // directive does not get added to parsed lines
            break;
        }
        case TokenType::IDENTIFIER: {
            // at this pass, all identifers must be a valid instruction
            if (!INSTRS.contains(first.raw)) {
                throw AsmError(tl.lineno, std::format("invalid mnemonic: {}", first.raw), tl.source_file);
            }

            // attach the current pc to this line and add it to parsed
            tl.pc = program_counter;

            // increase the pc by the word size
            program_counter += INSTRS.find(first.raw)->second.GetWordSize();

            // add the line with attached pc
            parsed.push_back(std::move(tl));
            break;
        }
        case TokenType::LABEL_DEF: {
            // strip the ':' and assign the current pc to the label, add it to the map
            std::string_view raw_label_name = first.raw;
            raw_label_name.remove_suffix(1);

            if (std::ranges::contains(BUILTINS, raw_label_name)) {
                throw AsmError(tl.lineno, "labels cannot share names with builtin mnemonics", tl.source_file);
            }

            if (symbol_map.contains(static_cast<std::string>(raw_label_name))) {
                throw AsmError(tl.lineno, std::format("Label {} already defined", raw_label_name), tl.source_file);
            }

            symbol_map[static_cast<std::string>(raw_label_name)] = program_counter;

            // dont emit labels into the next stage
            break;
        }

        default: {
            // should not happen, but just in case
            throw AsmError(tl.lineno, "ASMERROR: FORBIDDEN DIRECTIVE IN LABEL PASS", tl.source_file);
        }
        }

        if (program_counter > UINT8_MAX) {
            throw AsmError(tl.lineno, "Program counter ran out-of-bounds (>255)", tl.source_file);
        }
    }

    return {parsed, symbol_map};
}