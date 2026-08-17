#include "macro_pass.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <ranges>
#include <set>
#include <vector>

#include "asm_error.hpp"
#include "isa.hpp"
#include "token.hpp"

// public interface definition
std::vector<TokenizedLine> ResolveMacroAndDefine(std::vector<TokenizedLine> lines) {
    MacroEngine engine(std::move(lines));
    return engine.Run();
}

MacroEngine::MacroEngine(std::vector<TokenizedLine> lines) : lines(std::move(lines)) {}

std::vector<TokenizedLine> MacroEngine::Run() {
    // run both passes and transfer lines back to caller
    this->ResolveDefines();
    this->ExpandMacros();
    return std::move(this->lines);
}

// go through the source line by line, when we hit a define def, create a define.
// when going through source, check if the identifiers we are on matches a define, if so, replace and add to 'parsed'
void MacroEngine::ResolveDefines() {
    std::vector<TokenizedLine> parsed;
    parsed.reserve(this->lines.size()); // probably a similar number after parsing, reserve it.

    for (auto &tl : this->lines) {
        // shouldnt happen since we sanizied for this in lexer , but just incase
        if (tl.tokens.empty()) {
            continue;
        }
        Token &first = tl.GetFirstToken();

        // this line is not an identifer, cannot be define-replaced
        // check if it is a define defenition itself, else add to parsed lines and go next
        if (first.type == TokenType::MACRO_DIRECTIVE && first.raw == "%DEFINE") {
            // ensure the define has a value and value is of iden/imm
            const bool valid_syntax =
                tl.tokens.size() == 3 && tl.tokens[1].type == TokenType::IDENTIFIER &&
                (tl.tokens[2].type == TokenType::IMMEDIATE || tl.tokens[2].type == TokenType::IDENTIFIER);

            if (!valid_syntax) {
                throw AsmError(tl.lineno, "Malformed %DEFINE. Expected: %DEFINE <identifier> <value|identifier>",
                               tl.source_file);
            }

            // valid; consume and add to define map
            const std::string &key = tl.tokens[1].raw;

            // defines cannot overwrite
            if (INSTRS.contains(key)) {
                throw AsmError(tl.lineno, std::format("%DEFINE cannot overwrite built-in mnemonic '{}'", key),
                               tl.source_file);
            }

            // if the define's value is a previous define, get its value immediately and set it
            // required for chained defines
            Token val_tok = std::move(tl.tokens[2]);
            if (val_tok.type == TokenType::IDENTIFIER) {
                if (this->defines.contains(val_tok.raw)) {
                    val_tok = this->defines.at(val_tok.raw).value;
                }
            }

            DefineDef def{.name = std::move(tl.tokens[1]), .value = std::move(val_tok)};

            auto [it, inserted] = this->defines.try_emplace(key, std::move(def));
            if (!inserted) {
                throw AsmError(tl.lineno, std::format("Identifier {} already defined.", key), tl.source_file);
            }

            // consume the line here (dont emit %define lines into parsed)
            continue;
        }

        // macro definitions cannot be replaced, so just move these raw
        // macro BODIES are effected by defines!!
        if (first.type == TokenType::MACRO_DIRECTIVE && first.raw == "%MACRO") {
            parsed.push_back(std::move(tl));
            continue;
        }

        // finally perform token substitution on all non-'%' lines
        for (auto &tok : tl.tokens) {
            if (tok.type == TokenType::IDENTIFIER) {
                if (auto it = this->defines.find(tok.raw); it != this->defines.end()) {
                    tok.raw = it->second.value.raw;
                    tok.type = it->second.value.type;
                }
            }
        }

        parsed.push_back(std::move(tl));
    }

    // replace the source with the parsed, %DEFINE-free lines
    this->lines = std::move(parsed);
}

