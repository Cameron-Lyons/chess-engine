#include <iostream>
#include "ChessBoard.h"
#include "BitboardMoves.h"

void printBitboard(Bitboard bb) {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            if (bb & (1ULL << sq)) {
                std::cout << "1 ";
            } else {
                std::cout << "0 ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

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

int main() {
    initKnightAttacks();
    initKingAttacks();
    
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Initial Position:" << std::endl;
    printBoard(board);
    
    std::cout << "White Pawns Bitboard:" << std::endl;
    printBitboard(board.whitePawns);
    
    std::cout << "Black Pawns Bitboard:" << std::endl;
    printBitboard(board.blackPawns);
    
    std::cout << "All Pieces Bitboard:" << std::endl;
    printBitboard(board.allPieces);
    
    std::cout << "Testing bitboard move generation..." << std::endl;
    
    Bitboard whitePawns = board.whitePawns;
    Bitboard empty = ~board.allPieces;
    Bitboard blackPieces = board.blackPieces;
    
    Bitboard pawnPushesBB = pawnPushes(whitePawns, empty, ChessPieceColor::WHITE);
    Bitboard pawnCapturesBB = pawnCaptures(whitePawns, blackPieces, ChessPieceColor::WHITE);
    
    std::cout << "White Pawn Pushes:" << std::endl;
    printBitboard(pawnPushesBB);
    
    std::cout << "White Pawn Captures:" << std::endl;
    printBitboard(pawnCapturesBB);
    
    Bitboard whiteKnights = board.whiteKnights;
    Bitboard whitePieces = board.whitePieces;
    
    Bitboard knightMovesBB = knightMoves(whiteKnights, whitePieces);
    std::cout << "White Knight Moves:" << std::endl;
    printBitboard(knightMovesBB);
    
    Bitboard whiteBishops = board.whiteBishops;
    Bitboard bishopMovesBB = bishopMoves(whiteBishops, whitePieces, board.allPieces);
    std::cout << "White Bishop Moves:" << std::endl;
    printBitboard(bishopMovesBB);
    
    Bitboard whiteRooks = board.whiteRooks;
    Bitboard rookMovesBB = rookMoves(whiteRooks, whitePieces, board.allPieces);
    std::cout << "White Rook Moves:" << std::endl;
    printBitboard(rookMovesBB);
    
    Bitboard whiteQueens = board.whiteQueens;
    Bitboard queenMovesBB = queenMoves(whiteQueens, whitePieces, board.allPieces);
    std::cout << "White Queen Moves:" << std::endl;
    printBitboard(queenMovesBB);
    
    Bitboard whiteKings = board.whiteKings;
    Bitboard kingMovesBB = kingMoves(whiteKings, whitePieces);
    std::cout << "White King Moves:" << std::endl;
    printBitboard(kingMovesBB);
    
    int totalMoves = popcount(pawnPushesBB) + popcount(pawnCapturesBB) + 
                     popcount(knightMovesBB) + popcount(bishopMovesBB) + 
                     popcount(rookMovesBB) + popcount(queenMovesBB) + 
                     popcount(kingMovesBB);
    
    std::cout << "Total legal moves for White: " << totalMoves << std::endl;
    
    std::cout << "\n=== Testing Bitboard Sync After Moves ===" << std::endl;
    
    int e2 = 12; // e2 square
    int e4 = 28; // e4 square
    board.movePiece(e2, e4);
    
    std::cout << "After e2e4:" << std::endl;
    printBoard(board);
    
    std::cout << "White Pawns Bitboard after e2e4:" << std::endl;
    printBitboard(board.whitePawns);
    
    int b1 = 1;  // b1 square (knight)
    int f3 = 21; // f3 square
    board.movePiece(b1, f3);
    
    std::cout << "After Nf3:" << std::endl;
    printBoard(board);
    
    std::cout << "White Knights Bitboard after Nf3:" << std::endl;
    printBitboard(board.whiteKnights);
    
    std::cout << "Verifying bitboard sync..." << std::endl;
    board.updateBitboards(); // This should not change anything if sync is correct
    
    std::cout << "White Pawns Bitboard after sync check:" << std::endl;
    printBitboard(board.whitePawns);
    
    std::cout << "White Knights Bitboard after sync check:" << std::endl;
    printBitboard(board.whiteKnights);
    
    return 0;
} 