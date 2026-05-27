#pragma once

#include "../core/ChessPiece.h"

#include <concepts>
#include <cstdint>

namespace chess {

template <typename T>
concept BoardSquareIndex = std::integral<T>;

template <typename F, typename... Args>
concept SquareCallback = std::invocable<F, int, const Piece&>;

template <typename F>
concept SquarePredicate = std::invocable<F, int, const Piece&> &&
                          std::same_as<std::invoke_result_t<F, int, const Piece&>, bool>;

template <typename Container>
concept SquareIndexContainer = requires(Container c) {
    { *c.begin() } -> std::convertible_to<int>;
};

} // namespace chess
