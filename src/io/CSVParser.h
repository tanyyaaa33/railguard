#pragma once

#include <string>
#include <vector>

namespace rg {

// Minimal RFC4180-ish split (no embedded commas in fields in our synthetic data).
std::vector<std::string> parseCsvLine(const std::string& line);

}  // namespace rg
