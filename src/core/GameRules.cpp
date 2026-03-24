#include "GameRules.h"

#include "ChessEngine.h"
#include "search/search.h"

#include <iostream>
#include <vector>

bool hasAnyLegalMoves(Board& board, ChessPieceColor sideToMove) {
    std::vector<Move> moves = GetAllMoves(board, sideToMove);
    for (const auto& move : moves) {
        const Piece& piece = board.squares[move.first].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != sideToMove) {
            continue;
        }
        if (IsMoveLegal(board, move.first, move.second)) {
            return true;
        }
    }
    return false;
}

GameState checkGameState(Board& board) {
    ChessPieceColor currentPlayer = board.turn;
    std::vector<Move> moves = GetAllMoves(board, currentPlayer);
    std::vector<Move> legalMoves;
    for (const auto& move : moves) {
        Board testBoard = board;
        if (testBoard.movePiece(move.first, move.second)) {
            if (!IsKingInCheck(testBoard, currentPlayer)) {
                legalMoves.push_back(move);
            }
        }
    }

    bool isInCheck = IsKingInCheck(board, currentPlayer);

    if (legalMoves.empty()) {
        if (isInCheck) {
            return (currentPlayer == ChessPieceColor::WHITE) ? GameState::CHECKMATE_BLACK_WINS
                                                             : GameState::CHECKMATE_WHITE_WINS;
        }
        return GameState::STALEMATE;
    }

    std::vector<ChessPieceType> whitePieces;
    std::vector<ChessPieceType> blackPieces;
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whitePieces.push_back(piece.PieceType);
            } else {
                blackPieces.push_back(piece.PieceType);
            }
        }
    }

    const bool bareKings = whitePieces.empty() && blackPieces.empty();
    const bool loneMinorVsKing =
        (whitePieces.size() == 1 && blackPieces.empty() &&
         (whitePieces[0] == ChessPieceType::BISHOP || whitePieces[0] == ChessPieceType::KNIGHT)) ||
        (blackPieces.size() == 1 && whitePieces.empty() &&
         (blackPieces[0] == ChessPieceType::BISHOP || blackPieces[0] == ChessPieceType::KNIGHT));
    const bool bishopsOnly = whitePieces.size() == 1 && blackPieces.size() == 1 &&
                             whitePieces[0] == ChessPieceType::BISHOP &&
                             blackPieces[0] == ChessPieceType::BISHOP;

    if (bareKings || loneMinorVsKing || bishopsOnly) {
        return GameState::DRAW_INSUFFICIENT_MATERIAL;
    }

    return GameState::ONGOING;
}

GameState checkGameState(const Engine& engine) {
    Board board = engine.board();
    GameState boardState = checkGameState(board);
    if (boardState != GameState::ONGOING) {
        return boardState;
    }
    if (engine.isDrawByFiftyMoveRule()) {
        return GameState::DRAW_FIFTY_MOVE_RULE;
    }
    if (engine.isDrawByThreefoldRepetition()) {
        return GameState::DRAW_THREEFOLD_REPETITION;
    }
    return GameState::ONGOING;
}

void announceGameResult(GameState state) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "                GAME OVER                \n";
    std::cout << std::string(50, '=') << "\n";

    switch (state) {
        case GameState::CHECKMATE_WHITE_WINS:
            std::cout << "🏆 CHECKMATE! WHITE WINS! 🏆\n";
            std::cout << "Black king is in checkmate.\n";
            std::cout << "White has successfully cornered the black king!\n";
            break;

        case GameState::CHECKMATE_BLACK_WINS:
            std::cout << "🏆 CHECKMATE! BLACK WINS! 🏆\n";
            std::cout << "White king is in checkmate.\n";
            std::cout << "Black has successfully cornered the white king!\n";
            break;

        case GameState::STALEMATE:
            std::cout << "🤝 STALEMATE - DRAW! 🤝\n";
            std::cout << "The current player has no legal moves but is not in check.\n";
            std::cout << "The game ends in a draw by stalemate.\n";
            break;

        case GameState::DRAW_INSUFFICIENT_MATERIAL:
            std::cout << "🤝 DRAW - INSUFFICIENT MATERIAL! 🤝\n";
            std::cout << "Neither side has enough material to force checkmate.\n";
            std::cout << "The game ends in a draw.\n";
            break;

        case GameState::DRAW_THREEFOLD_REPETITION:
            std::cout << "🤝 DRAW - THREEFOLD REPETITION! 🤝\n";
            std::cout << "The same position has appeared three times.\n";
            std::cout << "The game ends in a draw.\n";
            break;

        case GameState::DRAW_FIFTY_MOVE_RULE:
            std::cout << "🤝 DRAW - FIFTY-MOVE RULE! 🤝\n";
            std::cout << "Fifty full moves passed without a pawn move or capture.\n";
            std::cout << "The game ends in a draw.\n";
            break;

        case GameState::ONGOING:
            break;
    }

    if (state != GameState::ONGOING) {
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Thank you for playing!\n";
        std::cout << "Press Enter to exit...\n";
    }
}
