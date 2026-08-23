#include "codegen.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <format>
#include <string_view>
#include <vector>

#include "asm_error.hpp"
#include "isa.hpp"
#include "token.hpp"

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

constexpr std::array DATA_DIRECTIVES = {std::string_view{"@INI"}, std::string_view{"@UINIT"},
                                        std::string_view{"@BLOCK"}};

// calcualte all the addresses that have a true instruction in them
// also check if all the data directives are valid or not.
std::bitset<256> CalculateLiveAddresses(const std::vector<TokenizedLine> &lines) {
    std::bitset<256> live;

    for (const auto &tl : lines) {
        // ub guard - shouldnt happen
        if (tl.tokens.empty()) {
            continue;
        }

        const auto &first = tl.GetFirstToken();

        switch (first.type) {
        case (TokenType::IDENTIFIER): {
            // this shouldnt be possible in this pass; defensive only
            if (!INSTRS.contains(first.raw)) {
                throw AsmError(tl.lineno, "ASSEMBLERERROR: unknown identifer in codegen pass");
            }

            // either 1 or 2 based on the instruction
            auto size = INSTRS.find(first.raw)->second.GetWordSize();

            // since this is an identifer line it SHOULD have been assigned a pc.
            if (!tl.pc.has_value()) {
                throw AsmError(tl.lineno, "ASSEMBLERERROR: instruction without assigned pc in codegen");
            }

            // fill in the live bitset
            // pc should be in range since that was checked in previous pass
            live.set(*tl.pc);
            if (size == 2) {
                live.set(*tl.pc + 1);
            }

            break;
        }

        case (TokenType::DATA_DIRECTIVE): {
            if (!std::ranges::contains(DATA_DIRECTIVES, first.raw)) {
                throw AsmError(tl.lineno, std::format("Directive {} is not a valid data directive", first.raw),
                               tl.source_file);
            }
            // generation not handled here.
            break;
        }

        default:
            throw AsmError(tl.lineno, "ASSEMBLERERROR: invalid directive in codegen pass");
        }
    }

    return live;
}

uint8_t ExtractInt8FromToken(const Token &tok) {
    assert(tok.type == TokenType::IMMEDIATE && "ExtractInt8FromToken called with non-IMMEDIATE token!");

    std::uint8_t parsed = 0;

    auto tok_raw_sv = static_cast<std::string_view>(tok.raw);

    // for: 0x & digits
    if (tok_raw_sv.starts_with("0x")) {
        tok_raw_sv.remove_prefix(2); // remove prefix

        auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed, 16);

        if (ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) {
            return parsed;
        }

        // should be unreachable.
        throw AsmError(0, "ASMERR: lexer failed to prevent bad immediate");
    }

    // for: 0b & digits
    if (tok_raw_sv.starts_with("0b")) {
        tok_raw_sv.remove_prefix(2); // remove prefix

        auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed, 2);

        if (ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) {
            return parsed;
        }

        // should be unreachable.
        throw AsmError(0, "ASMERR: lexer failed to prevent bad immediate");
    }

    // for: - & digits
    if (tok_raw_sv.starts_with("-")) {
        // no need to remove prefix, from chars uses minus sign to do twos complement for us.

        // ensure we do signed parsing
        auto parsed_signed = static_cast<std::int8_t>(parsed);

        auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed_signed, 10);

        if (ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) {
            return std::bit_cast<uint8_t>(parsed_signed);
        }

        // should be unreachable.
        throw AsmError(0, "ASMERR: lexer failed to prevent bad immediate");
    }

    // assume its no prefix, plain positive number
    auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed, 10);

    if (ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) {
        return parsed;
    }

    // should be unreachable.
    throw AsmError(0, "ASMERR: lexer failed to prevent bad immediate");
}

