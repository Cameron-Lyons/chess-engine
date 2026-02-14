#include "EvaluationTuning.h"

#include <iostream>

namespace EvaluationParams {

const int TUNED_PAWN_MG[64] = {0,   0,   0,  0,   0,   0,   0,   0,  78,  83,  86, 73,  102,
                               82,  85,  90, 7,   29,  21,  44,  40, 31,  44,  7,  -17, 16,
                               -2,  15,  14, 0,   15,  -13, -26, 3,  10,  20,  16, 2,   0,
                               -23, -22, 9,  5,   -11, -10, -2,  3,  -19, -31, 8,  -7,  -37,
                               -36, -14, 3,  -31, 0,   0,   0,   0,  0,   0,   0,  0};

const int TUNED_PAWN_EG[64] = {
    0,  0,  0,  0,  0,  0, 0,  0,  178, 173, 158, 134, 147, 132, 165, 187, 94, 100, 85, 67, 56, 53,
    82, 84, 32, 24, 13, 5, -2, 4,  17,  17,  13,  9,   -3,  -7,  -7,  -8,  3,  -1,  4,  7,  -6, 1,
    0,  -5, -1, -8, 13, 8, 8,  10, 13,  0,   2,   -7,  0,   0,   0,   0,   0,  0,   0,  0};

const int TUNED_KNIGHT_MG[64] = {-167, -89, -34, -49, 61,   -97, -15, -107, -73, -41, 72,  36,  23,
                                 62,   7,   -17, -47, 60,   37,  65,  84,   129, 73,  44,  -9,  17,
                                 19,   53,  37,  69,  18,   22,  -13, 4,    16,  13,  28,  19,  21,
                                 -8,   -23, -9,  12,  10,   19,  17,  25,   -16, -29, -53, -12, -3,
                                 -1,   18,  -14, -19, -105, -21, -58, -33,  -17, -28, -19, -23};

const int TUNED_KNIGHT_EG[64] = {-58, -38, -13, -28, -31, -27, -63, -99, -25, -8,  -25, -2,  -9,
                                 -25, -24, -52, -24, -20, 10,  9,   -1,  -9,  -19, -41, -17, 3,
                                 22,  22,  22,  11,  8,   -18, -18, -6,  16,  25,  16,  17,  4,
                                 -18, -23, -3,  -1,  15,  10,  -3,  -20, -22, -42, -20, -10, -5,
                                 -2,  -20, -23, -44, -29, -51, -23, -15, -22, -18, -50, -64};

const int TUNED_BISHOP_MG[64] = {-29, 4,  -82, -37, -25, -42, 7,   -8,  -26, 16,  -18, -13, 30,
                                 59,  18, -47, -16, 37,  43,  40,  35,  50,  37,  -2,  -4,  5,
                                 19,  50, 37,  37,  7,   -2,  -6,  13,  13,  26,  34,  12,  10,
                                 4,   0,  15,  15,  15,  14,  27,  18,  10,  4,   15,  16,  0,
                                 7,   21, 33,  1,   -33, -3,  -14, -21, -13, -12, -39, -21};

const int TUNED_BISHOP_EG[64] = {-14, -21, -11, -8,  -7,  -9, -17, -24, -8,  -4,  7,   -12, -3,
                                 -13, -4,  -14, 2,   -8,  0,  -1,  -2,  6,   0,   4,   -3,  9,
                                 12,  9,   14,  10,  3,   2,  -6,  3,   13,  19,  7,   10,  -3,
                                 -9,  -12, -3,  8,   10,  13, 3,   -7,  -15, -14, -18, -7,  -1,
                                 4,   -9,  -15, -27, -23, -9, -23, -5,  -9,  -16, -5,  -17};

const int TUNED_ROOK_MG[64] = {32,  42,  32,  51,  63,  9,   31,  43,  27,  32,  58,  62,  80,
                               67,  26,  44,  -5,  19,  26,  36,  17,  45,  61,  16,  -24, -11,
                               7,   26,  24,  35,  -8,  -20, -36, -26, -12, -1,  9,   -7,  6,
                               -23, -45, -25, -16, -17, 3,   0,   -5,  -33, -44, -16, -20, -9,
                               -1,  11,  -6,  -71, -19, -13, 1,   17,  16,  7,   -37, -26};

const int TUNED_ROOK_EG[64] = {13, 10, 18, 15, 12, 12, 8,   5,   11, 13, 13, 11, -3, 3,   8,  3,
                               7,  7,  7,  5,  4,  -3, -5,  -3,  4,  3,  13, 1,  2,  1,   -1, 2,
                               3,  5,  8,  4,  -5, -6, -8,  -11, -4, 0,  -5, -1, -7, -12, -8, -16,
                               -6, -6, 0,  2,  -9, -9, -11, -3,  -9, 2,  3,  -1, -5, -13, 4,  -20};

const int TUNED_QUEEN_MG[64] = {-28, 0,   29, 12,  59,  44,  43, 45,  -24, -39, -5,  1,   -16,
                                57,  28,  54, -13, -17, 7,   8,  29,  56,  47,  57,  -27, -27,
                                -16, -16, -1, 17,  -2,  1,   -9, -26, -9,  -10, -2,  -4,  3,
                                -3,  -14, 2,  -11, -2,  -5,  2,  14,  5,   -35, -8,  11,  2,
                                8,   15,  -3, 1,   -1,  -18, -9, 10,  -15, -25, -31, -50};

const int TUNED_QUEEN_EG[64] = {-9,  22,  22,  27,  27,  19,  10,  20,  -17, 20,  32,  41,  58,
                                25,  30,  0,   -20, 6,   9,   49,  47,  35,  19,  9,   3,   22,
                                24,  45,  57,  40,  57,  36,  -18, 28,  19,  47,  31,  34,  39,
                                23,  -16, -27, 15,  6,   9,   17,  10,  5,   -22, -23, -30, -16,
                                -16, -23, -36, -32, -33, -28, -22, -43, -5,  -32, -20, -41};

const int TUNED_KING_MG[64] = {-65, 23,  16,  -15, -56, -34, 2,   13,  29,  -1,  -20, -7,  -8,
                               -4,  -38, -29, -9,  24,  2,   -16, -20, 6,   22,  -22, -17, -20,
                               -12, -27, -30, -25, -14, -36, -49, -1,  -27, -39, -46, -44, -33,
                               -51, -14, -14, -22, -46, -44, -30, -15, -27, 1,   7,   -8,  -64,
                               -43, -16, 9,   8,   -15, 36,  12,  -54, 8,   -28, 24,  14};

const int TUNED_KING_EG[64] = {-74, -35, -18, -18, -11, 15,  4,   -17, -12, 17,  14,  17, 17,
                               38,  23,  11,  10,  17,  23,  15,  20,  45,  44,  13,  -8, 22,
                               24,  27,  26,  33,  26,  3,   -18, -4,  21,  24,  27,  23, 9,
                               -11, -19, -3,  11,  21,  23,  16,  7,   -9,  -27, -11, 4,  13,
                               14,  4,   -5,  -17, -53, -34, -21, -11, -28, -14, -24, -43};

int getMaterialValue(ChessPieceType piece) {
    constexpr int values[] = {PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE,
                              ROOK_VALUE, QUEEN_VALUE,  KING_VALUE, 0};
    return values[static_cast<int>(piece)];
}

int getTunedPST(ChessPieceType piece, int square, bool isEndgame) {
    static const int* const mgTables[] = {TUNED_PAWN_MG,   TUNED_KNIGHT_MG, TUNED_BISHOP_MG,
                                          TUNED_ROOK_MG,   TUNED_QUEEN_MG,  TUNED_KING_MG};
    static const int* const egTables[] = {TUNED_PAWN_EG,   TUNED_KNIGHT_EG, TUNED_BISHOP_EG,
                                          TUNED_ROOK_EG,   TUNED_QUEEN_EG,  TUNED_KING_EG};
    int idx = static_cast<int>(piece);
    if (idx < 0 || idx >= 6) return 0;
    return isEndgame ? egTables[idx][square] : mgTables[idx][square];
}

int interpolatePhase(int mgScore, int egScore, int phase) {

    return ((mgScore * phase) + (egScore * (TOTAL_PHASE - phase))) / TOTAL_PHASE;
}

void logEvaluationComponents(const char* component, int value) {
    (void)component;
    (void)value;
#ifdef EVALUATION_DEBUG
    std::cout << component << ": " << value << std::endl;
#endif
}

} // namespace EvaluationParams

