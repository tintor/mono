#pragma once
#include "santorini/board.h"
#include "santorini/action.h"

Step AutoMCTS(const Board& board, const size_t iterations, const bool trivial = false);
