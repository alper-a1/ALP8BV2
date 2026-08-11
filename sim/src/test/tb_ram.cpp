#include <verilated.h>

// verilator generated headers
#include "Vram.h"
// ---

int main(int argc, char **argv) {
    // Instantiate the hardware
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto top = std::make_unique<Vram>(contextp.get());

    return 0;
}