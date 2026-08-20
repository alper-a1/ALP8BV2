#include <filesystem>
#include <iostream>
#include <print>
#include <utility>

#include "asm_error.hpp"
#include "label_pass.hpp"
#include "lexer.hpp"
#include "macro_pass.hpp"
#include "metadata_pass.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_path>\n";
        return 1;
    }

    try {
        std::filesystem::path input_path(argv[1]);

        auto tokenized = LoadSourceWithBuiltins(input_path);
        auto [meta_pass, metadata] = ExtractMetadata(std::move(tokenized));
        auto expanded = ResolveDefineAndMacro(std::move(meta_pass));
        auto [pc_attached, sym_table] = GenerateSymbolMap(std::move(expanded));

    } catch (const AsmError &e) {
        std::println(std::cerr, "Assembly Error: {}", e.what());
        return 1;
    }

    return 0;
}