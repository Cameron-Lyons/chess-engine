#include "EnhancedSearch.h"
#include "../core/BitboardMoves.h"
#include "../core/MoveContent.h"
#include "../evaluation/Evaluation.h"
#include "../evaluation/EvaluationEnhanced.h"
#include "../utils/Profiler.h"
#include "MoveOrdering.h"
#include "SearchEnhancements.h"
#include "TranspositionTableV2.h"
#include "ValidMoves.h"

#include <algorithm>
#include <chrono>
#include <limits>

using namespace TTv2;

namespace EnhancedSearch {

static SearchEnhancements enhancements;
static MoveOrdering moveOrdering;
static SearchEnhancements::SearchInfo searchInfo;
static TranspositionTableV2 tt(128);

void GenerateAllMoves(Board& board, std::vector<MoveContent>& moves) {
    moves.clear();
    ChessPieceColor color = board.turn;

    for (int from = 0; from < 64; from++) {
        if (board.squares[from].piece.PieceType == ChessPieceType::NONE)
            continue;
        if (board.squares[from].piece.PieceColor != color)
            continue;

        for (int to = 0; to < 64; to++) {
            if (IsMoveLegal(board, from, to)) {
                MoveContent move;
                move.src = from;
                move.dest = to;
                move.piece = board.squares[from].piece.PieceType;
                move.capture = board.squares[to].piece.PieceType;
                move.promotion = ChessPieceType::NONE;

                if (move.piece == ChessPieceType::PAWN && (to / 8 == 0 || to / 8 == 7)) {
                    move.promotion = ChessPieceType::QUEEN;
                }

                moves.push_back(move);
            }
        }
    }
}

void GenerateCaptures(Board& board, std::vector<MoveContent>& moves) {
    GenerateAllMoves(board, moves);
    moves.erase(
        std::remove_if(moves.begin(), moves.end(),
                       [](const MoveContent& m) { return m.capture == ChessPieceType::NONE; }),
        moves.end());
}

bool MakeMove(Board& board, const MoveContent& move) {
    board.squares[move.dest] = board.squares[move.src];
    board.squares[move.src].piece.PieceType = ChessPieceType::NONE;

    if (move.promotion != ChessPieceType::NONE) {
        board.squares[move.dest].piece.PieceType = move.promotion;
    }

    board.turn = oppositeColor(board.turn);
    board.LastMove = move.dest;

    if (IsKingInCheck(board, oppositeColor(board.turn))) {
        board = board;
        return false;
    }

    return true;
}

bool IsInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

static constexpr int MATE_SCORE = 30000;
static constexpr int MATE_THRESHOLD = 29000;
static constexpr int MAX_PLY = 128;

static std::chrono::steady_clock::time_point searchStartTime;
static int searchTimeLimit = 0;

bool isTimeUp() {
    if (searchTimeLimit <= 0)
        return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - searchStartTime)
                       .count();

    return elapsed >= searchTimeLimit;
}

