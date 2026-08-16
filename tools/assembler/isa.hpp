#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
enum class OperandShape { NONE, SINGLE_REG, SINGLE_REG_IMM8, DUAL_REG, DUAL_REG_IMM8, IMM8_ONLY };

// quick wrapper to map shape to the words (bytes) it takes up
// anything with an imm8 is two bytes wide
constexpr int ShapeToWordSize(OperandShape shape) {
    switch (shape) {
    case OperandShape::NONE:
    case OperandShape::SINGLE_REG:
    case OperandShape::DUAL_REG:
        return 1;

    case OperandShape::IMM8_ONLY:
    case OperandShape::SINGLE_REG_IMM8:
    case OperandShape::DUAL_REG_IMM8:
        return 2;
    }
}

struct InstrDef {
    uint8_t encoding;
    OperandShape shape;

    int GetWordSize() const { return ShapeToWordSize(shape); }
};

const std::unordered_map<std::string, InstrDef> INSTRS{
    // in opcode order, roughly grouped by use
    // regiser blocks are zeroed out as default
    {"NOP", InstrDef{.encoding = 0b0000'0000, .shape = OperandShape::NONE}},
    {"RST", InstrDef{.encoding = 0b0000'0001, .shape = OperandShape::NONE}},
    {"CLC", InstrDef{.encoding = 0b0000'0010, .shape = OperandShape::NONE}},
    {"SEC", InstrDef{.encoding = 0b0000'0011, .shape = OperandShape::NONE}},

    {"ROR", InstrDef{.encoding = 0b0000'01'00, .shape = OperandShape::SINGLE_REG}},
    {"SHR", InstrDef{.encoding = 0b0000'10'00, .shape = OperandShape::SINGLE_REG}},
    {"NOT", InstrDef{.encoding = 0b0000'11'00, .shape = OperandShape::SINGLE_REG}},
    {"INC", InstrDef{.encoding = 0b0001'00'00, .shape = OperandShape::SINGLE_REG}},
    {"DEC", InstrDef{.encoding = 0b0001'01'00, .shape = OperandShape::SINGLE_REG}},
    {"LDI", InstrDef{.encoding = 0b0001'10'00, .shape = OperandShape::SINGLE_REG_IMM8}},
    {"RNG", InstrDef{.encoding = 0b0001'11'00, .shape = OperandShape::SINGLE_REG}},

    {"SUB", InstrDef{.encoding = 0b0010'0000, .shape = OperandShape::DUAL_REG}},
    {"SBC", InstrDef{.encoding = 0b0011'0000, .shape = OperandShape::DUAL_REG}},

    {"JMP", InstrDef{.encoding = 0b0100'0000, .shape = OperandShape::IMM8_ONLY}},
    {"JC", InstrDef{.encoding = 0b0100'0001, .shape = OperandShape::IMM8_ONLY}},
    {"JNC", InstrDef{.encoding = 0b0100'0010, .shape = OperandShape::IMM8_ONLY}},
    //  0b0100'0011 is unused
    {"JMPR", InstrDef{.encoding = 0b0100'0100, .shape = OperandShape::SINGLE_REG}},
    {"CBZ", InstrDef{.encoding = 0b0100'1000, .shape = OperandShape::SINGLE_REG_IMM8}},
    {"CBNZ", InstrDef{.encoding = 0b0100'1100, .shape = OperandShape::SINGLE_REG_IMM8}},
    {"CBEQ", InstrDef{.encoding = 0b0101'0000, .shape = OperandShape::DUAL_REG_IMM8}},
    {"CBNE", InstrDef{.encoding = 0b0110'0000, .shape = OperandShape::DUAL_REG_IMM8}},
    {"CBLT", InstrDef{.encoding = 0b0111'0000, .shape = OperandShape::DUAL_REG_IMM8}},

    {"ADD", InstrDef{.encoding = 0b1000'0000, .shape = OperandShape::DUAL_REG}},
    {"ADC", InstrDef{.encoding = 0b1001'0000, .shape = OperandShape::DUAL_REG}},
    {"OR", InstrDef{.encoding = 0b1010'0000, .shape = OperandShape::DUAL_REG}},
    {"AND", InstrDef{.encoding = 0b1011'0000, .shape = OperandShape::DUAL_REG}},

    {"XOR", InstrDef{.encoding = 0b1100'0000, .shape = OperandShape::DUAL_REG}},
    {"LDM", InstrDef{.encoding = 0b1101'0000, .shape = OperandShape::DUAL_REG}},
    {"STM", InstrDef{.encoding = 0b1110'0000, .shape = OperandShape::DUAL_REG}},
    {"MOV", InstrDef{.encoding = 0b1111'0000, .shape = OperandShape::DUAL_REG}},
};

const std::unordered_map<std::string, uint8_t> REGISTERS{
    {"R0", 0b00},
    {"R1", 0b01},
    {"R2", 0b10},
    {"R3", 0b11},
};