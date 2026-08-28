#include <format>
#include <print>
#include <verilated.h>

// verilator generated headers
#include "Vram.h"
#include "Vram_ram.h"
// ---

int main(int argc, char **argv) {
    // Instantiate the hardware
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto sim = std::make_unique<Vram>(contextp.get(), "RAM");

    std::println("Running testbench for {}.", sim->name());

    std::vector<std::string> errors;

    // advance one full clock cycle (0 -> 1), does not touch other inputs
    auto tick = [&]() {
        sim->clk = 0;
        sim->eval();
        sim->clk = 1;
        sim->eval();
    };

    // write d_in to addr on the next clock edge
    auto do_write = [&](uint8_t addr, uint8_t data) {
        sim->addr = addr;
        sim->d_in = data;
        sim->we = 1;
        sim->eval();
        tick();
        sim->we = 0;
        sim->eval();
    };

    // check that reading addr combinationally yields expected, without a clk pulse
    auto check_read = [&](uint8_t addr, uint8_t expected, const std::string &what) {
        sim->addr = addr;
        sim->eval();
        if (sim->d_out != expected) {
            errors.emplace_back(std::format("err: {} (addr=0x{:02X}, expected=0x{:02X}, got=0x{:02X})", what, addr,
                                            expected, sim->d_out));
        }
    };

    // simple write then read test at 0x20
    sim->addr = 0x20;
    sim->d_in = 0xAA;
    sim->we = 1;
    sim->eval();

    // no write shouldve occured, since clk did not pulse.
    if (sim->ram->mem[0x20] != 0) {
        errors.emplace_back("err: write occurred without clk pulse");
    }

    // pulse clock
    tick();
    if (sim->ram->mem[0x20] != 0xAA) {
        errors.emplace_back("err: write failed on clk pulse");
    }
    sim->we = 0;

    // check that we can async read this back before the next clock pulse
    check_read(0x20, 0xAA, "async read failed");

    // check that changes to addr / d_in does not mess with memory without clk pulse
    sim->d_in = 0x12;
    sim->addr = 0x80;
    sim->we = 1;
    sim->eval();

    if (sim->ram->mem[0x80] != 0) {
        errors.emplace_back("err: write occured without clk pulse");
    }

    // now pulse clock but with we deasserted; write must not occur
    sim->we = 0;
    tick();

    if (sim->ram->mem[0x80] != 0) {
        errors.emplace_back("err: write occured without we");
    }

    // addr held constant, output should reflect mem contents with no clk edge
    sim->addr = 0x20;
    sim->eval();
    if (sim->d_out != 0xAA) {
        errors.emplace_back("err: d_out incorrect on stable addr (no clk edge)");
    }

    // write does not clobber other addresses
    check_read(0x21, 0x00, "adjacent address was clobbered by unrelated write");

    // read and write same address, same cycle:
    // write is synchronous (lands after the edge), read is combinational
    // (reflects current mem), so d_out should show the OLD value during
    // the cycle the write happens, not the new one.
    sim->addr = 0x40;
    sim->d_in = 0x55;
    sim->we = 1;
    sim->eval();
    if (sim->d_out != 0x00) {
        errors.emplace_back(
            std::format("err: read-during-write did not show old value (expected=0x00, got=0x{:02X})", sim->d_out));
    }
    tick();
    sim->we = 0;
    sim->eval();
    check_read(0x40, 0x55, "read-during-write: new value not present after clk edge");

    // address boundaries
    do_write(0x00, 0x11);
    check_read(0x00, 0x11, "boundary address 0x00 failed");

    do_write(0xFF, 0x22);
    check_read(0xFF, 0x22, "boundary address 0xFF failed");

    // full address sweep: every address independently writable/readable
    for (int i = 0; i < 256; i++) {
        do_write(static_cast<uint8_t>(i), static_cast<uint8_t>(i));
    }
    for (int i = 0; i < 256; i++) {
        check_read(static_cast<uint8_t>(i), static_cast<uint8_t>(i),
                   std::format("address sweep mismatch at 0x{:02X}", i));
    }

    // data patterns: all-zero, all-one, walking bits
    do_write(0x50, 0x00);
    check_read(0x50, 0x00, "all-zero pattern failed");

    do_write(0x50, 0xFF);
    check_read(0x50, 0xFF, "all-one pattern failed");

    for (int bit = 0; bit < 8; bit++) {
        uint8_t pattern = static_cast<uint8_t>(1u << bit);
        do_write(0x50, pattern);
        check_read(0x50, pattern, std::format("walking-1 pattern failed at bit {}", bit));
    }

    sim->final();

    if (!errors.empty()) {
        std::println(stderr, "Hardware Verification FAILED with {} errors:", errors.size());
        // print only the first 20 errors to not flood if things go terrible
        std::println("Printing the first 20 errors:");
        for (size_t i = 0; i < std::min<size_t>(errors.size(), 20); i++) {
            std::println(stderr, "  {}", errors[i]);
        }
        return 1; // Non-zero exit code means failure
    }

    std::println("Testbench finished for {}. ALL TESTS PASSED.", sim->name());
    return 0;
}