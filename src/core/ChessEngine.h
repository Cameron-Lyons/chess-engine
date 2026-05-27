#pragma once

#include "core/CastlingConstants.h"
#include "core/ChessBoard.h"
#include "core/Move.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

class Engine {
public:
    static constexpr std::string_view kDefaultStartingFen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Engine() {
        newGame();
    }

    const Board& board() const {
        return board_;
    }

    Board& mutableBoard() {
        return board_;
    }

    bool isDrawByThreefoldRepetition() const {
        if (positionHistory_.empty()) {
            return false;
        }
        const uint64_t currentKey = positionHistory_.back();
        int count = 0;
        for (const auto& key : positionHistory_) {
            if (key == currentKey) {
                ++count;
            }
        }
        return count >= 3;
    }

    bool isDrawByFiftyMoveRule() const {
        return board_.halfmoveClock >= 100;
    }

    std::vector<uint64_t> previousPositionHashes() const {
        if (positionHistory_.size() <= 1) {
            return {};
        }
        return std::vector<uint64_t>(positionHistory_.begin(), positionHistory_.end() - 1);
    }

    void newGame(std::string_view fen = kDefaultStartingFen) {
        board_ = Board();
        board_.InitializeFromFEN(fen);
        previousBoard_ = board_;
        resetPositionHistory();
    }

    std::expected<void, ChessError> setPositionFromFEN(std::string_view fen) {
        auto parsed = board_.fromFEN(fen);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        previousBoard_ = board_;
        resetPositionHistory();
        return {};
    }

    std::expected<void, ChessError> applyMove(
        const Move& move, ChessPieceType promotionPiece = ChessPieceType::QUEEN) {
        if (!move.isValid()) {
            return std::unexpected(ChessError::InvalidPosition);
        }
        return applyMove(move.first, move.second, promotionPiece);
    }

    std::expected<void, ChessError> applyMove(
        int fromSquare, int toSquare, ChessPieceType promotionPiece = ChessPieceType::QUEEN) {
        if (fromSquare < 0 || fromSquare >= NUM_SQUARES || toSquare < 0 ||
            toSquare >= NUM_SQUARES) {
            return std::unexpected(ChessError::InvalidPosition);
        }

        Piece piece = board_.squares[fromSquare].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            return std::unexpected(ChessError::NoPieceAtSource);
        }
        if (piece.PieceColor != board_.turn) {
            return std::unexpected(ChessError::WrongTurn);
        }
        if (!IsMoveLegal(board_, fromSquare, toSquare)) {
            return std::unexpected(ChessError::InvalidMove);
        }

        previousBoard_ = board_;
        const int previousHalfmoveClock = board_.halfmoveClock;
        const int previousFullmoveNumber = board_.fullmoveNumber;
        const Piece capturedPiece = board_.squares[toSquare].piece;

        const int destRow = toSquare / BOARD_SIZE;
        const bool promotePawn = (piece.PieceType == ChessPieceType::PAWN &&
                                  (destRow == 0 || destRow == (BOARD_SIZE - 1)));
        board_.movePiece(fromSquare, toSquare);

        if (promotePawn) {
            const bool isValidPromotion = promotionPiece == ChessPieceType::QUEEN ||
                                          promotionPiece == ChessPieceType::ROOK ||
                                          promotionPiece == ChessPieceType::BISHOP ||
                                          promotionPiece == ChessPieceType::KNIGHT;
            const ChessPieceType finalPromotion =
                isValidPromotion ? promotionPiece : ChessPieceType::QUEEN;
            board_.squares[toSquare].piece.PieceType = finalPromotion;
            board_.updateBitboards();
        }

        const int srcCol = fromSquare % BOARD_SIZE;
        const int destCol = toSquare % BOARD_SIZE;

        if (piece.PieceType == ChessPieceType::KING &&
            srcCol == CastlingConstants::kWhiteKingStartCol) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                if (destCol == CastlingConstants::kKingsideKingDestCol) {
                    board_.movePiece(CastlingConstants::kWhiteKingsideRookSquare,
                                     CastlingConstants::kWhiteKingsideRookCastleDest);
                    board_.clearCastlingRights(CastlingConstants::kWhiteCastlingRightsMask);
                } else if (destCol == CastlingConstants::kQueensideKingDestCol) {
                    board_.movePiece(CastlingConstants::kWhiteQueensideRookSquare,
                                     CastlingConstants::kWhiteQueensideRookCastleDest);
                    board_.clearCastlingRights(CastlingConstants::kWhiteCastlingRightsMask);
                }
            } else {
                if (destCol == CastlingConstants::kKingsideKingDestCol) {
                    board_.movePiece(CastlingConstants::kBlackKingsideRookSquare,
                                     CastlingConstants::kBlackKingsideRookCastleDest);
                    board_.clearCastlingRights(CastlingConstants::kBlackCastlingRightsMask);
                } else if (destCol == CastlingConstants::kQueensideKingDestCol) {
                    board_.movePiece(CastlingConstants::kBlackQueensideRookSquare,
                                     CastlingConstants::kBlackQueensideRookCastleDest);
                    board_.clearCastlingRights(CastlingConstants::kBlackCastlingRightsMask);
                }
            }
        }

        board_.halfmoveClock = (piece.PieceType == ChessPieceType::PAWN ||
                                capturedPiece.PieceType != ChessPieceType::NONE)
                                   ? 0
                                   : (previousHalfmoveClock + 1);
        board_.fullmoveNumber = (piece.PieceColor == ChessPieceColor::BLACK)
                                    ? (previousFullmoveNumber + 1)
                                    : previousFullmoveNumber;

        UpdateCheckState(board_);
        if (IsKingInCheck(board_, piece.PieceColor)) {
            board_ = previousBoard_;
            UpdateCheckState(board_);
            return std::unexpected(ChessError::MoveLeavesKingInCheck);
        }

        board_.turn = (board_.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                              : ChessPieceColor::WHITE;
        positionHistory_.push_back(ComputeZobrist(board_));
        return {};
    }

private:
    void resetPositionHistory() {
        positionHistory_.clear();
        positionHistory_.push_back(ComputeZobrist(board_));
    }

    Board board_;
    Board previousBoard_;
    std::vector<uint64_t> positionHistory_;
};
