#pragma once
#include "santorini/board.h"
#include "santorini/action.h"

#include <optional>
#include <string_view>

using std::optional;
using std::string_view;

bool AreCardsAllowed(Card card1, Card card2);

optional<string_view> Next(Board& board);
optional<string_view> Place(Board& board, Coord dest);

optional<string_view> CanMove(const Board& board, Coord src, Coord dest);
optional<string_view> Move(Board& board, Coord src, Coord dest);

optional<string_view> Build(Board& board, Coord dest, bool dome);
optional<string_view> Execute(Board& board, const Step& step);
