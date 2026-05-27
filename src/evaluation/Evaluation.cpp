#include "Evaluation.h"
#include "../ai/EndgameTablebase.h"
#include "../core/BitboardMoves.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "Bitboard.h"
#include "EvaluationTuning.h"
#include "NNUE.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

using namespace EvaluationParams;

static std::atomic<bool> useNNUE{false};
thread_local bool useFastEval = false;

namespace {
constexpr int kPieceTypeCount = 6;
constexpr int kMirrorSquareOffset = NUM_SQUARES - 1;
} // namespace

namespace PieceSquareTables {

constexpr auto PAWN_MG = PAWN_TABLE;
constexpr auto KNIGHT_MG = KNIGHT_TABLE;
constexpr auto BISHOP_MG = BISHOP_TABLE;
constexpr auto ROOK_MG = ROOK_TABLE;
constexpr auto QUEEN_MG = QUEEN_TABLE;
constexpr auto KING_MG = KING_TABLE;

} // namespace PieceSquareTables

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {

    if (position < 0 || position >= NUM_SQUARES) {
        return 0;
    }

    static constexpr std::array<const std::array<int, NUM_SQUARES>*, kPieceTypeCount> tables = {
        &PieceSquareTables::PAWN_MG, &PieceSquareTables::KNIGHT_MG, &PieceSquareTables::BISHOP_MG,
        &PieceSquareTables::ROOK_MG, &PieceSquareTables::QUEEN_MG,  &PieceSquareTables::KING_MG};
    int idx = static_cast<int>(pieceType);
    int value = (idx >= 0 && idx < kPieceTypeCount) ? (*tables[idx])[position] : 0;
    if (color == ChessPieceColor::BLACK) {
        value = -value;
    }
    return value;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;
    for (int col = 0; col < BOARD_SIZE; col++) {
        int whitePawns = 0;
        int blackPawns = 0;
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = (row * BOARD_SIZE) + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                } else {
                    blackPawns++;
                }
            }
        }
        if (whitePawns > 1) {
            score -= DOUBLED_PAWN_PENALTY * (whitePawns - 1);
        }
        if (blackPawns > 1) {
            score += DOUBLED_PAWN_PENALTY * (blackPawns - 1);
        }
    }
    for (int col = 0; col < BOARD_SIZE; col++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = (row * BOARD_SIZE) + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(BOARD_SIZE - 1, col + 1);
                     adjCol++) {
                    if (adjCol == col) {
                        continue;
                    }
                    for (int adjRow = 0; adjRow < BOARD_SIZE; adjRow++) {
                        int adjPos = (adjRow * BOARD_SIZE) + adjCol;
                        if (board.squares[adjPos].piece.PieceType == ChessPieceType::PAWN &&
                            board.squares[adjPos].piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) {
                        break;
                    }
                }
                if (isolated) {
                    if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= ISOLATED_PAWN_PENALTY;
                    } else {
                        score += ISOLATED_PAWN_PENALTY;
                    }
                }
            }
        }
    }
    return score;
}

int evaluateMobility(const Board& board) {
    int mobilityScore = 0;
    const Bitboard occupancy = board.allPieces;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }

        const Bitboard ownPieces =
            piece.PieceColor == ChessPieceColor::WHITE ? board.whitePieces : board.blackPieces;
        int mobility = 0;
        int weight = 0;

        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT:
                mobility = popcount(KnightAttacks[i] & ~ownPieces);
                weight = 4;
                break;
            case ChessPieceType::BISHOP:
                mobility = popcount(bishopAttacks(i, occupancy) & ~ownPieces);
                weight = 3;
                break;
            case ChessPieceType::ROOK:
                mobility = popcount(rookAttacks(i, occupancy) & ~ownPieces);
                weight = 2;
                break;
            case ChessPieceType::QUEEN:
                mobility = popcount(queenAttacks(i, occupancy) & ~ownPieces);
                weight = 1;
                break;
            default:
                continue;
        }

        mobilityScore +=
            piece.PieceColor == ChessPieceColor::WHITE ? mobility * weight : -mobility * weight;
    }

    return mobilityScore;
}