void MacroEngine::ExpandMacros() {
    std::vector<TokenizedLine> parsed;
    parsed.reserve(this->lines.size()); // probably a similar number after parsing, reserve it.

    // similar structure to resolve defines, just a bit more involved
    // go line by line, if hit a macro definition ensure it does not collide with builtins or defines

    // since macros are not one line we need to track if we are making a macro def or not
    bool in_macro = false;
    MacroDef current_macro;

    for (auto &tl : this->lines) {
        if (tl.tokens.empty()) {
            continue;
        }
        Token &first = tl.GetFirstToken();

        // if in macro either append body or finish writing the macro
        if (in_macro) {
            if (first.type == TokenType::MACRO_DIRECTIVE && first.raw == "%ENDMACRO") {
                // we hit the end of the macro
                // no checks here since they happen at definition
                this->macros[current_macro.name.raw] = std::move(current_macro);
                in_macro = false;
            } else {
                // record local labels as we see them
                if (first.type == TokenType::LABEL_DEF) {
                    // check for duplicates
                    auto it = std::ranges::find_if(current_macro.local_labels,
                                                   [&](const Token &t) { return t.raw == first.raw; });

                    if (it != current_macro.local_labels.end()) {
                        throw AsmError(tl.lineno, std::format("%MACRO has duplicate label definition: {}", first.raw),
                                       tl.source_file);
                    }
                    current_macro.local_labels.push_back(first);
                }
                // push the body line regardless of if its a label or not
                current_macro.body.push_back(std::move(tl));
            }
            // skip, since we dont want macro lines in the new parsed output
            continue;
        }
        // is this a new macro definition?
        if (first.type == TokenType::MACRO_DIRECTIVE && first.raw == "%MACRO") {
            in_macro = true;

            // fresh new macrodef
            current_macro = MacroDef{};

            // first ensure the macro has a valid name token
            // since macros can have no args, just check name
            const bool valid_syntax = tl.tokens.size() >= 2 && tl.tokens[1].type == TokenType::IDENTIFIER;

            if (!valid_syntax) {
                throw AsmError(tl.lineno, "Malformed %MACRO. Expected: %MACRO <identifier> <arg1 arg2...argN>");
            }

            // definition without the %MACRO directive
            auto mac_def_view = tl.tokens | std::ranges::views::drop(1);

            // check that NONE of the definition matches existing define/builtin
            for (const auto &t : mac_def_view) {
                if (INSTRS.contains(t.raw)) {
                    throw AsmError(tl.lineno,
                                   std::format("%MACRO cannot overwrite built-in mnemonic '{}'", mac_def_view[0].raw),
                                   tl.source_file);
                }

                if (defines.contains(t.raw)) {
                    throw AsmError(tl.lineno,
                                   std::format("%MACRO definition cannot contain name of %DEFINE '{}'", t.raw),
                                   tl.source_file);
                }
            }
            // valid macro definition from here on, move in the definition

            current_macro.name = std::move(tl.tokens[1]);
            current_macro.args = std::move(mac_def_view | std::ranges::views::drop(1) | std::views::as_rvalue |
                                           std::ranges::to<std::vector>());

            // check that there are no duplicate args
            //
            auto dedup_check = current_macro.args;
            std::ranges::sort(dedup_check);
            if (std::ranges::adjacent_find(dedup_check) != dedup_check.end()) {
                throw AsmError(tl.lineno, std::format("%MACRO {} has duplicate arguments", current_macro.name.raw),
                               tl.source_file);
            }

            // skip, since we dont want macro lines in the new parsed output
            continue;
        }

        // is this a macro call site?
        if (first.type == TokenType::IDENTIFIER && this->macros.contains(first.raw)) {
            const auto &macro = this->macros[first.raw];

            // ensure that the callsite has the right number of args
            if (tl.tokens.size() - 1 != macro.args.size()) {
                throw AsmError(tl.lineno,
                               std::format("Invalid number of arguments given to %MACRO {}, expected={} got={}",
                                           macro.name.raw, macro.args.size(), tl.tokens.size() - 1),
                               tl.source_file);
            }

            // Expand the macro!
            // - Mangle the labels
            // - Substitute the arguments
            // - Push the expanded lines into 'parsed'
            // (We will write this expansion logic next!)

            // skip, since we dont want the old callsite anymore
            continue;
        }

        parsed.push_back(std::move(tl));
    }

    // if we are still in the macro by the end of the loop someone forgot an %ENDMACRO
    if (in_macro) {
        throw AsmError(0, std::format("Missing %ENDMACRO for {}", current_macro.name.raw));
    }

    // replace the source with the parsed, macro expanded lines
    this->lines = std::move(parsed);
}
