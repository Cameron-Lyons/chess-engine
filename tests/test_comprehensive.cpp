#include <iostream>
#include <vector>
#include "ChessBoard.h"
#include "BitboardMoves.h"

std::vector<std::pair<int, int>> generateBitboardMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generatePawnMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateKnightMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateBishopMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateRookMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateQueenMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateKingMoves(Board& board, ChessPieceColor color);

void printBoard(const Board& board) {
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << rank + 1 << " ";
        for (int file = 0; file < 8; file++) {
            int idx = rank * 8 + file;
            const Piece& piece = board.squares[idx].piece;
            
            if (piece.PieceType == ChessPieceType::NONE) {
                std::cout << ". ";
            } else {
                char pieceChar = ' ';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: pieceChar = 'P'; break;
                    case ChessPieceType::KNIGHT: pieceChar = 'N'; break;
                    case ChessPieceType::BISHOP: pieceChar = 'B'; break;
                    case ChessPieceType::ROOK: pieceChar = 'R'; break;
                    case ChessPieceType::QUEEN: pieceChar = 'Q'; break;
                    case ChessPieceType::KING: pieceChar = 'K'; break;
                    default: break;
                }
                
                if (piece.PieceColor == ChessPieceColor::BLACK) {
                    pieceChar = tolower(pieceChar);
                }
                std::cout << pieceChar << " ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << "  a b c d e f g h" << std::endl << std::endl;
}

bool isKingInCheck(const Board& board, ChessPieceColor color) {
    int kingSq = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING && board.squares[i].piece.PieceColor == color) {
            kingSq = i;
            break;
        }
    }
    if (kingSq == -1) return false;

    Bitboard occ = board.allPieces;
    ChessPieceColor enemyColor = (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    Bitboard enemyPawns = board.getPieceBitboard(ChessPieceType::PAWN, enemyColor);
    Bitboard enemyKnights = board.getPieceBitboard(ChessPieceType::KNIGHT, enemyColor);
    Bitboard enemyBishops = board.getPieceBitboard(ChessPieceType::BISHOP, enemyColor);
    Bitboard enemyRooks = board.getPieceBitboard(ChessPieceType::ROOK, enemyColor);
    Bitboard enemyQueens = board.getPieceBitboard(ChessPieceType::QUEEN, enemyColor);

    Bitboard pawnAttacksBB = pawnAttacks(enemyColor, kingSq);
    if (pawnAttacksBB & enemyPawns) return true;

    if (KnightAttacks[kingSq] & enemyKnights) return true;

    if (bishopAttacks(kingSq, occ) & (enemyBishops | enemyQueens)) return true;

    if (rookAttacks(kingSq, occ) & (enemyRooks | enemyQueens)) return true;

    return false;
}