#include "../evaluation/Evaluation.h"
#include <cmath>
#include <fstream>
#include <sstream>

double TexelTuner::sigmoid(double eval) const {
    return 1.0 / (1.0 + std::pow(10.0, -scalingK * eval / 400.0));
}

int TexelTuner::evaluateWithParams(const Board& board, const std::vector<int>& p) const {
    int score = 0;
    int paramIdx = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;

        int matVal = 0;
        int ptIdx = static_cast<int>(piece.PieceType) - 1;
        if (ptIdx >= 0 && ptIdx < 5 && ptIdx < static_cast<int>(p.size())) {
            matVal = p[ptIdx];
        }

        int pstVal = 0;
        int pstBase = 5;
        int pstIdx = pstBase + ptIdx * 64 + sq;
        if (pstIdx < static_cast<int>(p.size())) {
            pstVal = p[pstIdx];
        }

        int total = matVal + pstVal;
        if (piece.PieceColor == ChessPieceColor::WHITE)
            score += total;
        else
            score -= total;
    }

    (void)paramIdx;
    return (board.turn == ChessPieceColor::WHITE) ? score : -score;
}

double TexelTuner::computeError(const std::vector<int>& p) const {
    double totalError = 0.0;
    for (const auto& pos : positions) {
        Board board;
        board.InitializeFromFEN(pos.fen);
        int eval = evaluateWithParams(board, p);
        double predicted = sigmoid(eval);
        double diff = pos.result - predicted;
        totalError += diff * diff;
    }
    return positions.empty() ? 0.0 : totalError / positions.size();
}

