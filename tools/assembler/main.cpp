
#include "lexer.hpp"
#include "metadata_pass.hpp"

int main(int argc, char **argv) {
    // argv will be path input at some point in future.

    auto tokenized = LoadSourceWithBuiltins(argv[1]);
    auto [meta_pass, metadata] = ExtractMetadata(tokenized);
}