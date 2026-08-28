#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// each program rom is 512 byte aligned
constexpr std::size_t ALP8BF_TOTAL_BYTES = 512;

constexpr std::array<unsigned char, 4> ALP8BF_MAGIC = {'A', 'L', 'P', '8'};

// On-disk layout of a .alp8b binary.
// Owned by neither the assembler (writer) nor the sim (reader) - it is a
// property of the machine, so both sides share this single definition.
constexpr std::uint8_t ALP8BF_CHARS_NAME = 32;
constexpr std::uint8_t ALP8BF_CHARS_DATE = 32;
constexpr std::uint8_t ALP8BF_CHARS_DESC = 128;
struct Alp8bFile {

    std::array<unsigned char, 4> magic = ALP8BF_MAGIC;

    // metadata
    std::array<unsigned char, ALP8BF_CHARS_NAME> name{};
    std::array<unsigned char, ALP8BF_CHARS_DATE> date{};
    std::array<unsigned char, ALP8BF_CHARS_DESC> description{};
    std::array<std::uint8_t, 2> clock_hz{}; // little endian

    // program data
    std::array<std::uint8_t, 256> program{};

    // padding (each program rom is 512 byte aligned)
    // can be used for future metadata
    // - FPGA precalculated clock divider ?
    std::array<std::uint8_t, 58> _reserved_padding{};
};

// variety of safety & sanity checks to ensure no weird headaches later
static_assert(std::is_trivially_copyable_v<Alp8bFile>, "Alp8bFile must be trivially copyable");
static_assert(sizeof(Alp8bFile) == ALP8BF_TOTAL_BYTES, "Unexpected padding in Alp8bFile");
static_assert(alignof(Alp8bFile) == 1, "Alp8bFile must have 1-byte alignment");
static_assert(std::is_standard_layout_v<Alp8bFile>, "Must have standard layout for C compatibility");