#include "Valu.h" // The Verilator-generated header for alu.sv
#include "Valu_alu_defs.h"
#include <cstdint>
#include <memory>
#include <print>
#include <random>
#include <stdckdint.h>
#include <string>
#include <vector>

#include <magic_enum/magic_enum.hpp>
#include <verilated.h>

// logs mismatch into the string vector if occured
void log_mismatch(std::vector<std::string> &log, Valu_alu_defs::alu_op_t op, uint8_t a, uint8_t b, uint8_t expected,
                  uint8_t recieved) {
    if (expected != recieved) {
        log.emplace_back(std::format("mismatch on op={} a={} b={} expected={} got={}", magic_enum::enum_name(op), a, b,
                                     expected, recieved));
    }
}

void log_mismatch_carry(std::vector<std::string> &log, Valu_alu_defs::alu_op_t op, uint8_t a, uint8_t b, uint8_t cin,
                        uint8_t exp_res, uint8_t rec_res, uint8_t exp_c, uint8_t rec_c) {
    if (exp_res != rec_res || exp_c != rec_c) {
        log.emplace_back(std::format(
            "mismatch on op={} a={} b={} cin={} expected result={} got result={} expected cout={} got cout={}",
            magic_enum::enum_name(op), a, b, cin, exp_res, rec_res, exp_c, rec_c));
    }
}

int main(int argc, char **argv) {
    // Instantiate the hardware
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    auto top = std::make_unique<Valu>(contextp.get());

    std::println("Running testbench for {}.", top->name());

    std::vector<std::string> errors;

    // First test random values (noise testing)

    // to get random numbers for uint8_t
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> in_distrib(0, 255);
    std::bernoulli_distribution cin_distrib(0.5);

    for (int i = 0; i < 1000; i++) {
        uint8_t input_a = static_cast<uint8_t>(in_distrib(gen));
        uint8_t input_b = static_cast<uint8_t>(in_distrib(gen));
        uint8_t input_cin = static_cast<uint8_t>(cin_distrib(gen));

        // feed numeric inputs
        top->a = input_a;
        top->b = input_b;
        top->cin_flag = input_cin;

        // non flag related tests'
        // AND
        uint8_t expected_and = input_a & input_b;
        top->opcode = Valu_alu_defs::ALU_AND;
        top->eval();
        log_mismatch(errors, Valu_alu_defs::ALU_AND, input_a, input_b, expected_and, top->result);

        // OR
        uint8_t expected_or = input_a | input_b;
        top->opcode = Valu_alu_defs::ALU_OR;
        top->eval();
        log_mismatch(errors, Valu_alu_defs::ALU_OR, input_a, input_b, expected_or, top->result);

        // XOR
        uint8_t expected_xor = input_a ^ input_b;
        top->opcode = Valu_alu_defs::ALU_XOR;
        top->eval();
        log_mismatch(errors, Valu_alu_defs::ALU_XOR, input_a, input_b, expected_xor, top->result);

        // NOT
        uint8_t expected_not = ~input_a;
        top->opcode = Valu_alu_defs::ALU_NOT;
        top->eval();
        log_mismatch(errors, Valu_alu_defs::ALU_NOT, input_a, input_b, expected_not, top->result);

        // flag related tests

        // SHL
        uint8_t expected_shl = input_a << 1;
        uint8_t expected_shl_cout = (input_a & 0b1000'0000) >> 7;
        top->opcode = Valu_alu_defs::ALU_SHL;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_SHL, input_a, input_b, input_cin, expected_shl, top->result,
                           expected_shl_cout, top->cout_flag);

        // SHR
        uint8_t expected_shr = input_a >> 1;
        uint8_t expected_shr_cout = input_a & 0b0000'0001;
        top->opcode = Valu_alu_defs::ALU_SHR;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_SHR, input_a, input_b, input_cin, expected_shr, top->result,
                           expected_shr_cout, top->cout_flag);

        // ADD
        uint8_t expected_add = input_a + input_b;
        uint16_t temp_add = static_cast<uint16_t>(input_a) + input_b;
        uint8_t expected_add_cout = (temp_add >> 8) & 1;
        top->opcode = Valu_alu_defs::ALU_ADD;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_ADD, input_a, input_b, input_cin, expected_add, top->result,
                           expected_add_cout, top->cout_flag);

        // ADC
        uint8_t expected_adc = input_a + input_b + input_cin;
        uint16_t temp_adc = static_cast<uint16_t>(input_a) + input_b + input_cin;
        uint8_t expected_adc_cout = (temp_adc >> 8) & 1;
        top->opcode = Valu_alu_defs::ALU_ADC;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_ADC, input_a, input_b, input_cin, expected_adc, top->result,
                           expected_adc_cout, top->cout_flag);

        // SUB
        uint8_t expected_sub = input_a - input_b;
        uint8_t expected_sub_cout = (input_a < input_b);
        top->opcode = Valu_alu_defs::ALU_SUB;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_SUB, input_a, input_b, input_cin, expected_sub, top->result,
                           expected_sub_cout, top->cout_flag);

        // SBC
        uint8_t expected_sbc = input_a - input_b - input_cin;
        uint8_t expected_sbc_cout =
            ((static_cast<uint16_t>(input_a) - static_cast<uint16_t>(input_b) - input_cin) >> 8) & 1;
        top->opcode = Valu_alu_defs::ALU_SBC;
        top->eval();
        log_mismatch_carry(errors, Valu_alu_defs::ALU_SBC, input_a, input_b, input_cin, expected_sbc, top->result,
                           expected_sbc_cout, top->cout_flag);
    }

    // edge case testing tbd

    if (!errors.empty()) {
        std::println(stderr, "Hardware Verification FAILED with {} errors:", errors.size());
        // print only the first 20 errors to not flood if things go terrible
        for (size_t i = 0; i < std::min<size_t>(errors.size(), 20); i++) {
            std::println(stderr, "  {}", errors[i]);
        }
        return 1; // Non-zero exit code means failure
    }

    std::println("Testbench finished for {}. ALL TESTS PASSED.", top->name());
    return 0; // Success
}