int evaluateCenterControl(const Board& board, ChessPieceColor color) {
    int score = 0;
    int centerSquares[] = {27, 28, 35, 36};
    for (int square : centerSquares) {
        if (board.squares[square].piece.PieceType != ChessPieceType::NONE &&
            board.squares[square].piece.PieceColor == color) {
            score += CENTER_CONTROL_BONUS;
        }
    }
    return score;
}

int evaluateCenterControl(const Board& board) {
    return evaluateCenterControl(board, ChessPieceColor::WHITE) -
           evaluateCenterControl(board, ChessPieceColor::BLACK);
}

int evaluateKingSafety(const Board& board) {
    int safetyScore = 0;
    int whiteKingPos = -1;
    int blackKingPos = -1;
    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
            if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                whiteKingPos = i;
            } else {
                blackKingPos = i;
            }
        }
    }

    if (whiteKingPos != -1) {
        safetyScore += evaluateKingSafetyForColor(board, whiteKingPos, ChessPieceColor::WHITE);
    }
    if (blackKingPos != -1) {
        safetyScore -= evaluateKingSafetyForColor(board, blackKingPos, ChessPieceColor::BLACK);
    }

    return safetyScore;
}

int evaluateKingSafety(const Board& board, ChessPieceColor color) {
    int kingPos = -1;
    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING &&
            board.squares[i].piece.PieceColor == color) {
            kingPos = i;
            break;
        }
    }

    if (kingPos == -1) {
        return 0;
    }

    return evaluateKingSafetyForColor(board, kingPos, color);
}

int evaluateKingSafetyForColor(const Board& board, int kingPos, ChessPieceColor color) {
    int score = 0;
    int kingRow = kingPos / BOARD_SIZE;
    int kingCol = kingPos % BOARD_SIZE;
    int pawnShield = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int checkRow = kingRow + dr;
            int checkCol = kingCol + dc;
            if (checkRow >= 0 && checkRow < BOARD_SIZE && checkCol >= 0 && checkCol < BOARD_SIZE) {
                int checkSquare = (checkRow * BOARD_SIZE) + checkCol;
                const Piece& piece = board.squares[checkSquare].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
                    pawnShield++;
                }
            }
        }
    }
    score += pawnShield * KING_SAFETY_PAWN_SHIELD_BONUS;
    bool isOnOpenFile = true;
    for (int rank = 0; rank < BOARD_SIZE; rank++) {
        int square = (rank * BOARD_SIZE) + kingCol;
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            isOnOpenFile = false;
            break;
        }
    }
    if (isOnOpenFile) {
        score -= KING_SAFETY_OPEN_FILE_PENALTY;
    }

    bool isOnSemiOpenFile = true;
    for (int rank = 0; rank < BOARD_SIZE; rank++) {
        int square = (rank * BOARD_SIZE) + kingCol;
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
            isOnSemiOpenFile = false;
            break;
        }
    }
    if (isOnSemiOpenFile) {
        score -= KING_SAFETY_SEMI_OPEN_FILE_PENALTY;
    }

    return score;
}