int quiescence(Board& board, int alpha, int beta, int depth) {
    PROFILE_SCOPE("Quiescence");

    searchInfo.qnodes++;
    searchInfo.updateSelectiveDepth();

    if (searchInfo.stopped || isTimeUp()) {
        searchInfo.stopped = true;
        return 0;
    }

    int standPat = EvaluatePosition(board);

    if (standPat >= beta)
        return beta;
    if (standPat > alpha)
        alpha = standPat;

    if (depth <= -6)
        return standPat;

    std::vector<MoveContent> moves;
    GenerateCaptures(board, moves);

    if (moves.empty())
        return standPat;

    moveOrdering.orderMoves(moves, board, searchInfo.ply, nullptr);

    for (const auto& move : moves) {
        if (enhancements.getDelta().canPrune(standPat, alpha, getPieceValue(move.capture))) {
            continue;
        }

        if (!enhancements.getSEE().see(board, move, 0)) {
            continue;
        }

        Board newBoard = board;
        if (!MakeMove(newBoard, move))
            continue;

        searchInfo.ply++;
        int score = -quiescence(newBoard, -beta, -alpha, depth - 1);
        searchInfo.ply--;

        if (searchInfo.stopped)
            return 0;

        if (score >= beta) {
            return beta;
        }

        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

int alphaBeta(Board& board, int depth, int alpha, int beta, bool pvNode, bool cutNode,
              MoveContent* prevMove, bool doNull) {
    PROFILE_SCOPE("AlphaBeta");

    searchInfo.nodes++;

    if (searchInfo.stopped || isTimeUp()) {
        searchInfo.stopped = true;
        return 0;
    }

    bool isRoot = (searchInfo.ply == 0);
    bool inCheck = IsInCheck(board, board.turn);
    int originalAlpha = alpha;

    if (depth <= 0) {
        return quiescence(board, alpha, beta, 0);
    }

    if (!isRoot) {
        if (board.halfMoveClock >= 100)
            return 0;

        alpha = std::max(alpha, -MATE_SCORE + searchInfo.ply);
        beta = std::min(beta, MATE_SCORE - searchInfo.ply - 1);
        if (alpha >= beta)
            return alpha;
    }

    TTEntry* ttEntry = nullptr;
    MoveContent ttMove;
    bool ttHit = false;
    int ttValue = 0;

    if (!isRoot) {
        uint64_t hash = ComputeHash(board);
        ttEntry = tt.probe(hash);
        ttHit = (ttEntry != nullptr && ttEntry->key == hash);

        if (ttHit) {
            ttMove = ttEntry->move;
            ttValue = ttEntry->score;

            if (ttEntry->depth >= depth && !pvNode) {
                if (ttEntry->flag == EXACT)
                    return ttValue;
                if (ttEntry->flag == LOWERBOUND && ttValue >= beta)
                    return ttValue;
                if (ttEntry->flag == UPPERBOUND && ttValue <= alpha)
                    return ttValue;
            }
        }
    }

    int staticEval = 0;
    bool improving = false;

    if (!inCheck) {
        staticEval = ttHit ? ttEntry->eval : EvaluatePosition(board);
        searchInfo.staticEval[searchInfo.ply] = staticEval;

        if (searchInfo.ply >= 2) {
            improving = staticEval > searchInfo.staticEval[searchInfo.ply - 2];
        }
        searchInfo.improving[searchInfo.ply] = improving;

        if (enhancements.getRazor().canApply(depth, staticEval, alpha, pvNode, inCheck)) {
            int razorMargin = enhancements.getRazor().getMargin(depth);
            int razorAlpha = alpha - razorMargin;
            int razorScore = quiescence(board, razorAlpha, razorAlpha + 1, 0);
            if (razorScore <= razorAlpha)
                return razorScore;
        }

        auto hasNonPawnMaterial = [&board](ChessPieceColor color) {
            for (int i = 0; i < 64; i++) {
                if (board.squares[i].piece.PieceColor == color &&
                    board.squares[i].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[i].piece.PieceType != ChessPieceType::PAWN &&
                    board.squares[i].piece.PieceType != ChessPieceType::KING) {
                    return true;
                }
            }
            return false;
        };

        if (!pvNode && doNull && depth >= 3 && staticEval >= beta &&
            hasNonPawnMaterial(board.turn)) {

            int R = 3 + depth / 6 + std::min(3, (staticEval - beta) / 200);

            Board nullBoard = board;
            nullBoard.turn = oppositeColor(nullBoard.turn);

            searchInfo.ply++;
            int nullScore = -alphaBeta(nullBoard, depth - R - 1, -beta, -beta + 1, false, !cutNode,
                                       nullptr, false);
            searchInfo.ply--;

            if (nullScore >= beta && abs(nullScore) < MATE_THRESHOLD) {
                return beta;
            }
        }

        if (enhancements.getProbCut().canApply(depth, beta, pvNode, inCheck)) {
            int probCutBeta = enhancements.getProbCut().getBeta(beta, depth);

            std::vector<MoveContent> captures;
            GenerateCaptures(board, captures);
            moveOrdering.orderMoves(captures, board, searchInfo.ply, prevMove);

            for (const auto& move : captures) {
                if (!enhancements.getSEE().see(board, move, probCutBeta - staticEval)) {
                    continue;
                }

                Board newBoard = board;
                if (!MakeMove(newBoard, move))
                    continue;

                searchInfo.ply++;
                int score = -alphaBeta(newBoard, depth - 4, -probCutBeta, -probCutBeta + 1, false,
                                       !cutNode, &move, true);
                searchInfo.ply--;

                if (score >= probCutBeta)
                    return score;
            }
        }
    }

    if (pvNode && !ttHit && enhancements.getIID().shouldApply(depth, ttHit, pvNode)) {
        int iidDepth = enhancements.getIID().getReducedDepth(depth);
        alphaBeta(board, iidDepth, alpha, beta, pvNode, cutNode, prevMove, doNull);

        uint64_t hash = ComputeHash(board);
        ttEntry = tt.probe(hash);
        if (ttEntry && ttEntry->key == hash) {
            ttMove = ttEntry->move;
            ttHit = true;
        }
    }

    std::vector<MoveContent> moves;
    GenerateAllMoves(board, moves);

    if (moves.empty()) {
        if (inCheck) {
            return -MATE_SCORE + searchInfo.ply;
        }
        return 0;
    }

    if (ttHit && ttMove.src != -1) {
        moveOrdering.setPVMove(ttMove);
    }
    moveOrdering.orderMoves(moves, board, searchInfo.ply, prevMove);
    moveOrdering.clearPVMove();

    MoveContent bestMove;
    int bestScore = -std::numeric_limits<int>::max();
    int moveNum = 0;
    int quietMoveNum = 0;
    int cutNodes = 0;

    bool skipQuiets = false;

    for (const auto& move : moves) {
        bool isQuiet = (move.capture == ChessPieceType::NONE);
        bool givesCheck = false;

        {
            Board testBoard = board;
            if (MakeMove(testBoard, move)) {
                givesCheck = IsInCheck(testBoard, testBoard.turn);
            }
        }

        if (isQuiet) {
            quietMoveNum++;

            if (skipQuiets && !givesCheck)
                continue;

            if (!inCheck && !givesCheck) {
                if (enhancements.getLMP().canPrune(depth, quietMoveNum, improving, inCheck, false,
                                                   staticEval, beta)) {
                    skipQuiets = true;
                    continue;
                }

                if (enhancements.getFutility().canPrune(
                        depth, staticEval + getPieceValue(move.piece), alpha, beta, inCheck)) {
                    continue;
                }
            }
        }

        moveNum++;

        int extension = 0;

        if (inCheck) {
            extension = 1;
        } else if (ttHit && move == ttMove &&
                   enhancements.getSingular().shouldExtend(depth, beta, ttValue, ttEntry->depth)) {

            int singularBeta = enhancements.getSingular().getReducedBeta(ttValue, depth);
            int singularDepth = (depth - 1) / 2;

            Board testBoard = board;
            searchInfo.ply++;
            int singularScore = alphaBeta(testBoard, singularDepth, singularBeta - 1, singularBeta,
                                          false, cutNode, prevMove, true);
            searchInfo.ply--;

            if (singularScore < singularBeta) {
                extension = 1;
            }
        } else if (givesCheck && enhancements.getSEE().see(board, move, 0)) {
            extension = 1;
        }

        Board newBoard = board;
        if (!MakeMove(newBoard, move))
            continue;

        int newDepth = depth - 1 + extension;
        int score;

        searchInfo.ply++;

        if (moveNum == 1) {
            score = -alphaBeta(newBoard, newDepth, -beta, -alpha, pvNode, false, &move, true);
        } else {
            int reduction = 0;

            if (depth >= 3 && moveNum > 1 && isQuiet && !inCheck && !givesCheck) {
                reduction = enhancements.getLMR().getReduction(depth, moveNum, pvNode, improving,
                                                               inCheck, givesCheck, false, false);
                reduction = std::min(depth - 2, reduction);
            }

            score = -alphaBeta(newBoard, newDepth - reduction, -alpha - 1, -alpha, false, true,
                               &move, true);

            if (score > alpha && reduction > 0) {
                score = -alphaBeta(newBoard, newDepth, -alpha - 1, -alpha, false, !cutNode, &move,
                                   true);
            }

            if (score > alpha && score < beta) {
                score = -alphaBeta(newBoard, newDepth, -beta, -alpha, true, false, &move, true);
            }
        }

        searchInfo.ply--;

        if (searchInfo.stopped)
            return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;

            if (score > alpha) {
                alpha = score;

                if (score >= beta) {
                    if (isQuiet) {
                        moveOrdering.updateKillers(move, searchInfo.ply);
                        moveOrdering.updateHistory(move, depth, board.turn, true);
                        if (prevMove) {
                            moveOrdering.updateCounterMove(*prevMove, move, board.turn);
                        }
                    }

                    tt.store(ComputeHash(board), bestMove, beta, LOWERBOUND, depth, staticEval);

                    if (enhancements.getMultiCut().canApply(depth, pvNode, inCheck)) {
                        cutNodes++;
                        if (enhancements.getMultiCut().shouldCut(cutNodes) &&
                            moveNum >= SearchEnhancements::MultiCut::MC_MAX_MOVES) {
                            return beta;
                        }
                    }

                    return beta;
                }
            }
        }

        if (isQuiet && score < alpha - 100) {
            moveOrdering.updateHistory(move, depth, board.turn, false);
        }
    }

    if (bestScore == -std::numeric_limits<int>::max()) {
        if (inCheck) {
            return -MATE_SCORE + searchInfo.ply;
        }
        return 0;
    }

    TTFlag flag = (bestScore >= beta)            ? LOWERBOUND
                  : (bestScore <= originalAlpha) ? UPPERBOUND
                                                 : EXACT;

    tt.store(ComputeHash(board), bestMove, bestScore, flag, depth, staticEval);

    return bestScore;
}

int iterativeDeepening(Board& board, int maxDepth, int maxTime, MoveContent& bestMove) {
    PROFILE_SCOPE("IterativeDeepening");

    searchInfo.clear();
    moveOrdering.clear();
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = maxTime;

    int score = 0;
    auto& aspiration = enhancements.getAspiration();

    for (int depth = 1; depth <= maxDepth && !searchInfo.stopped; depth++) {
        searchInfo.rootDepth = depth;

        if (depth >= 5) {
            aspiration.reset(score);

            while (!searchInfo.stopped) {
                score = alphaBeta(board, depth, aspiration.alpha, aspiration.beta, true, false,
                                  nullptr, true);

                if (searchInfo.stopped)
                    break;

                if (score <= aspiration.alpha) {
                    aspiration.widen(false);
                } else if (score >= aspiration.beta) {
                    aspiration.widen(true);
                } else {
                    break;
                }
            }
        } else {
            score = alphaBeta(board, depth, -std::numeric_limits<int>::max(),
                              std::numeric_limits<int>::max(), true, false, nullptr, true);
        }

        if (!searchInfo.stopped) {
            uint64_t hash = ComputeHash(board);
            TTEntry* entry = tt.probe(hash);
            if (entry && entry->key == hash) {
                bestMove = entry->move;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - searchStartTime)
                               .count();

            double nps = elapsed > 0 ? (searchInfo.nodes * 1000.0 / elapsed) : 0;

            std::cout << "info depth " << depth << " score cp " << score << " nodes "
                      << searchInfo.nodes << " nps " << static_cast<int>(nps) << " time " << elapsed
                      << " pv " << moveToString(bestMove) << std::endl;
        }

        if (abs(score) > MATE_THRESHOLD) {
            break;
        }
    }

    return score;
}

std::string moveToString(const MoveContent& move) {
    std::string result;
    result += char('a' + (move.src % 8));
    result += char('1' + (move.src / 8));
    result += char('a' + (move.dest % 8));
    result += char('1' + (move.dest / 8));

    if (move.promotion != ChessPieceType::NONE) {
        switch (move.promotion) {
            case ChessPieceType::QUEEN:
                result += 'q';
                break;
            case ChessPieceType::ROOK:
                result += 'r';
                break;
            case ChessPieceType::BISHOP:
                result += 'b';
                break;
            case ChessPieceType::KNIGHT:
                result += 'n';
                break;
            default:
                break;
        }
    }

    return result;
}

uint64_t ComputeHash(const Board& board) {
    uint64_t hash = 0;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType != ChessPieceType::NONE) {
            hash ^=
                ((uint64_t)i << 32) | ((uint64_t)board.squares[i].piece.PieceType << 16) |
                ((uint64_t)(board.squares[i].piece.PieceColor == ChessPieceColor::WHITE ? 1 : 0));
        }
    }
    hash ^= (board.turn == ChessPieceColor::WHITE) ? 0x1234567890ABCDEF : 0xFEDCBA0987654321;
    return hash;
}

int getPieceValue(ChessPieceType piece) {
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

ChessPieceColor oppositeColor(ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
}

} // namespace EnhancedSearch