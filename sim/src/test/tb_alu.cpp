#include <cstdint>
#include <format>
#include <memory>
#include <print>
#include <random>
#include <string>
#include <vector>

#include <magic_enum/magic_enum.hpp>
#include <verilated.h>

// verilator generated headers
#include "Valu.h"
#include "Valu_alu_defs.h"
// ---

inline constexpr int NOISE_TEST_COUNT = 1000;

struct ModelALUResult {
    uint8_t result;
    uint8_t cout_flag;
    uint8_t zero_flag;
};

ModelALUResult model_alu(Valu_alu_defs::alu_op_t op, uint8_t a, uint8_t b, uint8_t cin) {
    ModelALUResult out{};

    switch (op) {
    case Valu_alu_defs::alu_op_t::ALU_AND: {
        out.result = a & b;
        out.cout_flag = 0;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_OR: {
        out.result = a | b;
        out.cout_flag = 0;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_XOR: {
        out.result = a ^ b;
        out.cout_flag = 0;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_NOT: {
        out.result = ~a;
        out.cout_flag = 0;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_SHL: {
        out.result = a << 1;
        out.cout_flag = (a & 0b1000'0000) >> 7;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_SHR: {
        out.result = a >> 1;
        out.cout_flag = a & 0b1;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_ADD: {
        out.result = a + b;
        out.cout_flag = ((static_cast<uint16_t>(a) + b) >> 8) & 1;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_ADC: {
        out.result = a + b + cin;
        out.cout_flag = ((static_cast<uint16_t>(a) + b + cin) >> 8) & 1;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_SUB: {
        out.result = a - b;
        out.cout_flag = (a < b); // if A is smaller a borrow (1) is generated
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_SBC: {
        out.result = a - b - cin;
        out.cout_flag = ((static_cast<uint16_t>(a) - static_cast<uint16_t>(b) - cin) >> 8) & 1;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_INC: {
        out.result = a + 1;
        out.cout_flag = 0;
        break;
    }
    case Valu_alu_defs::alu_op_t::ALU_DEC: {
        out.result = a - 1;
        out.cout_flag = 0;
        break;
    }
    }
    out.zero_flag = out.result == 0;

    return out;
}

// compare the sim results to the model results to ensure matching, log error if mismatch found.
void log_mismatch(const Valu &sim, const ModelALUResult &correct, std::vector<std::string> &errors,
                  Valu_alu_defs::alu_op_t aluop, uint8_t a, uint8_t b) {
    // result mismatch
    if (sim.result != correct.result) {
        errors.emplace_back(std::format("result mismatch: op={} a={} b={} expected={} got={}",
                                        magic_enum::enum_name(aluop), a, b, correct.result, sim.result));
    }

    // flags mismatch cout
    if (sim.cout_flag != correct.cout_flag) {
        errors.emplace_back(std::format("flag mismatch: op={} a={} b={} exp_cout={} got_cout={} ",
                                        magic_enum::enum_name(aluop), a, b, correct.cout_flag, sim.cout_flag));
    }

    // flags mismatch zero
    if (sim.zero_flag != correct.zero_flag) {
        errors.emplace_back(std::format("flag mismatch: op={} a={} b={} exp_zero={} got_zero={} ",
                                        magic_enum::enum_name(aluop), a, b, correct.zero_flag, sim.zero_flag));
    }
}

int main(int argc, char **argv) {
    // Instantiate the hardware
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto sim = std::make_unique<Valu>(contextp.get(), "ALU");

    std::println("Running testbench for {}.", sim->name());

    std::vector<std::string> errors;

    // First test random values (noise testing)

    // to get random numbers for uint8_t
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> in_distrib(0, 255);
    std::bernoulli_distribution cin_distrib(0.5);

    for (int i = 0; i < NOISE_TEST_COUNT; i++) {
        uint8_t input_a = static_cast<uint8_t>(in_distrib(gen));
        uint8_t input_b = static_cast<uint8_t>(in_distrib(gen));
        uint8_t input_cin = static_cast<uint8_t>(cin_distrib(gen));

        // feed numeric inputs
        sim->a = input_a;
        sim->b = input_b;
        sim->cin_flag = input_cin;

        for (auto aluop : magic_enum::enum_values<Valu_alu_defs::alu_op_t>()) {
            // get the correct model ALU results to compare to.
            ModelALUResult correct = model_alu(aluop, input_a, input_b, input_cin);

            // run the sim to get the simulated results
            sim->opcode = aluop;
            sim->eval();

            log_mismatch(*sim, correct, errors, aluop, input_a, input_b);
        }
    }

    // edge case testing boundry values

    // min(-128)=0x80, max(127)=0x7F
    std::vector<uint8_t> edge_a = {0, 1, 0xFF, 0x80, 0x7F};
    std::vector<uint8_t> edge_b = {0, 1, 0xFF, 0x80, 0x7F};

    for (auto input_a : edge_a) {
        for (auto input_b : edge_b) {
            for (auto input_cin : {0, 1}) {
                sim->a = input_a;
                sim->b = input_b;
                sim->cin_flag = input_cin;

                // test every operation for the boundry values.
                for (auto aluop : magic_enum::enum_values<Valu_alu_defs::alu_op_t>()) {
                    // get the correct model ALU results to compare to.
                    ModelALUResult correct = model_alu(aluop, input_a, input_b, input_cin);

                    // run the sim to get the simulated results
                    sim->opcode = aluop;
                    sim->eval();

                    log_mismatch(*sim, correct, errors, aluop, input_a, input_b);
                }
            }
        }
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
    return 0; // Success
}