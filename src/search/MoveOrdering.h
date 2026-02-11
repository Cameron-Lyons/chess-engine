#pragma once

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "../core/MoveContent.h"

#include <algorithm>
#include <cstring>
#include <vector>

class MoveOrdering {
private:
    static constexpr int KILLER_MOVE_BONUS = 900000;
    static constexpr int FIRST_KILLER_BONUS = 950000;
    static constexpr int SECOND_KILLER_BONUS = 940000;
    static constexpr int COUNTER_MOVE_BONUS = 850000;
    static constexpr int HISTORY_MAX = 8192;
    static constexpr int HISTORY_DIVISOR = 2;

    struct KillerMoves {
        MoveContent killers[2];
        int count = 0;

        void add(const MoveContent& move) {
            if (move.capture != ChessPieceType::NONE)
                return;

            if (count > 0 && killers[0] == move)
                return;

            if (count > 0) {
                killers[1] = killers[0];
            }
            killers[0] = move;
            count = std::min(2, count + 1);
        }

        bool isKiller(const MoveContent& move) const {
            return count > 0 && (killers[0] == move || (count > 1 && killers[1] == move));
        }

        int getKillerScore(const MoveContent& move) const {
            if (count > 0 && killers[0] == move)
                return FIRST_KILLER_BONUS;
            if (count > 1 && killers[1] == move)
                return SECOND_KILLER_BONUS;
            return 0;
        }
    };

    KillerMoves killers[64];

    int historyTable[2][64][64];
    int butterflyHistory[2][64][64];
    int counterMoveHistory[6][64][6][64];

    MoveContent counterMoves[2][64];

    MoveContent pvMove;
    bool hasPV = false;

public:
    MoveOrdering() {
        clear();
    }

    void clear() {
        std::memset(killers, 0, sizeof(killers));
        std::memset(historyTable, 0, sizeof(historyTable));
        std::memset(butterflyHistory, 0, sizeof(butterflyHistory));
        std::memset(counterMoveHistory, 0, sizeof(counterMoveHistory));
        std::memset(counterMoves, 0, sizeof(counterMoves));
        hasPV = false;
    }

    void updateKillers(const MoveContent& move, int ply) {
        if (ply < 64 && move.capture == ChessPieceType::NONE) {
            killers[ply].add(move);
        }
    }

    void updateHistory(const MoveContent& move, int depth, ChessPieceColor side, bool good) {
        int& histValue = historyTable[side == ChessPieceColor::WHITE ? 0 : 1][move.src][move.dest];
        int bonus = depth * depth;

        if (good) {
            histValue += bonus;
            if (histValue > HISTORY_MAX) {
                histValue /= HISTORY_DIVISOR;
            }
        } else {
            histValue -= bonus;
            if (histValue < -HISTORY_MAX) {
                histValue /= HISTORY_DIVISOR;
            }
        }
    }

    void updateButterflyHistory(const MoveContent& move, int depth, ChessPieceColor side) {
        int& value = butterflyHistory[side == ChessPieceColor::WHITE ? 0 : 1][move.src][move.dest];
        value += depth * depth;

        if (value > HISTORY_MAX) {
            for (int from = 0; from < 64; from++) {
                for (int to = 0; to < 64; to++) {
                    butterflyHistory[0][from][to] /= 2;
                    butterflyHistory[1][from][to] /= 2;
                }
            }
        }
    }

    void updateCounterMove(const MoveContent& previousMove, const MoveContent& move,
                           ChessPieceColor side) {
        if (previousMove.src >= 0 && previousMove.src < 64 && previousMove.dest >= 0 &&
            previousMove.dest < 64) {
            counterMoves[side == ChessPieceColor::WHITE ? 0 : 1][previousMove.dest] = move;
        }
    }

    void updateCounterMoveHistory(ChessPieceType piece, int from, ChessPieceType prevPiece,
                                  int prevTo, int depth, bool good) {
        if (piece == ChessPieceType::NONE || prevPiece == ChessPieceType::NONE)
            return;

        int pieceIdx = static_cast<int>(piece) - 1;
        int prevPieceIdx = static_cast<int>(prevPiece) - 1;

        if (pieceIdx >= 0 && pieceIdx < 6 && prevPieceIdx >= 0 && prevPieceIdx < 6) {
            int& value = counterMoveHistory[pieceIdx][from][prevPieceIdx][prevTo];
            int delta = depth * depth;

            if (good) {
                value += delta;
                if (value > HISTORY_MAX)
                    value = HISTORY_MAX;
            } else {
                value -= delta;
                if (value < -HISTORY_MAX)
                    value = -HISTORY_MAX;
            }
        }
    }