int main() {
    initKnightAttacks();
    initKingAttacks();
    
    std::cout << "=== COMPREHENSIVE BITBOARD TEST ===" << std::endl << std::endl;
    
    std::cout << "Test 1: Basic Move Generation" << std::endl;
    std::cout << "=============================" << std::endl;
    
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Initial Position:" << std::endl;
    printBoard(board);
    
    auto whiteMoves = generateBitboardMoves(board, ChessPieceColor::WHITE);
    
    std::cout << "White moves generated: " << whiteMoves.size() << std::endl;
    
    if (whiteMoves.size() == 20) {
        std::cout << "✓ PASS: Correct number of moves for starting position" << std::endl;
    } else {
        std::cout << "✗ FAIL: Expected 20 moves, got " << whiteMoves.size() << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Test 2: Check Detection" << std::endl;
    std::cout << "======================" << std::endl;
    
    bool whiteInCheck = isKingInCheck(board, ChessPieceColor::WHITE);
    bool blackInCheck = isKingInCheck(board, ChessPieceColor::BLACK);
    
    std::cout << "Starting position:" << std::endl;
    std::cout << "  White in check: " << (whiteInCheck ? "✗ Yes" : "✓ No") << std::endl;
    std::cout << "  Black in check: " << (blackInCheck ? "✗ Yes" : "✓ No") << std::endl;
    
    Board checkBoard;
    checkBoard.InitializeFromFEN("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    checkBoard.movePiece(1, 18); // Nb1c3
    checkBoard.movePiece(52, 36); // e5e4
    checkBoard.movePiece(18, 28); // Nc3e4
    checkBoard.movePiece(60, 52); // Ke8e7
    checkBoard.movePiece(28, 45); // Ne4d6 (check)
    
    std::cout << "Position after Ne4d6:" << std::endl;
    printBoard(checkBoard);
    
    bool blackInCheckAfter = isKingInCheck(checkBoard, ChessPieceColor::BLACK);
    std::cout << "  Black in check: " << (blackInCheckAfter ? "✓ Yes" : "✗ No") << std::endl;
    std::cout << std::endl;
    
    std::cout << "Test 3: Bitboard Sync" << std::endl;
    std::cout << "====================" << std::endl;
    
    Board syncBoard;
    syncBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    syncBoard.movePiece(12, 28); // e2e4
    
    bool pawnOnE4 = syncBoard.squares[28].piece.PieceType == ChessPieceType::PAWN;
    bool pawnBitboardOnE4 = syncBoard.whitePawns & (1ULL << 28);
    bool noPawnOnE2 = syncBoard.squares[12].piece.PieceType == ChessPieceType::NONE;
    bool noPawnBitboardOnE2 = !(syncBoard.whitePawns & (1ULL << 12));
    
    std::cout << "After e2e4:" << std::endl;
    std::cout << "  Pawn on e4 (array): " << (pawnOnE4 ? "✓ Yes" : "✗ No") << std::endl;
    std::cout << "  Pawn on e4 (bitboard): " << (pawnBitboardOnE4 ? "✓ Yes" : "✗ No") << std::endl;
    std::cout << "  No pawn on e2 (array): " << (noPawnOnE2 ? "✓ Yes" : "✗ No") << std::endl;
    std::cout << "  No pawn on e2 (bitboard): " << (noPawnBitboardOnE2 ? "✓ Yes" : "✗ No") << std::endl;
    
    if (pawnOnE4 && pawnBitboardOnE4 && noPawnOnE2 && noPawnBitboardOnE2) {
        std::cout << "✓ PASS: Bitboards are in sync" << std::endl;
    } else {
        std::cout << "✗ FAIL: Bitboards are out of sync" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Test 4: Individual Piece Move Generation" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    auto pawnMoves = generatePawnMoves(board, ChessPieceColor::WHITE);
    auto knightMoves = generateKnightMoves(board, ChessPieceColor::WHITE);
    auto bishopMoves = generateBishopMoves(board, ChessPieceColor::WHITE);
    auto rookMoves = generateRookMoves(board, ChessPieceColor::WHITE);
    auto queenMoves = generateQueenMoves(board, ChessPieceColor::WHITE);
    auto kingMoves = generateKingMoves(board, ChessPieceColor::WHITE);
    
    std::cout << "Moves by piece type:" << std::endl;
    std::cout << "  Pawns: " << pawnMoves.size() << std::endl;
    std::cout << "  Knights: " << knightMoves.size() << std::endl;
    std::cout << "  Bishops: " << bishopMoves.size() << std::endl;
    std::cout << "  Rooks: " << rookMoves.size() << std::endl;
    std::cout << "  Queens: " << queenMoves.size() << std::endl;
    std::cout << "  Kings: " << kingMoves.size() << std::endl;
    
    int totalIndividual = pawnMoves.size() + knightMoves.size() + bishopMoves.size() + 
                         rookMoves.size() + queenMoves.size() + kingMoves.size();
    
    if (totalIndividual == 20) {
        std::cout << "✓ PASS: Individual piece moves sum to correct total" << std::endl;
    } else {
        std::cout << "✗ FAIL: Individual moves sum to " << totalIndividual << ", expected 20" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== TEST COMPLETE ===" << std::endl;
    
    return 0;
} 