int evaluatePassedPawns(const Board& board, ChessPieceColor color) {
    int score = 0;

    for (int col = 0; col < BOARD_SIZE; col++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = (row * BOARD_SIZE) + col;
            const Piece& piece = board.squares[pos].piece;

            if (piece.PieceType != ChessPieceType::PAWN || piece.PieceColor != color) {
                continue;
            }

            bool isPassed = true;

            if (color == ChessPieceColor::WHITE) {
                for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1);
                     checkCol++) {
                    for (int checkRow = row + 1; checkRow < BOARD_SIZE; checkRow++) {
                        int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                        if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                            board.squares[checkPos].piece.PieceColor == ChessPieceColor::BLACK) {
                            isPassed = false;
                            break;
                        }
                    }
                    if (!isPassed) {
                        break;
                    }
                }
                if (isPassed) {
                    score += ((row - 1) * PASSED_PAWN_RANK_SCALE) + PASSED_PAWN_BASE_BONUS;
                }
            } else {
                for (int checkCol = std::max(0, col - 1);
                     checkCol <= std::min(BOARD_SIZE - 1, col + 1); checkCol++) {
                    for (int checkRow = row - 1; checkRow >= 0; checkRow--) {
                        int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                        if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                            board.squares[checkPos].piece.PieceColor == ChessPieceColor::WHITE) {
                            isPassed = false;
                            break;
                        }
                    }
                    if (!isPassed) {
                        break;
                    }
                }
                if (isPassed) {
                    score += (((BOARD_SIZE - 2) - row) * PASSED_PAWN_RANK_SCALE) +
                             PASSED_PAWN_BASE_BONUS;
                }
            }
        }
    }

    return score;
}

int evaluatePassedPawns(const Board& board) {
    return evaluatePassedPawns(board, ChessPieceColor::WHITE) -
           evaluatePassedPawns(board, ChessPieceColor::BLACK);
}

int evaluateBishopPair(const Board& board) {
    int whiteBishops = 0;
    int blackBishops = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::BISHOP) {
            if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                whiteBishops++;
            } else {
                blackBishops++;
            }
        }
    }

    int score = 0;
    if (whiteBishops >= 2) {
        score += BISHOP_PAIR_BONUS;
    }
    if (blackBishops >= 2) {
        score -= BISHOP_PAIR_BONUS;
    }
    return score;
}

int evaluateRooksOnOpenFiles(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::ROOK) {
            int col = i % BOARD_SIZE;
            bool isOpenFile = true;
            bool isSemiOpen = true;

            for (int row = 0; row < BOARD_SIZE; row++) {
                int pos = (row * BOARD_SIZE) + col;
                if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                    isOpenFile = false;
                    if (board.squares[pos].piece.PieceColor == board.squares[i].piece.PieceColor) {
                        isSemiOpen = false;
                    }
                }
            }

            if (isOpenFile) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += ROOK_OPEN_FILE_BONUS;
                } else {
                    score -= ROOK_OPEN_FILE_BONUS;
                }
            } else if (isSemiOpen) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += ROOK_SEMI_OPEN_FILE_BONUS;
                } else {
                    score -= ROOK_SEMI_OPEN_FILE_BONUS;
                }
            }
        }
    }

    return score;
}

int evaluateEndgame(const Board& board) {
    int totalMaterial = 0;
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            totalMaterial += piece.PieceValue;
        }
    }

    int score = 0;
    if (totalMaterial < ENDGAME_MATERIAL_THRESHOLD) {
        for (int i = 0; i < NUM_SQUARES; i++) {
            if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
                int file = i % BOARD_SIZE;
                int rank = i / BOARD_SIZE;
                const int centerDistance =
                    static_cast<int>(std::max(std::abs(file - 3.5), std::abs(rank - 3.5)));

                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score +=
                        (KING_CENTER_DISTANCE_TARGET - centerDistance) * KING_CENTRALIZATION_SCALE;
                } else {
                    score -=
                        (KING_CENTER_DISTANCE_TARGET - centerDistance) * KING_CENTRALIZATION_SCALE;
                }
            }
        }
    }

    return score;
}

thread_local PawnHashTable pawnHashTable;

std::uint64_t computePawnHash(const Board& board) {
    std::uint64_t h = 1469598103934665603ULL;
    for (int sq = 0; sq < NUM_SQUARES; ++sq) {
        const Piece& p = board.squares[sq].piece;
        if (p.PieceType == ChessPieceType::PAWN) {
            const std::uint64_t pawnKey =
                static_cast<std::uint64_t>(sq + 1) |
                (static_cast<std::uint64_t>(p.PieceColor == ChessPieceColor::BLACK) << 8);
            h ^= pawnKey;
            h *= 1099511628211ULL;
        }
    }
    return h;
}

