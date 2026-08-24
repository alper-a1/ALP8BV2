#include "write_binary.hpp"
#include "asm_error.hpp"
#include "alp8b_file.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ranges>
#include <string_view>

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

    Alp8bFile to_write{.name = StringToCharArray<MAX_CHARS_NAME>(meta.name),
                          .date = StringToCharArray<MAX_CHARS_DATE>(meta.date),
                          .description = StringToCharArray<MAX_CHARS_DESC>(meta.description),
                          .clock_hz = meta.clock,
                          .program = data};

    std::ofstream out(location, std::ios::binary);
    if (!out) {
        throw AsmError(0, "WRITEERR FAILED TO OPEN OUT FILE");
    }

    // convert to bytes & write the whole file in one single binary dump
    auto raw_bytes = std::bit_cast<std::array<char, sizeof(Alp8bFile)>>(to_write);
    out.write(raw_bytes.data(), raw_bytes.size());
}