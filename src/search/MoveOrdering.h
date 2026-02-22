#pragma once

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "../core/MoveContent.h"

#include <algorithm>
#include <cstring>
#include <vector>

class MoveOrdering {
private:
    static constexpr int ZERO = 0;
    static constexpr int ONE = 1;
    static constexpr int TWO = 2;
    static constexpr int BOARD_SQUARES = 64;
    static constexpr int PIECE_TYPE_COUNT = 6;
    static constexpr int WHITE_INDEX = 0;
    static constexpr int BLACK_INDEX = 1;
    static constexpr int KILLER_MOVE_BONUS = 900000;
    static constexpr int FIRST_KILLER_BONUS = 950000;
    static constexpr int SECOND_KILLER_BONUS = 940000;
    static constexpr int COUNTER_MOVE_BONUS = 850000;
    static constexpr int PV_MOVE_BONUS = 10000000;
    static constexpr int CAPTURE_SCORE_BASE = 1000000;
    static constexpr int CAPTURE_VICTIM_SCALE = 100;
    static constexpr int PROMOTION_SCORE_BASE = 800000;
    static constexpr int HISTORY_MAX = 8192;
    static constexpr int HISTORY_DIVISOR = 2;
    static constexpr int PAWN_VALUE = 100;
    static constexpr int KNIGHT_VALUE = 320;
    static constexpr int BISHOP_VALUE = 330;
    static constexpr int ROOK_VALUE = 500;
    static constexpr int QUEEN_VALUE = 900;
    static constexpr int KING_VALUE = 20000;

    static int colorIndex(ChessPieceColor side) {
        return side == ChessPieceColor::WHITE ? WHITE_INDEX : BLACK_INDEX;
    }

    struct KillerMoves {
        MoveContent killers[TWO];
        int count = ZERO;

        void add(const MoveContent& move) {
            if (move.capture != ChessPieceType::NONE) {
                return;
            }

            if (count > ZERO && killers[ZERO] == move) {
                return;
            }

            if (count > ZERO) {
                killers[ONE] = killers[ZERO];
            }
            killers[ZERO] = move;
            count = std::min(TWO, count + ONE);
        }

        bool isKiller(const MoveContent& move) const {
            return count > ZERO && (killers[ZERO] == move || (count > ONE && killers[ONE] == move));
        }

        int getKillerScore(const MoveContent& move) const {
            if (count > ZERO && killers[ZERO] == move) {
                return FIRST_KILLER_BONUS;
            }
            if (count > ONE && killers[ONE] == move) {
                return SECOND_KILLER_BONUS;
            }
            return ZERO;
        }
    };
    KillerMoves killers[BOARD_SQUARES];
    int historyTable[TWO][BOARD_SQUARES][BOARD_SQUARES];
    int butterflyHistory[TWO][BOARD_SQUARES][BOARD_SQUARES];
    int counterMoveHistory[PIECE_TYPE_COUNT][BOARD_SQUARES][PIECE_TYPE_COUNT][BOARD_SQUARES];
    MoveContent counterMoves[TWO][BOARD_SQUARES];
    MoveContent pvMove;
    bool hasPV = false;

public:
    MoveOrdering() {
        clear();
    }

    void clear() {
        std::memset(killers, ZERO, sizeof(killers));
        std::memset(historyTable, ZERO, sizeof(historyTable));
        std::memset(butterflyHistory, ZERO, sizeof(butterflyHistory));
        std::memset(counterMoveHistory, ZERO, sizeof(counterMoveHistory));
        std::memset(counterMoves, ZERO, sizeof(counterMoves));
        hasPV = false;
    }

    void updateKillers(const MoveContent& move, int ply) {
        if (ply < BOARD_SQUARES && move.capture == ChessPieceType::NONE) {
            killers[ply].add(move);
        }
    }

    void updateHistory(const MoveContent& move, int depth, ChessPieceColor side, bool good) {
        int& histValue = historyTable[colorIndex(side)][move.src][move.dest];
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
        int& value = butterflyHistory[colorIndex(side)][move.src][move.dest];
        value += depth * depth;

        if (value > HISTORY_MAX) {
            for (int from = ZERO; from < BOARD_SQUARES; from++) {
                for (int to = ZERO; to < BOARD_SQUARES; to++) {
                    butterflyHistory[WHITE_INDEX][from][to] /= HISTORY_DIVISOR;
                    butterflyHistory[BLACK_INDEX][from][to] /= HISTORY_DIVISOR;
                }
            }
        }
    }

