#include "engine_globals.h"
#include <sstream>
#include <random>
#include <vector>

std::unordered_map<std::string, std::vector<std::string>> OpeningBookOptions = {
    // After 1.e4 - Multiple responses
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", {"e5", "c5", "e6", "c6", "Nf6"}},
    
    // Italian Game
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"Nf3"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", {"Nc6"}},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", {"Bc4"}},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", {"Bc5"}},
    {"r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", {"O-O"}},
    
    // Ruy Lopez
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", {"Bb5"}},
    {"r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", {"a6"}},
    {"r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4", {"Ba4"}},
    {"r1bqkbnr/1ppp1ppp/p1n5/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 1 4", {"Nf6"}},
    {"r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 5", {"O-O"}},
    {"r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 3 5", {"Be7"}},
    
    // Queen's Gambit
    {"rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1", {"d5"}},
    {"rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2", {"c4"}},
    {"rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"e6"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Nf6"}},
    {"rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"Bg5"}},
    
    // Sicilian Defense
    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"Nf3"}},
    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", {"d6"}},
    {"rnbqkbnr/pp2pppp/3p4/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3", {"d4"}},
    {"rnbqkbnr/pp2pppp/3p4/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3", {"cxd4"}},
    {"rnbqkbnr/pp2pppp/3p4/8/3pP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 4", {"Nxd4"}},
    {"rnbqkbnr/pp2pppp/3p4/8/3NP3/8/PPP2PPP/RNBQKB1R b KQkq - 0 4", {"Nf6"}},
    
    // French Defense
    {"rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"d4"}},
    {"rnbqkbnr/pppp1ppp/4p3/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq - 0 2", {"d5"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", {"Bb4"}},
    
    // Caro-Kann Defense
    {"rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"d4"}},
    {"rnbqkbnr/pp1ppppp/2p5/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq - 0 2", {"d5"}},
    {"rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/pp2pppp/2p5/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", {"dxe4"}},
    
    // King's Indian Defense
    {"rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1", {"Nf6"}},
    {"rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2", {"c4"}},
    {"rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"g6"}},
    {"rnbqkb1r/pppppp1p/5np1/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkb1r/pppppp1p/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Bg7"}},
    {"rnbqk2r/ppppppbp/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"e4"}},
    
    // English Opening
    {"rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq - 0 1", {"e5"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 0 2", {"Nc3"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR b KQkq - 1 2", {"Nf6"}},
    {"rnbqkb1r/pppp1ppp/5n2/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR w KQkq - 2 3", {"g3"}},
    {"rnbqkb1r/pppp1ppp/5n2/4p3/2P5/2N3P1/PP1PPP1P/R1BQKBNR b KQkq - 0 3", {"d5"}},
    
    // Alekhine's Defense
    {"rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", {"e5"}},
    {"rnbqkb1r/pppppppp/5n2/4P3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 2 2", {"Nd5"}},
    {"rnbqkb1r/pppppppp/8/3nP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 3 3", {"d4"}},
    {"rnbqkb1r/pppppppp/8/3nP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3", {"d6"}},
    
    // Nimzo-Indian Defense
    {"rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2", {"c4"}},
    {"rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"e6"}},
    {"rnbqkb1r/pppp1ppp/4pn2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkb1r/pppp1ppp/4pn2/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Bb4"}},
    {"rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"e3"}}
};

std::unordered_map<std::string, std::string> OpeningBook;

uint64_t ZobristTable[64][12];
uint64_t ZobristBlackToMove;
ThreadSafeTT TransTable;

std::string getFEN(const Board& board) {
    std::string fen;
    for (int row = 7; row >= 0; --row) {
        int emptyCount = 0;
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                char symbol = '.';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p'; break;
                    case ChessPieceType::KNIGHT: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n'; break;
                    case ChessPieceType::BISHOP: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b'; break;
                    case ChessPieceType::ROOK: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r'; break;
                    case ChessPieceType::QUEEN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q'; break;
                    case ChessPieceType::KING: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k'; break;
                    default: symbol = '.'; break;
                }
                fen += symbol;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (row > 0) fen += '/';
    }
    fen += board.turn == ChessPieceColor::WHITE ? " w" : " b";
    fen += " KQkq - 0 1";
    return fen;
}

bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    std::string cleanMove(move);
    if (!cleanMove.empty() && (cleanMove.back() == '+' || cleanMove.back() == '#')) {
        cleanMove.pop_back();
    }
    
    if (cleanMove == "O-O" || cleanMove == "0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 6; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 6; destRow = 7;
        }
        return true;
    }
    if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 2; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 2; destRow = 7;
        }
        return true;
    }
    
    ChessPieceType pieceType = ChessPieceType::PAWN;
    size_t startPos = 0;
    bool isCapture = false;
    if (cleanMove.length() > 0) {
        switch (cleanMove[0]) {
            case 'N': pieceType = ChessPieceType::KNIGHT; startPos = 1; break;
            case 'B': pieceType = ChessPieceType::BISHOP; startPos = 1; break;
            case 'R': pieceType = ChessPieceType::ROOK; startPos = 1; break;
            case 'Q': pieceType = ChessPieceType::QUEEN; startPos = 1; break;
            case 'K': pieceType = ChessPieceType::KING; startPos = 1; break;
            default: pieceType = ChessPieceType::PAWN; startPos = 0; break;
        }
    }
    
    size_t xPos = cleanMove.find('x');
    if (xPos != std::string::npos) {
        isCapture = true;
    }
    
    if (cleanMove.length() == 2 && pieceType == ChessPieceType::PAWN) {
        destCol = cleanMove[0] - 'a';
        destRow = cleanMove[1] - '1';
        
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) {
            return false;
        }
        
        if (board.turn == ChessPieceColor::WHITE) {
            if (destRow > 0) {
                int checkRow = destRow - 1;
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::WHITE) {
                    int destPos = destRow * 8 + destCol;
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
            
            if (destRow == 3) { 
                int checkRow = 1; 
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::WHITE) {
                    int destPos = destRow * 8 + destCol;
                    int intermediatPos = 2 * 8 + destCol;
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE && 
                        board.squares[intermediatPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        } else { 
            if (destRow < 7) {
                int checkRow = destRow + 1;
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::BLACK) {
                    int destPos = destRow * 8 + destCol;
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
            
            if (destRow == 4) { 
                int checkRow = 6; 
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::BLACK) {
                    int destPos = destRow * 8 + destCol;
                    int intermediatPos = 5 * 8 + destCol;
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE && 
                        board.squares[intermediatPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || srcCol < 0 || srcCol >= 8) return false;
        
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            const Piece& piece = board.squares[pos].piece;
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                int from = pos;
                int to = destRow * 8 + destCol;
                if (board.turn == ChessPieceColor::WHITE) {
                    if ((to == from + 7 && srcCol > 0) || (to == from + 9 && srcCol < 7)) {
                        if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].piece.PieceColor == ChessPieceColor::BLACK) {
                            srcRow = row;
                            return true;
                        }
                    }
                } else {
                    if ((to == from - 9 && srcCol > 0) || (to == from - 7 && srcCol < 7)) {
                        if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].piece.PieceColor == ChessPieceColor::WHITE) {
                            srcRow = row;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4) {
        size_t equalPos = cleanMove.find('=');
        if (equalPos != std::string::npos && equalPos >= 2) {
            // Extract destination from before the '='
            destCol = cleanMove[equalPos - 2] - 'a';
            destRow = cleanMove[equalPos - 1] - '1';
            
            if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) {
                return false;
            }
            
            bool isPromotionRank = (destRow == 7 && board.turn == ChessPieceColor::WHITE) ||
                                   (destRow == 0 && board.turn == ChessPieceColor::BLACK);
            
            if (!isPromotionRank) {
                return false;
            }
            
            if (board.turn == ChessPieceColor::WHITE) {
                if (destRow > 0) {
                    int checkRow = destRow - 1;
                    int pos = checkRow * 8 + destCol;
                    const Piece& piece = board.squares[pos].piece;
                    if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::WHITE) {
                        int destPos = destRow * 8 + destCol;
                        if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                            srcCol = destCol;
                            srcRow = checkRow;
                            return true;
                        }
                    }
                }
            } else { 
                if (destRow < 7) {
                    int checkRow = destRow + 1;
                    int pos = checkRow * 8 + destCol;
                    const Piece& piece = board.squares[pos].piece;
                    if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::BLACK) {
                        int destPos = destRow * 8 + destCol;
                        if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                            srcCol = destCol;
                            srcRow = checkRow;
                            return true;
                        }
                    }
                }
            }
            return false;
        }
        
        if (cleanMove.length() >= 6 && cleanMove[1] == 'x' && equalPos != std::string::npos) {
            destCol = cleanMove[2] - 'a';
            destRow = cleanMove[3] - '1';
            srcCol = cleanMove[0] - 'a';
            
            if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || srcCol < 0 || srcCol >= 8) return false;
            
            bool isPromotionRank = (destRow == 7 && board.turn == ChessPieceColor::WHITE) ||
                                   (destRow == 0 && board.turn == ChessPieceColor::BLACK);
            
            if (!isPromotionRank) {
                return false;
            }
            
            for (int row = 0; row < 8; row++) {
                int pos = row * 8 + srcCol;
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                    int from = pos;
                    int to = destRow * 8 + destCol;
                    if (board.turn == ChessPieceColor::WHITE) {
                        if ((to == from + 7 && srcCol > 0) || (to == from + 9 && srcCol < 7)) {
                            if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[to].piece.PieceColor == ChessPieceColor::BLACK) {
                                srcRow = row;
                                return true;
                            }
                        }
                    } else {
                        if ((to == from - 9 && srcCol > 0) || (to == from - 7 && srcCol < 7)) {
                            if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[to].piece.PieceColor == ChessPieceColor::WHITE) {
                                srcRow = row;
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    }
    
    if (pieceType != ChessPieceType::PAWN) {
        if (cleanMove.length() < startPos + 2) return false;
        
        if (isCapture) {
            size_t destStart = xPos + 1;
            if (destStart + 1 >= cleanMove.length()) return false;
            destCol = cleanMove[destStart] - 'a';
            destRow = cleanMove[destStart + 1] - '1';
        } else {
            destCol = cleanMove[startPos] - 'a';
            destRow = cleanMove[startPos + 1] - '1';
        }
        
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) return false;
        
        int destPos = destRow * 8 + destCol;
        const Piece& destPiece = board.squares[destPos].piece;
        
        if (isCapture) {
            if (destPiece.PieceType == ChessPieceType::NONE || destPiece.PieceColor == board.turn) {
                return false; 
            }
        } else {
            if (destPiece.PieceType != ChessPieceType::NONE) {
                return false; 
            }
        }
        
        std::vector<std::pair<int, int>> candidates;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int pos = row * 8 + col;
                const Piece& piece = board.squares[pos].piece;
                
                if (piece.PieceType == pieceType && piece.PieceColor == board.turn) {
                    bool canReach = false;
                    
                    switch (pieceType) {
                        case ChessPieceType::KNIGHT: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            canReach = (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);
                            break;
                        }
                        case ChessPieceType::BISHOP: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            if (rowDiff == colDiff && rowDiff > 0) {
                                // Check diagonal path is clear
                                int rowStep = (destRow > row) ? 1 : -1;
                                int colStep = (destCol > col) ? 1 : -1;
                                canReach = true;
                                for (int i = 1; i < rowDiff; i++) {
                                    int checkPos = (row + i * rowStep) * 8 + (col + i * colStep);
                                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                                        canReach = false;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::ROOK: {
                            if (row == destRow || col == destCol) {
                                // Check path is clear
                                canReach = true;
                                if (row == destRow) {
                                    int start = std::min(col, destCol) + 1;
                                    int end = std::max(col, destCol);
                                    for (int c = start; c < end; c++) {
                                        if (board.squares[row * 8 + c].piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else {
                                    int start = std::min(row, destRow) + 1;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[r * 8 + col].piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::QUEEN: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            
                            if (row == destRow || col == destCol || rowDiff == colDiff) {
                                canReach = true;
                                
                                if (row == destRow) {
                                    int start = std::min(col, destCol) + 1;
                                    int end = std::max(col, destCol);
                                    for (int c = start; c < end; c++) {
                                        if (board.squares[row * 8 + c].piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (col == destCol) {
                                    int start = std::min(row, destRow) + 1;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[r * 8 + col].piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (rowDiff == colDiff) {
                                    int rowStep = (destRow > row) ? 1 : -1;
                                    int colStep = (destCol > col) ? 1 : -1;
                                    for (int i = 1; i < rowDiff; i++) {
                                        int checkPos = (row + i * rowStep) * 8 + (col + i * colStep);
                                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::KING: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            canReach = rowDiff <= 1 && colDiff <= 1 && (rowDiff + colDiff) > 0;
                            break;
                        }
                        default:
                            break;
                    }
                    
                    if (canReach) {
                        candidates.push_back({col, row});
                    }
                }
            }
        }
        
        if (candidates.size() == 1) {
            srcCol = candidates[0].first;
            srcRow = candidates[0].second;
            return true;
        } else if (candidates.size() > 1) {
            srcCol = candidates[0].first;
            srcRow = candidates[0].second;
            return true;
        }
    }
    
    return false;
}

ChessPieceType getPromotionPiece(std::string_view move) {
    std::string cleanMove(move);
    if (!cleanMove.empty() && (cleanMove.back() == '+' || cleanMove.back() == '#')) {
        cleanMove.pop_back();
    }
    
    size_t equalPos = cleanMove.find('=');
    if (equalPos != std::string::npos && equalPos + 1 < cleanMove.length()) {
        char promotionChar = cleanMove[equalPos + 1];
        switch (promotionChar) {
            case 'Q': case 'q': return ChessPieceType::QUEEN;
            case 'R': case 'r': return ChessPieceType::ROOK;
            case 'B': case 'b': return ChessPieceType::BISHOP;
            case 'N': case 'n': return ChessPieceType::KNIGHT;
            default: return ChessPieceType::QUEEN; 
        }
    }
    return ChessPieceType::QUEEN; 
} 