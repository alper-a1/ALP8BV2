// tb_isa.cpp — ISA conformance testbench for cpu_top
//
// Every instruction path in the control FSM is exercised with HAND-ENCODED
// machine code (deliberately NOT run through the assembler), so a failure is
// unambiguously a CPU bug, never an assembler bug.
//
// Flag semantics encoded by the cases (per docs/isaV1.1.txt + design decision):
//   - Z flag: updated ONLY by compare/branch instructions (CBZ/CBNZ/CBEQ/CBNE/CBLT),
//     since only those consume it.
//   - C flag: updated by arithmetic (ADD/ADC/SUB/SBC, ROR/SHR), settable by
//     CLC/SEC, and consumable by ADC/SBC — so carry CHAINING across instructions
//     is real behavior and must be tested.
//   - Reserved opcodes (0x01 SYSCALL slot) and the S_ERROR lockup are
//     INTENTIONALLY UNTESTED — error state design is pending.
//
// Build & run (wired into CMake as target Vtbsim_isa):
//   cmake --build build --target Vtbsim_isa && ./build/Vtbsim_isa
//
// ============================================================================
// TODO — steps to finalise this testbench:
//
// 1. Sanity-case the harness itself FIRST. Before trusting any failure, add one
//    trivial case (e.g. a NOP) and confirm RAM is untouched and PC advances as
//    expected. This proves the reset/load/snapshot plumbing before it is used
//    to indict the hardware.
//
// 2. Budget ticks per case deliberately. After bench.run()'s reset sequence the
//    machine is already one fetch cycle into instruction 0 (state=S_FETCH_INPC,
//    IR=ram[0x00], PC=0x00). Derive budgets from the FSM's per-class cycle cost
//    (2 fetch cycles + execution cycles; the per-class paths are listed in
//    src/cu/fsm_states.sv). Running more ticks than the program needs is safe
//    for halt-loop style programs (JMP __halt), but exact budgets catch
//    off-by-one state machine bugs faster.
//
// 3. Coverage checklist — 1-2 cases per group; branches ALWAYS as a
//    taken/not-taken pair:
//      - 2reg math:  ADD, ADC, SUB, SBC, AND, OR, XOR  (3-cycle path, untested)
//      - carry chain: SUB then SBC across a 16-bit value (C consumed, not just set)
//      - 1reg math:  INC/DEC wraparound, NOT, ROR/SHR carry-out (2-cycle path)
//      - branches:   JMP, JC, JNC, JZR, JMPR, CBZ, CBNZ, CBEQ, CBNE, CBLT
//      - memory:     LDM (incl. reading 0x00 / 0xFF), STM into 0x00 / 0xFF,
//                    LDI immediate alignment
//      - misc:       MOV, CLC, SEC, RNG
//
// 4. RNG case: the LFSR is seeded to 0xAA with fixed taps, so its output
//    sequence is fully deterministic. Compute the first N outputs independently
//    in C++ (same polynomial) and compare exactly — this also pins down the
//    lfsr_rng.sv feedback fix.
//
// 5. For branch tests, have each arm write a distinct marker byte to a scratch
//    RAM address and assert on that marker — it proves which path was taken
//    without needing to guess the final PC.
//
// 6. When green, this file is the regression gate: rerun it after any .sv change.
// ============================================================================

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <vector>

#include <verilated.h>

// verilator generated headers
#include "Vcpu_top.h"
// required for digging through cpu_top members
#include "Vcpu_top__Syms.h" // IWYU pragma: keep

// complete machine state, for test assertions
struct MachineState {
    std::array<std::uint8_t, 256> ram{};
    std::uint8_t r0{}, r1{}, r2{}, r3{};
    std::uint8_t pc{}, mar{};
    bool zf{}, cf{};
};

// one conformance case: hand-encoded machine code plus a check on the outcome
struct IsaTestCase {
    const char *name;
    // bytes placed at the matching RAM address; addresses beyond the vector
    // read back as zero
    std::vector<std::uint8_t> init_ram;
    // clock edges to run after reset
    std::uint64_t ticks;
    // assert on the final machine state; append to errors on mismatch
    std::function<void(const MachineState &, std::vector<std::string> &)> check;
};

