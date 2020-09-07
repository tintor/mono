#pragma once
#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/ostream.h"

using fmt::print;
using fmt::format;
constexpr auto warning = fg(fmt::color::crimson);