int evaluatePosition(const Board& board, int contempt) {

    if (useNNUE.load(std::memory_order_relaxed) && NNUE::globalEvaluator) {
        return NNUE::evaluate(board);
    }

    int mgScore = 0;
    int egScore = 0;
    int gamePhase = 0;
    for (int square = 0; square < NUM_SQUARES; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT:
            case ChessPieceType::BISHOP:
                gamePhase += 1;
                break;
            case ChessPieceType::ROOK:
                gamePhase += 2;
                break;
            case ChessPieceType::QUEEN:
                gamePhase += QUEEN_GAME_PHASE_INCREMENT;
                break;
            default:
                break;
        }
        int materialValue = piece.PieceValue;
        int adjustedSquare =
            (piece.PieceColor == ChessPieceColor::WHITE) ? square : kMirrorSquareOffset - square;

        int newMgPST = getTunedPST(piece.PieceType, adjustedSquare, false);
        int newEgPST = getTunedPST(piece.PieceType, adjustedSquare, true);

        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + newMgPST;
            egScore += materialValue + newEgPST;
        } else {
            mgScore -= materialValue + newMgPST;
            egScore -= materialValue + newEgPST;
        }
    }
    gamePhase = std::min(gamePhase, TOTAL_PHASE);

    if (ENABLE_PAWN_STRUCTURE) {
        std::uint64_t pawnKey = computePawnHash(board);
        int pawnMg = 0;
        int pawnEg = 0;
        if (const auto cachedPawnScore = pawnHashTable.probe(pawnKey)) {
            pawnMg = cachedPawnScore->mgScore;
            pawnEg = cachedPawnScore->egScore;
            mgScore += pawnMg;
            egScore += pawnEg;
        } else {
            int pawnScore = evaluatePawnStructure(board);
            int passedPawnScore = evaluatePassedPawns(board);
            pawnMg = pawnScore +
                     static_cast<int>(static_cast<float>(passedPawnScore) * PAWN_MG_PASSED_SCALE);
            pawnEg = static_cast<int>(static_cast<float>(pawnScore) * PAWN_EG_SCALE) +
                     static_cast<int>(static_cast<float>(passedPawnScore) * PASSED_PAWN_EG_SCALE);
            pawnHashTable.store(pawnKey, pawnMg, pawnEg);
            mgScore += pawnMg;
            egScore += pawnEg;
        }
    }

    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateMobility(board);
        mgScore += mobilityScore;
        egScore += static_cast<int>(static_cast<float>(mobilityScore) * MOBILITY_EG_SCALE);
    }

    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += static_cast<int>(static_cast<float>(kingSafetyScore) * KING_SAFETY_EG_SCALE);
    }

    const bool fastEval = useFastEval;

    int bishopPairScore = evaluateBishopPair(board);
    mgScore += bishopPairScore;
    egScore += static_cast<int>(static_cast<float>(bishopPairScore) * BISHOP_PAIR_EG_SCALE);
    int rookFileScore = evaluateRooksOnOpenFiles(board);
    mgScore += rookFileScore;
    egScore += rookFileScore;
    int tacticalSafetyScore = 0;
    if (!fastEval) {
        tacticalSafetyScore = evaluateTacticalSafety(board);
        mgScore += tacticalSafetyScore;
    }
    int endgameScore = 0;
    if (gamePhase < OPENING_ENDGAME_BOUNDARY_PHASE) {
        endgameScore = evaluateEndgame(board);
        egScore += endgameScore;
        int endgameKnowledgeScore = EndgameKnowledge::evaluateEndgame(board);
        egScore += endgameKnowledgeScore;
    }
    egScore += static_cast<int>(static_cast<float>(tacticalSafetyScore) * TACTICAL_SAFETY_EG_SCALE);
    if (!fastEval) {
        int hangingPiecesScore = evaluateHangingPieces(board);
        mgScore += hangingPiecesScore;
        egScore += hangingPiecesScore;
        int queenTrapScore = evaluateQueenTrapDanger(board);
        mgScore += queenTrapScore;
        egScore += static_cast<int>(static_cast<float>(queenTrapScore) * QUEEN_TRAP_EG_SCALE);
    }

    if (board.turn == ChessPieceColor::WHITE) {
        mgScore += TEMPO_BONUS;
        egScore += TEMPO_BONUS / 2;
    } else {
        mgScore -= TEMPO_BONUS;
        egScore -= TEMPO_BONUS / 2;
    }

    int finalScore = interpolatePhase(mgScore, egScore, gamePhase);

    if (contempt != 0) {
        finalScore += (board.turn == ChessPieceColor::WHITE) ? contempt : -contempt;
    }

    if (!fastEval) {
        logEvaluationComponents("Final Enhanced Score", finalScore);
    }
    return finalScore;
}

