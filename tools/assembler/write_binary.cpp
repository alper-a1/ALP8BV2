#include "write_binary.hpp"
#include "alp8b_file.hpp"
#include "asm_error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ranges>
#include <string_view>

template <std::size_t N>
    requires(N > 0)
constexpr std::array<unsigned char, N> StringToCharArray(std::string_view str) {
    std::array<unsigned char, N> out{};
    auto chars =
        str | std::views::take(N - 1) | std::views::transform([](char c) { return static_cast<unsigned char>(c); });
    std::ranges::copy(chars, out.begin());
    return out;
}

// uint16 to little endian array
constexpr std::array<std::uint8_t, 2> UI16ToArrayLE(std::uint16_t in) {
    return {static_cast<std::uint8_t>(in & 0xFF), static_cast<std::uint8_t>(in >> 8)};
}

void WriteBinary(const std::array<std::uint8_t, 256> &data, const ProgramMetadata &meta,
                 const std::filesystem::path &location) {

    Alp8bFile to_write{.name = StringToCharArray<ALP8BF_CHARS_NAME>(meta.name),
                       .date = StringToCharArray<ALP8BF_CHARS_DATE>(meta.date),
                       .description = StringToCharArray<ALP8BF_CHARS_DESC>(meta.description),
                       .clock_hz = UI16ToArrayLE(meta.clock),
                       .program = data};

    std::ofstream out(location, std::ios::binary);
    if (!out) {
        throw AsmError(0, "WRITEERR FAILED TO OPEN OUT FILE");
    }

    // convert to bytes & write the whole file in one single binary dump
    out.write(reinterpret_cast<const char *>(&to_write), sizeof(Alp8bFile));

    if (!out) {
        throw AsmError(0, "WRITEERR: FAILED WHILE WRITING DATA TO FILE");
    }
}