    void setPVMove(const MoveContent& move) {
        pvMove = move;
        hasPV = true;
    }

    void clearPVMove() {
        hasPV = false;
    }

    int scoreMove(const MoveContent& move, const Board& board, int ply,
                  const MoveContent* previousMove = nullptr) const {
        if (hasPV && move == pvMove) {
            return 10000000;
        }

        int score = 0;

        if (move.capture != ChessPieceType::NONE) {
            int victimValue = getPieceValue(move.capture);
            int attackerValue = getPieceValue(move.piece);
            score = 1000000 + victimValue * 100 - attackerValue;
        }

        if (move.promotion != ChessPieceType::NONE) {
            score += 800000 + getPieceValue(move.promotion);
        }

        if (score == 0 && ply < 64) {
            int killerScore = killers[ply].getKillerScore(move);
            if (killerScore > 0) {
                return killerScore;
            }
        }

        if (score == 0 && previousMove != nullptr && previousMove->dest >= 0 &&
            previousMove->dest < 64) {
            ChessPieceColor side = board.turn;
            const MoveContent& counter =
                counterMoves[side == ChessPieceColor::WHITE ? 0 : 1][previousMove->dest];
            if (counter == move) {
                return COUNTER_MOVE_BONUS;
            }
        }

        if (score == 0) {
            ChessPieceColor side = board.turn;
            score = historyTable[side == ChessPieceColor::WHITE ? 0 : 1][move.src][move.dest];

            if (previousMove != nullptr && previousMove->piece != ChessPieceType::NONE) {
                int pieceIdx = static_cast<int>(move.piece) - 1;
                int prevPieceIdx = static_cast<int>(previousMove->piece) - 1;

                if (pieceIdx >= 0 && pieceIdx < 6 && prevPieceIdx >= 0 && prevPieceIdx < 6 &&
                    previousMove->dest >= 0 && previousMove->dest < 64) {
                    score +=
                        counterMoveHistory[pieceIdx][move.src][prevPieceIdx][previousMove->dest];
                }
            }
        }

        return score;
    }

    void orderMoves(std::vector<MoveContent>& moves, const Board& board, int ply,
                    const MoveContent* previousMove = nullptr) {
        struct ScoredMove {
            MoveContent move;
            int score;
        };

        std::vector<ScoredMove> scoredMoves;
        scoredMoves.reserve(moves.size());

        for (const auto& move : moves) {
            int score = scoreMove(move, board, ply, previousMove);
            scoredMoves.push_back({move, score});
        }

        std::stable_sort(
            scoredMoves.begin(), scoredMoves.end(),
            [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

        moves.clear();
        for (const auto& sm : scoredMoves) {
            moves.push_back(sm.move);
        }
    }

    void ageHistory() {
        for (int c = 0; c < 2; c++) {
            for (int from = 0; from < 64; from++) {
                for (int to = 0; to < 64; to++) {
                    historyTable[c][from][to] /= 2;
                    butterflyHistory[c][from][to] /= 2;
                }
            }
        }

        for (int p1 = 0; p1 < 6; p1++) {
            for (int from = 0; from < 64; from++) {
                for (int p2 = 0; p2 < 6; p2++) {
                    for (int to = 0; to < 64; to++) {
                        counterMoveHistory[p1][from][p2][to] /= 2;
                    }
                }
            }
        }
    }

private:
    static int getPieceValue(ChessPieceType piece) {
        switch (piece) {
            case ChessPieceType::PAWN:
                return 100;
            case ChessPieceType::KNIGHT:
                return 320;
            case ChessPieceType::BISHOP:
                return 330;
            case ChessPieceType::ROOK:
                return 500;
            case ChessPieceType::QUEEN:
                return 900;
            case ChessPieceType::KING:
                return 20000;
            default:
                return 0;
        }
    }
};