int evaluateHangingPieces(const Board& board, ChessPieceColor color) {
    int penalty = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }
        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) {
            continue;
        }
        bool isUnderAttack = false;
        bool isDefended = false;

        ChessPieceColor enemyColor =
            (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

        for (int j = 0; j < NUM_SQUARES; j++) {
            const Piece& enemyPiece = board.squares[j].piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE ||
                enemyPiece.PieceColor != enemyColor) {
                continue;
            }
            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }

        if (isUnderAttack) {
            for (int j = 0; j < NUM_SQUARES; j++) {
                const Piece& friendlyPiece = board.squares[j].piece;
                if (friendlyPiece.PieceType == ChessPieceType::NONE ||
                    friendlyPiece.PieceColor != color || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    isDefended = true;
                    break;
                }
            }

            if (!isDefended) {
                penalty +=
                    static_cast<int>(static_cast<float>(piece.PieceValue) * HANGING_PENALTY_RATIO);

                if (piece.PieceType == ChessPieceType::QUEEN) {
                    penalty += HANGING_QUEEN_PENALTY;
                }
            }
        }

        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;

            bool inEnemyTerritory =
                (color == ChessPieceColor::WHITE && row >= QUEEN_ENEMY_TERRITORY_WHITE_ROW) ||
                (color == ChessPieceColor::BLACK && row <= QUEEN_ENEMY_TERRITORY_BLACK_ROW);

            if (inEnemyTerritory) {
                int supportCount = 0;
                for (int j = 0; j < NUM_SQUARES; j++) {
                    const Piece& friendlyPiece = board.squares[j].piece;
                    if (friendlyPiece.PieceType == ChessPieceType::NONE ||
                        friendlyPiece.PieceColor != color || j == i ||
                        friendlyPiece.PieceType == ChessPieceType::PAWN) {
                        continue;
                    }
                    if (canPieceAttackSquare(board, j, i)) {
                        supportCount++;
                    }
                }

                if (supportCount == 0) {
                    penalty += QUEEN_UNSUPPORTED_PENALTY;
                }

                int nearbyEnemies = 0;
                for (int dr = -NEARBY_ENEMY_RADIUS; dr <= NEARBY_ENEMY_RADIUS; dr++) {
                    for (int dc = -NEARBY_ENEMY_RADIUS; dc <= NEARBY_ENEMY_RADIUS; dc++) {
                        int checkRow = row + dr;
                        int checkCol = col + dc;
                        if (checkRow >= 0 && checkRow < BOARD_SIZE && checkCol >= 0 &&
                            checkCol < BOARD_SIZE) {
                            int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                            if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[checkPos].piece.PieceColor == enemyColor) {
                                nearbyEnemies++;
                            }
                        }
                    }
                }

                if (nearbyEnemies >= NEARBY_ENEMY_THRESHOLD) {
                    penalty += QUEEN_CROWDED_PENALTY;
                }
            }
        }

        if (piece.PieceType == ChessPieceType::QUEEN || piece.PieceType == ChessPieceType::ROOK ||
            piece.PieceType == ChessPieceType::BISHOP) {

            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;
            bool isOnEdge = (row == 0 || row == 7 || col == 0 || col == 7);
            bool isInCorner = ((row == 0 || row == 7) && (col == 0 || col == 7));

            if (isInCorner && piece.PieceType == ChessPieceType::QUEEN) {
                penalty += QUEEN_CORNER_TRAP_PENALTY;
            } else if (isOnEdge && piece.PieceType == ChessPieceType::QUEEN) {
                penalty += QUEEN_EDGE_TRAP_PENALTY;
            }
        }
    }

    return penalty;
}

