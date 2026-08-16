#pragma once

#include <filesystem>
#include <format>
#include <stdexcept>
#include <string_view>

class AsmError : public std::runtime_error {
  public:
    explicit AsmError(int lineno, std::string_view message, const std::filesystem::path &origin = {})
        : std::runtime_error(origin.empty() ? std::format("{}, {}", lineno, message)
                                            : std::format("{}:{}, {}", origin.string(), lineno, message)) {}
};