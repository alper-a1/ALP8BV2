// #include <format>
// #include <print>
#include <verilated.h>

// verilator generated headers
#include "Valp8b.h"
// required for cpu internals access
// provides the type definitions for all components
// unused __directly__, but requried.
#include "Valp8b__Syms.h" // IWYU pragma: keep
// ---

int main(int argc, char **argv) {
    // Instantiate the hardware
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto sim = std::make_unique<Valp8b>(contextp.get(), "CPU");

    sim->cpu_top->dp->gpr0->val;
    sim->cpu_top->dp->u_ram->mem;
}