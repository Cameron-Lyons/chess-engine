#pragma once

#include "../core/ChessBoard.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class EndgameTablebase {
public:
    enum class TablebaseType : std::uint8_t { SYZYGY, NALIMOV, CUSTOM };

    struct TablebaseResult {
        int distanceToMate;
        std::vector<Move> bestMoves;
        bool isExact;
    };
    EndgameTablebase(const std::string& tablebasePath = "tablebases/");
    ~EndgameTablebase();
    EndgameTablebase(const EndgameTablebase&) = delete;
    auto operator=(const EndgameTablebase&) -> EndgameTablebase& = delete;
    EndgameTablebase(EndgameTablebase&&) = delete;
    auto operator=(EndgameTablebase&&) -> EndgameTablebase& = delete;
    auto isInTablebase(const Board& board) -> bool;
    auto probe(const Board& board, TablebaseResult& result) -> bool;
    auto getBestMove(const Board& board, Move& bestMove) -> bool;
    auto isWinning(const Board& board) -> bool;
    auto isLosing(const Board& board) -> bool;
    auto isDraw(const Board& board) -> bool;
    auto getAvailableTablebases() -> std::vector<std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
    auto boardToTablebaseKey(const Board& board) -> std::string;
    auto isValidEndgamePosition(const Board& board) -> bool;
    auto countPieces(const Board& board) -> int;
};

class EndgameKnowledge {
public:
    static auto evaluateKPK(const Board& board) -> int;
    static auto evaluateKRK(const Board& board) -> int;
    static auto evaluateKQK(const Board& board) -> int;
    static auto evaluateKBNK(const Board& board) -> int;
    static auto evaluateKBBK(const Board& board) -> int;
    static auto evaluateEndgame(const Board& board) -> int;

private:
    static auto getKingDistance(int king1, int king2) -> int;
    static auto isPawnPromoting(const Board& board, int pawnSquare) -> bool;
    static auto getPawnAdvancement(const Board& board, int pawnSquare) -> int;
};