void TexelTuner::findOptimalK() {
    double bestK = 1.0;
    double bestError = 1e9;
    for (double k = 0.5; k <= 2.0; k += 0.01) {
        scalingK = k;
        double err = computeError(params);
        if (err < bestError) {
            bestError = err;
            bestK = k;
        }
    }
    scalingK = bestK;

    for (double delta = 0.005; delta >= 0.0001; delta /= 2.0) {
        bool improved = true;
        while (improved) {
            improved = false;
            scalingK += delta;
            double err = computeError(params);
            if (err < bestError) {
                bestError = err;
                improved = true;
            } else {
                scalingK -= 2 * delta;
                err = computeError(params);
                if (err < bestError) {
                    bestError = err;
                    improved = true;
                } else {
                    scalingK += delta;
                }
            }
        }
    }
}

bool TexelTuner::loadPositions(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto sep = line.rfind(';');
        if (sep == std::string::npos) sep = line.rfind(',');
        if (sep == std::string::npos) continue;

        TuningPosition pos;
        pos.fen = line.substr(0, sep);
        std::string resultStr = line.substr(sep + 1);

        while (!resultStr.empty() && resultStr[0] == ' ')
            resultStr.erase(resultStr.begin());

        if (resultStr == "1-0" || resultStr == "1.0")
            pos.result = 1.0;
        else if (resultStr == "0-1" || resultStr == "0.0")
            pos.result = 0.0;
        else if (resultStr == "1/2-1/2" || resultStr == "0.5")
            pos.result = 0.5;
        else {
            try { pos.result = std::stod(resultStr); }
            catch (...) { continue; }
        }

        positions.push_back(pos);
    }

    return !positions.empty();
}

void TexelTuner::initParams() {
    params.clear();
    params.push_back(EvaluationParams::PAWN_VALUE);
    params.push_back(EvaluationParams::KNIGHT_VALUE);
    params.push_back(EvaluationParams::BISHOP_VALUE);
    params.push_back(EvaluationParams::ROOK_VALUE);
    params.push_back(EvaluationParams::QUEEN_VALUE);

    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_PAWN_MG[sq]);
    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_KNIGHT_MG[sq]);
    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_BISHOP_MG[sq]);
    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_ROOK_MG[sq]);
    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_QUEEN_MG[sq]);
    for (int sq = 0; sq < 64; ++sq) params.push_back(EvaluationParams::TUNED_KING_MG[sq]);
}

void TexelTuner::optimize(int iterations) {
    initParams();
    findOptimalK();

    std::cout << "Optimal K: " << scalingK << std::endl;
    std::cout << "Initial error: " << computeError(params) << std::endl;

    for (int iter = 0; iter < iterations; ++iter) {
        bool anyImproved = false;
        for (size_t i = 0; i < params.size(); ++i) {
            double bestError = computeError(params);

            params[i] += 1;
            double errorUp = computeError(params);
            if (errorUp < bestError) {
                anyImproved = true;
                continue;
            }

            params[i] -= 2;
            double errorDown = computeError(params);
            if (errorDown < bestError) {
                anyImproved = true;
                continue;
            }

            params[i] += 1;
        }

        double currentError = computeError(params);
        std::cout << "Iteration " << (iter + 1) << " error: " << currentError << std::endl;

        if (!anyImproved) {
            std::cout << "Converged at iteration " << (iter + 1) << std::endl;
            break;
        }
    }
}

void TexelTuner::exportParams(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) return;

    file << "constexpr int PAWN_VALUE = " << params[0] << ";\n";
    file << "constexpr int KNIGHT_VALUE = " << params[1] << ";\n";
    file << "constexpr int BISHOP_VALUE = " << params[2] << ";\n";
    file << "constexpr int ROOK_VALUE = " << params[3] << ";\n";
    file << "constexpr int QUEEN_VALUE = " << params[4] << ";\n";

    const char* names[] = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};
    int base = 5;
    for (int p = 0; p < 6; ++p) {
        file << "\nconst int TUNED_" << names[p] << "_MG[64] = {\n    ";
        for (int sq = 0; sq < 64; ++sq) {
            file << params[base + p * 64 + sq];
            if (sq < 63) file << ", ";
            if ((sq + 1) % 8 == 0 && sq < 63) file << "\n    ";
        }
        file << "\n};\n";
    }
}