#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "token.hpp"

struct MacroDef {
    Token name;

    // MUST be identifer tokens
    std::vector<Token> args;

    // all the lines between %MACRO and %ENDMACRO
    std::vector<TokenizedLine> body;

    std::vector<Token> local_labels;
};

struct DefineDef {
    // MUST BE identifier
    Token name;

    // can be IDENTIFER or IMMEDIATE token
    Token value;
};

// main class to collect, expand and replace defines and macros
// two pass (define -> macro) with a shared namespace
class MacroEngine {
  public:
    MacroEngine(std::vector<TokenizedLine> lines);

    std::vector<TokenizedLine> Run();

  private:
    // macroengine owns the source lines while it processes
    std::vector<TokenizedLine> lines;

    // defines mapping , keys MUST NOT collide with _macros
    std::unordered_map<std::string, DefineDef> defines;
    std::unordered_map<std::string, MacroDef> macros;
    // internal global macro label prefix counter
    size_t macro_instance_ctr = 0;

    // collect all define defenitions, perform token substitution, strip define lines
    void ResolveDefines();

    // collect all macros. ensuring that their name&args do not clash with defines
    // expand macros, prefix internal macro labels with global counter to maintain uniqueness
    // strip out macro lines & definitions
    void ExpandMacros();
};

// public interface to use the macro. should not create and call macroengine.run directly
std::vector<TokenizedLine> ResolveMacroAndDefine(std::vector<TokenizedLine> lines);