int evaluateHangingPieces(const Board& board) {
    return evaluateHangingPieces(board, ChessPieceColor::BLACK) -
           evaluateHangingPieces(board, ChessPieceColor::WHITE);
}

bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos) {
    if (piecePos < 0 || piecePos >= NUM_SQUARES || targetPos < 0 || targetPos >= NUM_SQUARES) {
        return false;
    }

    const Piece& piece = board.squares[piecePos].piece;
    if (piece.PieceType == ChessPieceType::NONE) {
        return false;
    }

    Bitboard attacks = 0;
    switch (piece.PieceType) {
        case ChessPieceType::PAWN:
            attacks = pawnAttacks(piece.PieceColor, piecePos);
            break;
        case ChessPieceType::KNIGHT:
            attacks = KnightAttacks[piecePos];
            break;
        case ChessPieceType::BISHOP:
            attacks = bishopAttacks(piecePos, board.allPieces);
            break;
        case ChessPieceType::ROOK:
            attacks = rookAttacks(piecePos, board.allPieces);
            break;
        case ChessPieceType::QUEEN:
            attacks = queenAttacks(piecePos, board.allPieces);
            break;
        case ChessPieceType::KING:
            attacks = KingAttacks[piecePos];
            break;
        default:
            return false;
    }

    return get_bit(attacks, targetPos);
}

int evaluateQueenTrapDanger(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::QUEEN) {
            continue;
        }
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;
        int escapeSquares = 0;

        int directions[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                {0, 1},   {1, -1}, {1, 0},  {1, 1}};

        for (auto& direction : directions) {
            int newRow = row + direction[0];
            int newCol = col + direction[1];

            while (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
                int newPos = (newRow * BOARD_SIZE) + newCol;

                if (board.squares[newPos].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[newPos].piece.PieceColor == piece.PieceColor) {
                    break;
                }

                bool squareIsSafe = true;
                ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                                 ? ChessPieceColor::BLACK
                                                 : ChessPieceColor::WHITE;

                for (int j = 0; j < NUM_SQUARES; j++) {
                    const Piece& enemyPiece = board.squares[j].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE ||
                        enemyPiece.PieceColor != enemyColor) {
                        continue;
                    }
                    if (canPieceAttackSquare(board, j, newPos)) {
                        squareIsSafe = false;
                        break;
                    }
                }

                if (squareIsSafe) {
                    escapeSquares++;
                }

                if (board.squares[newPos].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[newPos].piece.PieceColor != piece.PieceColor) {
                    break;
                }

                newRow += direction[0];
                newCol += direction[1];

                if (abs(newRow - row) > QUEEN_ESCAPE_SEARCH_RADIUS ||
                    abs(newCol - col) > QUEEN_ESCAPE_SEARCH_RADIUS) {
                    break;
                }
            }
        }

        if (escapeSquares == 0) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= QUEEN_TRAP_NO_ESCAPE_PENALTY;
            } else {
                score += QUEEN_TRAP_NO_ESCAPE_PENALTY;
            }
        } else if (escapeSquares <= QUEEN_TRAP_LOW_ESCAPE_THRESHOLD) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= QUEEN_TRAP_LOW_ESCAPE_PENALTY;
            } else {
                score += QUEEN_TRAP_LOW_ESCAPE_PENALTY;
            }
        } else if (escapeSquares <= QUEEN_TRAP_MEDIUM_ESCAPE_THRESHOLD) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= QUEEN_TRAP_MEDIUM_ESCAPE_PENALTY;
            } else {
                score += QUEEN_TRAP_MEDIUM_ESCAPE_PENALTY;
            }
        }

        bool isOnEdge = (row == 0 || row == 7 || col == 0 || col == 7);
        bool isInCorner = ((row == 0 || row == 7) && (col == 0 || col == 7));

        if (isInCorner && escapeSquares <= QUEEN_TRAP_CORNER_ESCAPE_THRESHOLD) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= QUEEN_TRAP_CORNER_PENALTY;
            } else {
                score += QUEEN_TRAP_CORNER_PENALTY;
            }
        } else if (isOnEdge && escapeSquares <= QUEEN_TRAP_EDGE_ESCAPE_THRESHOLD) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= QUEEN_TRAP_EDGE_PENALTY;
            } else {
                score += QUEEN_TRAP_EDGE_PENALTY;
            }
        }
    }

    return score;
}