    void updateCounterMove(const MoveContent& previousMove, const MoveContent& move,
                           ChessPieceColor side) {
        if (previousMove.src >= ZERO && previousMove.src < BOARD_SQUARES &&
            previousMove.dest >= ZERO && previousMove.dest < BOARD_SQUARES) {
            counterMoves[colorIndex(side)][previousMove.dest] = move;
        }
    }

    void updateCounterMoveHistory(ChessPieceType piece, int from, ChessPieceType prevPiece,
                                  int prevTo, int depth, bool good) {
        if (piece == ChessPieceType::NONE || prevPiece == ChessPieceType::NONE) {
            return;
        }

        int pieceIdx = static_cast<int>(piece) - ONE;
        int prevPieceIdx = static_cast<int>(prevPiece) - ONE;

        if (pieceIdx >= ZERO && pieceIdx < PIECE_TYPE_COUNT && prevPieceIdx >= ZERO &&
            prevPieceIdx < PIECE_TYPE_COUNT) {
            int& value = counterMoveHistory[pieceIdx][from][prevPieceIdx][prevTo];
            int delta = depth * depth;

            if (good) {
                value += delta;
                value = std::min(value, HISTORY_MAX);
            } else {
                value -= delta;
                value = std::max(value, -HISTORY_MAX);
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
            return PV_MOVE_BONUS;
        }

        int score = ZERO;

        if (move.capture != ChessPieceType::NONE) {
            int victimValue = getPieceValue(move.capture);
            int attackerValue = getPieceValue(move.piece);
            score = CAPTURE_SCORE_BASE + (victimValue * CAPTURE_VICTIM_SCALE) - attackerValue;
        }

        if (move.promotion != ChessPieceType::NONE) {
            score += PROMOTION_SCORE_BASE + getPieceValue(move.promotion);
        }

        if (score == ZERO && ply < BOARD_SQUARES) {
            int killerScore = killers[ply].getKillerScore(move);
            if (killerScore > ZERO) {
                return killerScore;
            }
        }

        if (score == ZERO && previousMove != nullptr && previousMove->dest >= ZERO &&
            previousMove->dest < BOARD_SQUARES) {
            ChessPieceColor side = board.turn;
            const MoveContent& counter = counterMoves[colorIndex(side)][previousMove->dest];
            if (counter == move) {
                return COUNTER_MOVE_BONUS;
            }
        }

        if (score == ZERO) {
            ChessPieceColor side = board.turn;
            score = historyTable[colorIndex(side)][move.src][move.dest];

            if (previousMove != nullptr && previousMove->piece != ChessPieceType::NONE) {
                int pieceIdx = static_cast<int>(move.piece) - ONE;
                int prevPieceIdx = static_cast<int>(previousMove->piece) - ONE;

                if (pieceIdx >= ZERO && pieceIdx < PIECE_TYPE_COUNT && prevPieceIdx >= ZERO &&
                    prevPieceIdx < PIECE_TYPE_COUNT && previousMove->dest >= ZERO &&
                    previousMove->dest < BOARD_SQUARES) {
                    score +=
                        counterMoveHistory[pieceIdx][move.src][prevPieceIdx][previousMove->dest];
                }
            }
        }

        return score;
    }

    void orderMoves(std::vector<MoveContent>& moves, const Board& board, int ply,
                    const MoveContent* previousMove = nullptr) const {
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
        for (int c = ZERO; c < TWO; c++) {
            for (int from = ZERO; from < BOARD_SQUARES; from++) {
                for (int to = ZERO; to < BOARD_SQUARES; to++) {
                    historyTable[c][from][to] /= HISTORY_DIVISOR;
                    butterflyHistory[c][from][to] /= HISTORY_DIVISOR;
                }
            }
        }

        for (auto& p1 : counterMoveHistory) {
            for (auto& from : p1) {
                for (auto& p2 : from) {
                    for (int to = ZERO; to < BOARD_SQUARES; to++) {
                        p2[to] /= HISTORY_DIVISOR;
                    }
                }
            }
        }
    }

private:
    static int getPieceValue(ChessPieceType piece) {
        switch (piece) {
            case ChessPieceType::PAWN:
                return PAWN_VALUE;
            case ChessPieceType::KNIGHT:
                return KNIGHT_VALUE;
            case ChessPieceType::BISHOP:
                return BISHOP_VALUE;
            case ChessPieceType::ROOK:
                return ROOK_VALUE;
            case ChessPieceType::QUEEN:
                return QUEEN_VALUE;
            case ChessPieceType::KING:
                return KING_VALUE;
            default:
                return ZERO;
        }
    }
};