// one verilated cpu_top, reused for every case
class IsaBench {
  public:
    IsaBench()
        : contextp(std::make_unique<VerilatedContext>()), sim(std::make_unique<Vcpu_top>(contextp.get(), "tb_isa")) {}

    // load init_ram, clear all file registers, run the hardware reset cycle,
    // advance `ticks` edges, and return the resulting machine state.
    //
    // the model instance is shared between cases, and rst_n does NOT clear the
    // gprs/mar/alu latches/ir (only pc, flags and lfsr reset), so every case
    // gets an explicit zero start on top of the hardware reset.
    MachineState run(const std::span<const std::uint8_t> init_ram, std::uint64_t ticks) {
        auto *dp = sim->cpu_top->dp;

        std::memset(dp->u_ram->mem.data(), 0, sizeof(dp->u_ram->mem));
        std::ranges::copy(init_ram, dp->u_ram->mem.data());

        dp->gpr0->val = 0;
        dp->gpr1->val = 0;
        dp->gpr2->val = 0;
        dp->gpr3->val = 0;
        dp->mar->val = 0;
        dp->alutmp->val = 0;
        dp->alures->val = 0;
        dp->u_ir->val = 0;

        // identical reset sequence to sim/src/cpu.cpp SimReset:
        // after it, the machine has already consumed ONE fetch cycle of
        // instruction 0 (state=S_FETCH_INPC, IR=ram[0x00], PC=0x00)
        sim->rst_n = 0;
        tick();
        tick();
        tick();
        sim->rst_n = 1;
        tick();

        for (std::uint64_t i = 0; i < ticks; ++i) {
            tick();
        }
        return snapshot();
    }

    void finish() const { sim->final(); }

  private:
    // one low -> high clock edge, identical pattern to sim/src/cpu.cpp SimTick
    void tick() const {
        sim->clk = 0;
        sim->eval();
        sim->clk = 1;
        sim->eval();
    }

    [[nodiscard]] MachineState snapshot() const {
        auto *dp = sim->cpu_top->dp;
        MachineState s;
        std::copy(dp->u_ram->mem.data(), dp->u_ram->mem.data() + s.ram.size(), s.ram.begin());
        s.r0 = dp->gpr0->val;
        s.r1 = dp->gpr1->val;
        s.r2 = dp->gpr2->val;
        s.r3 = dp->gpr3->val;
        s.pc = dp->u_pc->val;
        s.mar = dp->mar->val;
        s.zf = dp->u_flags->zero_flag != 0;
        s.cf = dp->u_flags->carry_flag != 0;
        return s;
    }

    std::unique_ptr<VerilatedContext> contextp;
    std::unique_ptr<Vcpu_top> sim;
};

int main(int argc, char **argv) {
    IsaBench bench;
    std::vector<std::string> errors;

    // conformance cases (see TODO above)
    std::vector<IsaTestCase> cases = {
        // shape of a case, for reference:
        // {
        //     .name = "example: ...",
        //     .init_ram = { /* hand-encoded machine code, byte 0 at RAM 0x00 */ },
        //     .ticks = 0,
        //     .check = [](const MachineState &s, std::vector<std::string> &errs) {
        //         // append to errs on mismatch, e.g.:
        //         // if (s.ram[0x10] != 0x5A) { errs.push_back("ram[0x10] ..."); }
        //     },
        // },
    };

    for (const auto &tc : cases) {
        const std::size_t before = errors.size();
        MachineState s = bench.run(tc.init_ram, tc.ticks);
        tc.check(s, errors);
        // label any new errors with their case
        for (std::size_t i = before; i < errors.size(); ++i) {
            errors[i] = std::format("case '{}': {}", tc.name, errors[i]);
        }
    }

    bench.finish();

    // same reporting convention as tb_ram.cpp
    if (errors.empty()) {
        std::println("tb_isa: all {} conformance case(s) passed", cases.size());
        return 0;
    }
    std::println(stderr, "tb_isa: FAILED with {} error(s), printing the first 20:", errors.size());
    for (std::size_t i = 0; i < std::min<std::size_t>(errors.size(), 20); ++i) {
        std::println(stderr, "  {}", errors[i]);
    }
    return 1;
}
