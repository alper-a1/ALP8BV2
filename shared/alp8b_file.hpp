#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

// On-disk layout of a .alp8b binary.
// Owned by neither the assembler (writer) nor the sim (reader) - it is a
// property of the machine, so both sides share this single definition.
constexpr std::uint8_t MAX_CHARS_NAME = 32;
constexpr std::uint8_t MAX_CHARS_DATE = 32;
constexpr std::uint8_t MAX_CHARS_DESC = 128;
// ensure this is 1:1 byte order mapped for easy reading
#pragma pack(push, 1)
struct Alp8bFile {

    std::array<unsigned char, 4> magic = {'A', 'L', 'P', '8'};

    // metadata
    std::array<unsigned char, MAX_CHARS_NAME> name{};
    std::array<unsigned char, MAX_CHARS_DATE> date{};
    std::array<unsigned char, MAX_CHARS_DESC> description{};
    std::uint16_t clock_hz{};

    // program data
    std::array<std::uint8_t, 256> program{};
};
#pragma pack(pop)

// Ensure safety: must be trivially copyable for raw byte dump
static_assert(std::is_trivially_copyable_v<Alp8bFile>, "Alp8bFile must be trivially copyable");
static_assert(sizeof(Alp8bFile) == 4 + MAX_CHARS_NAME + MAX_CHARS_DATE + MAX_CHARS_DESC + 2 + 256,
              "Unexpected padding in Alp8bFile");
