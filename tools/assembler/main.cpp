#include <filesystem>
#include <iostream>
#include <print>
#include <utility>

#include "asm_error.hpp"
#include "codegen.hpp"
#include "label_pass.hpp"
#include "lexer.hpp"
#include "macro_pass.hpp"
#include "metadata_pass.hpp"
#include "write_binary.hpp"

int main(int argc, char **argv) {
    // check if the program name + 2 arguments are provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_path> [<output_path>]\n";
        return 1;
    }

    std::filesystem::path input_path = argv[1];
    std::filesystem::path output_path;

    if (argc >= 3) {
        output_path = argv[2];
    } else {
        // default to same dir, just .bin output name
        output_path = input_path.replace_extension(".bin");
    }

    try {
        auto tokenized = LoadSourceWithBuiltins(input_path);
        auto [meta_pass, metadata] = ExtractMetadata(std::move(tokenized));
        auto expanded = ResolveDefineAndMacro(std::move(meta_pass));
        auto label_mapped = ResolveAndMapLabels(std::move(expanded));
        auto compiled = ConvertToMachineCode(label_mapped);
        WriteBinary(compiled, metadata, output_path);

    } catch (const AsmError &e) {
        std::println(std::cerr, "Assembly Error: {}", e.what());
        return 1;
    }

    return 0;
}