int evaluateTacticalSafety(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;
            int attackers = 0;
            int defenders = 0;

            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < NUM_SQUARES; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }

            if (attackers > defenders) {
                int dangerLevel = (attackers - defenders) * DANGER_LEVEL_SCALE;
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= dangerLevel;
                } else {
                    score += dangerLevel;
                }

                if (attackers > 0) {
                    int weakestAttacker = 10000;
                    for (int j = 0; j < NUM_SQUARES; j++) {
                        const Piece& attacker = board.squares[j].piece;
                        if (attacker.PieceColor == enemyColor &&
                            canPieceAttackSquare(board, j, i)) {
                            weakestAttacker = std::min<int>(attacker.PieceValue, weakestAttacker);
                        }
                    }

                    if (weakestAttacker < piece.PieceValue) {
                        if (piece.PieceColor == ChessPieceColor::WHITE) {
                            score -= QUEEN_TACTICAL_PENALTY;
                        } else {
                            score += QUEEN_TACTICAL_PENALTY;
                        }
                    }
                }
            }

            bool nearEnemyKing = false;
            for (int j = 0; j < NUM_SQUARES; j++) {
                if (board.squares[j].piece.PieceType == ChessPieceType::KING &&
                    board.squares[j].piece.PieceColor == enemyColor) {
                    int kingRow = j / BOARD_SIZE;
                    int kingCol = j % BOARD_SIZE;
                    int distance = (abs(row - kingRow) > abs(col - kingCol)) ? abs(row - kingRow)
                                                                             : abs(col - kingCol);
                    if (distance <= NEAR_ENEMY_KING_DISTANCE) {
                        nearEnemyKing = true;
                        break;
                    }
                }
            }

            if (nearEnemyKing && defenders < NEAR_ENEMY_KING_LOW_DEFENDER_THRESHOLD) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= NEAR_ENEMY_KING_PENALTY;
                } else {
                    score += NEAR_ENEMY_KING_PENALTY;
                }
            }
        }

        else if (piece.PieceValue >= HIGH_VALUE_PIECE_THRESHOLD) {
            int attackers = 0;
            int defenders = 0;
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < NUM_SQUARES; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }

            if (attackers > defenders) {
                int penalty = (attackers - defenders) * piece.PieceValue / PENALTY_DIVISOR;
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= penalty;
                } else {
                    score += penalty;
                }
            }
        }
    }

    return score;
}

void setNNUEEnabled(bool enabled) {
    useNNUE.store(enabled, std::memory_order_relaxed);
}

void setFastEvaluationMode(bool enabled) {
    useFastEval = enabled;
}

bool isFastEvaluationMode() {
    return useFastEval;
}
