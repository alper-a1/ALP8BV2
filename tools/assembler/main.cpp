#include <filesystem>
#include <iostream>
#include <print>
#include <utility>

#include "asm_error.hpp"
#include "lexer.hpp"
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

    } catch (const AsmError &e) {
        std::println(std::cerr, "Assembly Error: {}", e.what());
        return 1;
    }

    return 0;
}