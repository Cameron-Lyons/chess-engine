#include "engine_globals.h"
#include <sstream>

std::map<std::string, std::string> OpeningBook = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"},
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "e7e5"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", "g1f3"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", "b8c6"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "f1c4"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", "f8c5"},
    // Extended book
    // Italian Game
    {"rnbqkbnr/pppp1ppp/8/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", "g8f6"},
    // Sicilian Defense
    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2", "g1f3"},
    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", "d7d6"},
    // French Defense
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "e7e6"},
    {"rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", "d2d4"},
    // Queen's Gambit
    {"rnbqkbnr/ppp1pppp/8/3p4/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 2", "e7e6"},
    {"rnbqkbnr/ppp1pppp/8/3p4/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 2", "c7c6"},
    // King's Indian Defense
    {"rnbqkb1r/pppppp1p/5np1/8/2P5/5N2/PP1PPPBP/RNBQK2R b KQkq - 3 3", "g7g6"},
    // Caro-Kann
    {"rnbqkbnr/pp2pppp/2p5/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3", "e4d5"},
    // Scandinavian Defense
    {"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", "e4d5"},
    // English Opening
    {"rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq - 0 1", "g8f6"},
    // Nimzo-Indian
    {"rnbqkb1r/pppppppp/5n2/8/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 1 2", "d2d4"},
};

uint64_t ZobristTable[64][12];
uint64_t ZobristBlackToMove;
ThreadSafeTT TransTable;

std::string getFEN(const Board& board) {
    std::string fen;
    for (int row = 7; row >= 0; --row) {
        int emptyCount = 0;
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
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
    
    // Handle castling
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
    
    // Determine piece type
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
    
    // Check for capture notation
    size_t xPos = cleanMove.find('x');
    if (xPos != std::string::npos) {
        isCapture = true;
    }
    
    // Handle simple pawn moves like "e4"
    if (cleanMove.length() == 2 && pieceType == ChessPieceType::PAWN) {
        destCol = cleanMove[0] - 'a';
        destRow = cleanMove[1] - '1';
        
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) {
            return false;
        }
        
        // Find the pawn that can move to this square
        if (board.turn == ChessPieceColor::WHITE) {
            // Check one square back
            if (destRow > 0) {
                int checkRow = destRow - 1;
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].Piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::WHITE) {
                    // Check if destination is empty
                    int destPos = destRow * 8 + destCol;
                    if (board.squares[destPos].Piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
            
            // Check two squares back (for initial pawn moves)
            if (destRow == 3) { // Moving to 4th rank
                int checkRow = 1; // From 2nd rank
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].Piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::WHITE) {
                    // Check if both destination and intermediate square are empty
                    int destPos = destRow * 8 + destCol;
                    int intermediatPos = 2 * 8 + destCol;
                    if (board.squares[destPos].Piece.PieceType == ChessPieceType::NONE && 
                        board.squares[intermediatPos].Piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        } else { // Black's turn
            // Check one square forward
            if (destRow < 7) {
                int checkRow = destRow + 1;
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].Piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::BLACK) {
                    // Check if destination is empty
                    int destPos = destRow * 8 + destCol;
                    if (board.squares[destPos].Piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
            
            // Check two squares forward (for initial pawn moves)
            if (destRow == 4) { // Moving to 5th rank
                int checkRow = 6; // From 7th rank
                int pos = checkRow * 8 + destCol;
                const Piece& piece = board.squares[pos].Piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessPieceColor::BLACK) {
                    // Check if both destination and intermediate square are empty
                    int destPos = destRow * 8 + destCol;
                    int intermediatPos = 5 * 8 + destCol;
                    if (board.squares[destPos].Piece.PieceType == ChessPieceType::NONE && 
                        board.squares[intermediatPos].Piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    // Handle pawn captures like "exd4"
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || srcCol < 0 || srcCol >= 8) return false;
        
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            const Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                int from = pos;
                int to = destRow * 8 + destCol;
                if (board.turn == ChessPieceColor::WHITE) {
                    if ((to == from + 7 && srcCol > 0) || (to == from + 9 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::BLACK) {
                            srcRow = row;
                            return true;
                        }
                    }
                } else {
                    if ((to == from - 9 && srcCol > 0) || (to == from - 7 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::WHITE) {
                            srcRow = row;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    
    // Handle other piece moves (Knight, Bishop, Rook, Queen, King)
    if (pieceType != ChessPieceType::PAWN) {
        if (cleanMove.length() < startPos + 2) return false;
        
        // Parse destination square
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
        const Piece& destPiece = board.squares[destPos].Piece;
        
        // Check if destination is valid
        if (isCapture) {
            if (destPiece.PieceType == ChessPieceType::NONE || destPiece.PieceColor == board.turn) {
                return false; // Can't capture empty square or own piece
            }
        } else {
            if (destPiece.PieceType != ChessPieceType::NONE) {
                return false; // Can't move to occupied square without capture notation
            }
        }
        
        // Find pieces of the right type that can reach the destination
        std::vector<std::pair<int, int>> candidates;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int pos = row * 8 + col;
                const Piece& piece = board.squares[pos].Piece;
                
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
                                    if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) {
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
                                        if (board.squares[row * 8 + c].Piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else {
                                    int start = std::min(row, destRow) + 1;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[r * 8 + col].Piece.PieceType != ChessPieceType::NONE) {
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
                            
                            // Queen moves like rook or bishop
                            if (row == destRow || col == destCol || rowDiff == colDiff) {
                                canReach = true;
                                
                                if (row == destRow) {
                                    // Horizontal movement
                                    int start = std::min(col, destCol) + 1;
                                    int end = std::max(col, destCol);
                                    for (int c = start; c < end; c++) {
                                        if (board.squares[row * 8 + c].Piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (col == destCol) {
                                    // Vertical movement
                                    int start = std::min(row, destRow) + 1;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[r * 8 + col].Piece.PieceType != ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (rowDiff == colDiff) {
                                    // Diagonal movement
                                    int rowStep = (destRow > row) ? 1 : -1;
                                    int colStep = (destCol > col) ? 1 : -1;
                                    for (int i = 1; i < rowDiff; i++) {
                                        int checkPos = (row + i * rowStep) * 8 + (col + i * colStep);
                                        if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) {
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
            // TODO: Handle disambiguation (like Nbd2, R1e1, etc.)
            // For now, just return the first candidate
            srcCol = candidates[0].first;
            srcRow = candidates[0].second;
            return true;
        }
    }
    
    return false;
} 