std::array<uint8_t, 256> ConvertToMachineCode(std::vector<TokenizedLine> lines) {
    std::array<uint8_t, 256> out{};

    std::bitset<256> live = CalculateLiveAddresses(lines);

    // no need to check if first token is invalid at this point
    // also all lines should have PC value (checked in previous pass)
    for (const auto &tl : lines) {
        const auto &first = tl.GetFirstToken();

        switch (first.type) {
        case (TokenType::IDENTIFIER): {

            auto instr_it = INSTRS.find(first.raw);
            const auto &instr = instr_it->second;
            // bounds check for pc in label pass, existence check in live address loop; safe.
            const auto i_addr_begin = static_cast<uint8_t>(*tl.pc);

            // based on the instr shape, compile the instruction
            switch (instr.shape) {
            case (OperandShape::NONE): {
                if (tl.tokens.size() != 1) {
                    throw AsmError(tl.lineno, std::format("mnemonic {} should not have operands", instr_it->first),
                                   tl.source_file);
                }

                // simplest, just write raw opcode to the output
                out.at(i_addr_begin) = instr.encoding;
                break;
            }
            case (OperandShape::IMM8_ONLY): {
                if (tl.tokens.size() != 2 || tl.tokens[1].type != TokenType::IMMEDIATE) {
                    throw AsmError(tl.lineno, std::format("mnemonic {} should should have a single immediate operand",
                                                          instr_it->first));
                }

                uint8_t imm8 = ExtractInt8FromToken(tl.tokens[1]);

                out.at(i_addr_begin) = instr.encoding;
                // since we know that all imm8 only will be two words long
                out.at(i_addr_begin + 1) = imm8;
                break;
            }
            case (OperandShape::SINGLE_REG): {
                if (tl.tokens.size() != 2 || tl.tokens[1].type != TokenType::IDENTIFIER ||
                    !REGISTERS.contains(tl.tokens[1].raw)) {
                    throw AsmError(tl.lineno, std::format("mnemonic {} should should have a single register operand",
                                                          instr_it->first));
                }

                uint8_t regb = REGISTERS.at(tl.tokens[1].raw);

                uint8_t constructed = instr.encoding | regb;

                out.at(i_addr_begin) = constructed;
                break;
            }
            case (OperandShape::DUAL_REG): {
                bool invalid = tl.tokens.size() != 3 || tl.tokens[1].type != TokenType::IDENTIFIER ||
                               !REGISTERS.contains(tl.tokens[1].raw) || tl.tokens[2].type != TokenType::IDENTIFIER ||
                               !REGISTERS.contains(tl.tokens[2].raw);

                if (invalid) {
                    throw AsmError(tl.lineno, std::format("mnemonic {} should should have a two register operands",
                                                          instr_it->first));
                }

                uint8_t rega = REGISTERS.at(tl.tokens[1].raw);
                uint8_t regb = REGISTERS.at(tl.tokens[2].raw);

                uint8_t constructed = instr.encoding;
                constructed |= (rega << 2);
                constructed |= regb;

                out.at(i_addr_begin) = constructed;
                break;
            }
            case (OperandShape::SINGLE_REG_IMM8): {
                bool invalid = tl.tokens.size() != 3 || tl.tokens[1].type != TokenType::IDENTIFIER ||
                               !REGISTERS.contains(tl.tokens[1].raw) || tl.tokens[2].type != TokenType::IMMEDIATE;

                if (invalid) {
                    throw AsmError(
                        tl.lineno,
                        std::format("mnemonic {} should should have a register operand and immediate operand",
                                    instr_it->first));
                }

                uint8_t val = ExtractInt8FromToken(tl.tokens[1]);
                uint8_t regb = REGISTERS.at(tl.tokens[2].raw);

                uint8_t constructed = instr.encoding | regb;

                out.at(i_addr_begin) = constructed;
                // since we know that all imm8 only will be two words long
                out.at(i_addr_begin + 1) = val;
                break;
            }
            case (OperandShape::DUAL_REG_IMM8): {
                bool invalid = tl.tokens.size() != 4 || tl.tokens[1].type != TokenType::IDENTIFIER ||
                               !REGISTERS.contains(tl.tokens[1].raw) || tl.tokens[2].type != TokenType::IDENTIFIER ||
                               !REGISTERS.contains(tl.tokens[2].raw) || tl.tokens[3].type != TokenType::IMMEDIATE;

                if (invalid) {
                    throw AsmError(tl.lineno, std::format("mnemonic {} should should have a two register operands",
                                                          instr_it->first));
                }

                uint8_t rega = REGISTERS.at(tl.tokens[1].raw);
                uint8_t regb = REGISTERS.at(tl.tokens[2].raw);
                uint8_t imm8 = ExtractInt8FromToken(tl.tokens[3]);

                uint8_t constructed = instr.encoding;
                constructed |= (rega << 2);
                constructed |= regb;

                out.at(i_addr_begin) = constructed;
                // since we know that all imm8 only will be two words long
                out.at(i_addr_begin + 1) = imm8;
                break;
            }
            }

            break;
        }

        case (TokenType::DATA_DIRECTIVE): {
        }

        default:
            throw AsmError(tl.lineno, "ASSEMBLERERROR: invalid directive in codegen pass");
            break;
        }
    }

    return out;
}