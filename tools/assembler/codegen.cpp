#include "codegen.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <vector>

#include "asm_error.hpp"
#include "isa.hpp"
#include "token.hpp"

constexpr std::array DATA_DIRECTIVES = {std::string_view{"@INI"}, std::string_view{"@UINI"},
                                        std::string_view{"@BLOCK"}};

// calcualte all the addresses that have a true instruction in them
// also check if all the data directives are valid or not.
std::bitset<256> CalculateLiveAddresses(const std::vector<TokenizedLine> &lines) {
    std::bitset<256> live;

    for (const auto &tl : lines) {
        assert(!tl.tokens.empty() && "lexer guarantees non-empty token lists; all passes preserve this");

        const auto &first = tl.GetFirstToken();

        switch (first.type) {
        case (TokenType::IDENTIFIER): {
            assert(INSTRS.contains(first.raw) && "label pass guarantees every identifier is a valid mnemonic");

            // either 1 or 2 based on the instruction
            auto size = INSTRS.find(first.raw)->second.GetWordSize();

            assert(tl.pc.has_value() && "label pass assigns a pc to every instruction line");

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
            assert(false && "only identifiers and data directives reach codegen");
            break;
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

        // the lexer validated every immediate; a failed parse is an assembler bug
        assert((ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) &&
               "lexer failed to prevent bad immediate (0x)");
        return parsed;
    }

    // for: 0b & digits
    if (tok_raw_sv.starts_with("0b")) {
        tok_raw_sv.remove_prefix(2); // remove prefix

        auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed, 2);

        assert((ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) &&
               "lexer failed to prevent bad immediate (0b)");
        return parsed;
    }

    // for: - & digits
    if (tok_raw_sv.starts_with("-")) {
        // no need to remove prefix, from chars uses minus sign to do twos complement for us.

        // ensure we do signed parsing
        auto parsed_signed = static_cast<std::int8_t>(parsed);

        auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed_signed, 10);

        assert((ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) &&
               "lexer failed to prevent bad immediate (signed)");
        return std::bit_cast<uint8_t>(parsed_signed);
    }

    // assume its no prefix, plain positive number
    auto [ptr, ec] = std::from_chars(tok_raw_sv.data(), tok_raw_sv.data() + tok_raw_sv.size(), parsed, 10);

    assert((ec == std::errc{} && ptr == tok_raw_sv.data() + tok_raw_sv.size()) &&
           "lexer failed to prevent bad immediate (plain)");
    return parsed;
}

std::array<uint8_t, 256> ConvertToMachineCode(const std::vector<TokenizedLine> &lines) {
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
                    throw AsmError(tl.lineno,
                                   std::format("mnemonic {} should have a single immediate operand", instr_it->first),
                                   tl.source_file);
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
                    throw AsmError(tl.lineno,
                                   std::format("mnemonic {} should have a single register operand", instr_it->first),
                                   tl.source_file);
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
                    throw AsmError(tl.lineno,
                                   std::format("mnemonic {} should have a two register operands", instr_it->first),
                                   tl.source_file);
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
                    throw AsmError(tl.lineno,
                                   std::format("mnemonic {} should have a register operand and immediate operand",
                                               instr_it->first),
                                   tl.source_file);
                }

                uint8_t regb = REGISTERS.at(tl.tokens[1].raw);
                uint8_t val = ExtractInt8FromToken(tl.tokens[2]);

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
                    throw AsmError(tl.lineno,
                                   std::format("mnemonic {} should have a two register operands and an imm8 operand",
                                               instr_it->first),
                                   tl.source_file);
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
            if (first.raw == "@INI") {
                bool invalid = tl.tokens.size() != 3 || tl.tokens[1].type != TokenType::IMMEDIATE ||
                               tl.tokens[2].type != TokenType::IMMEDIATE;

                if (invalid) {
                    throw AsmError(tl.lineno, "@INI directive should have two immediate operands ([addr] [val])",
                                   tl.source_file);
                }

                uint8_t addr = ExtractInt8FromToken(tl.tokens[1]);
                uint8_t val = ExtractInt8FromToken(tl.tokens[2]);

                if (live.test(addr)) {
                    throw AsmError(tl.lineno,
                                   "@INI directive tried to overwrite program memory. if this was indended, use @UINI",
                                   tl.source_file);
                }

                out.at(addr) = val;
            } else if (first.raw == "@UINI") {
                bool invalid = tl.tokens.size() != 3 || tl.tokens[1].type != TokenType::IMMEDIATE ||
                               tl.tokens[2].type != TokenType::IMMEDIATE;

                if (invalid) {
                    throw AsmError(tl.lineno, "@UINI directive should have two immediate operands ([addr] [val])",
                                   tl.source_file);
                }

                uint8_t addr = ExtractInt8FromToken(tl.tokens[1]);
                uint8_t val = ExtractInt8FromToken(tl.tokens[2]);
                // no check, just overwrite
                out.at(addr) = val;
            } else if (first.raw == "@BLOCK") {
                bool invalid = tl.tokens.size() != 4 || tl.tokens[1].type != TokenType::IMMEDIATE ||
                               tl.tokens[2].type != TokenType::IMMEDIATE || tl.tokens[3].type != TokenType::IMMEDIATE;

                if (invalid) {
                    throw AsmError(
                        tl.lineno,
                        "@BLOCK directive should have a three immediate operands ([addr start] [addr end] [val])",
                        tl.source_file);
                }

                uint8_t addr_start = ExtractInt8FromToken(tl.tokens[1]);
                uint8_t addr_end = ExtractInt8FromToken(tl.tokens[2]);
                uint8_t val = ExtractInt8FromToken(tl.tokens[3]);

                if (addr_start > addr_end) {
                    throw AsmError(
                        tl.lineno,
                        "@BLOCK directive needs to have start address < end address ([addr start] [addr end] [val])",
                        tl.source_file);
                }

                for (size_t i = addr_start; i <= addr_end; i++) {
                    if (live.test(i)) {
                        throw AsmError(tl.lineno,
                                       std::format("@BLOCK tried to overwrite program mem (occured at addr {})", i),
                                       tl.source_file);
                    }

                    out.at(i) = val;
                }
            }
            break;
        }

        default:
            assert(false && "only identifiers and data directives reach codegen");
            break;
        }
    }

    return out;
}