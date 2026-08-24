#include "write_binary.hpp"
#include "asm_error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ranges>
#include <string_view>

constexpr std::uint8_t MAX_CHARS_NAME = 32;
constexpr std::uint8_t MAX_CHARS_DATE = 32;
constexpr std::uint8_t MAX_CHARS_DESC = 128;
// ensure this is 1:1 byte order mapped for easy reading
#pragma pack(push, 1)
struct BinaryFormat {

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
static_assert(std::is_trivially_copyable_v<BinaryFormat>, "BinaryFormat must be trivially copyable");
static_assert(sizeof(BinaryFormat) == 4 + MAX_CHARS_NAME + MAX_CHARS_DATE + MAX_CHARS_DESC + 2 + 256,
              "Unexpected padding in BinaryFormat");

template <std::size_t N>
    requires(N > 0)
constexpr auto StringToCharArray(std::string_view str) -> std::array<unsigned char, N> {
    std::array<unsigned char, N> out{};
    auto chars =
        str | std::views::take(N - 1) | std::views::transform([](char c) { return static_cast<unsigned char>(c); });
    std::ranges::copy(chars, out.begin());
    return out;
}

void WriteBinary(const std::array<std::uint8_t, 256> &data, const ProgramMetadata &meta,
                 const std::filesystem::path &location) {

    BinaryFormat to_write{.name = StringToCharArray<MAX_CHARS_NAME>(meta.name),
                          .date = StringToCharArray<MAX_CHARS_DATE>(meta.date),
                          .description = StringToCharArray<MAX_CHARS_DESC>(meta.description),
                          .clock_hz = meta.clock,
                          .program = data};

    std::ofstream out(location, std::ios::binary);
    if (!out) {
        throw AsmError(0, "WRITEERR FAILED TO OPEN OUT FILE");
    }

    // convert to bytes & write the whole file in one single binary dump
    auto raw_bytes = std::bit_cast<std::array<char, sizeof(BinaryFormat)>>(to_write);
    out.write(raw_bytes.data(), raw_bytes.size());
}