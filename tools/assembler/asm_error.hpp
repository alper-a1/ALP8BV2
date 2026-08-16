#pragma once

#include <format>
#include <stdexcept>
#include <string>

// small wrapper over errors thrown
class AsmError : public std::runtime_error {
  public:
    explicit AsmError(int lineno, const std::string &message, const std::string &source_file)
        : std::runtime_error(std::format("{}:{}, {}", source_file, lineno, message)) {}
};
