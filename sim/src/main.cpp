#include "Vtop.h" // The Verilator-generated header for top.v
#include <iostream>
#include <memory>
#include <verilated.h>

int main(int argc, char **argv) {
    // Instantiate the hardware

    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto top = std::make_unique<Vtop>(contextp.get());

    std::cout << "Starting Simulation..." << std::endl;

    // Run for 10 clock cycles
    for (int i = 0; i < 10; i++) {
        top->clk = 0;
        top->eval();

        top->clk = 1;
        top->eval();

        std::cout << "Cycle: " << i << " | LED: " << (int)top->led << std::endl;
    }

    std::cout << "Simulation Complete." << std::endl;

    return 0;
}