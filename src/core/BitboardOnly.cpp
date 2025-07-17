#include "BitboardOnly.h"
#include <sstream>
#include <cctype>

// Zobrist random numbers (should be initialized once)
static uint64_t zobristPieces[2][6][64];
static uint64_t zobristSideToMove;
static uint64_t zobristCastling[16];
static uint64_t zobristEnPassant[8];
static bool zobristInitialized = false;

static void initializeZobrist() {
    if (zobristInitialized) return;
    
    // Simple PRNG for deterministic initialization
    uint64_t seed = 0x123456789ABCDEF0ULL;
    auto next = [&seed]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    };
    
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            for (int square = 0; square < 64; ++square) {
                zobristPieces[color][piece][square] = next();
            }
        }
    }
    
    zobristSideToMove = next();
    
    for (int i = 0; i < 16; ++i) {
        zobristCastling[i] = next();
    }
    
    for (int i = 0; i < 8; ++i) {
        zobristEnPassant[i] = next();
    }
    
    zobristInitialized = true;
}

BitboardPosition::BitboardPosition() {
    initializeZobrist();
    
    // Clear all bitboards
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            pieces[color][piece] = 0;
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        occupancy[i] = 0;
    }
    
    sideToMove = WHITE;
    castlingRights = 0xF;  // KQkq all allowed
    epSquare = 64;  // No en passant
    halfmoveClock = 0;
    fullmoveNumber = 1;
    hash = 0;
    lastMoveTime = ChessClock::now();
}

void BitboardPosition::updateOccupancy() {
    occupancy[WHITE] = pieces[WHITE][PAWN] | pieces[WHITE][KNIGHT] | pieces[WHITE][BISHOP] |
                      pieces[WHITE][ROOK] | pieces[WHITE][QUEEN] | pieces[WHITE][KING];
    
    occupancy[BLACK] = pieces[BLACK][PAWN] | pieces[BLACK][KNIGHT] | pieces[BLACK][BISHOP] |
                      pieces[BLACK][ROOK] | pieces[BLACK][QUEEN] | pieces[BLACK][KING];
    
    occupancy[2] = occupancy[WHITE] | occupancy[BLACK];
}

void BitboardPosition::removePiece(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type) - 1;
    
    pieces[c][p] &= ~(1ULL << square);
    updateHash(square, color, type);
}

void BitboardPosition::placePiece(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type) - 1;
    
    pieces[c][p] |= (1ULL << square);
    updateHash(square, color, type);
}

void BitboardPosition::updateHash(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type) - 1;
    
    hash ^= zobristPieces[c][p][square];
}

void BitboardPosition::makeMove(int from, int to, ChessPieceType promotion) {
    ChessPieceType movingPiece = getPieceAt(from);
    ChessPieceColor movingColor = getColorAt(from);
    ChessPieceType capturedPiece = getPieceAt(to);
    
    // Remove piece from source
    removePiece(from, movingColor, movingPiece);
    
    // Remove captured piece if any
    if (capturedPiece != ChessPieceType::NONE) {
        ChessPieceColor capturedColor = getColorAt(to);
        removePiece(to, capturedColor, capturedPiece);
    }
    
    // Handle promotion
    ChessPieceType finalPiece = (promotion != ChessPieceType::NONE) ? promotion : movingPiece;
    
    // Place piece at destination
    placePiece(to, movingColor, finalPiece);
    
    // Handle en passant capture
    if (movingPiece == ChessPieceType::PAWN && to == epSquare && epSquare < 64) {
        int captureSquare = (movingColor == ChessPieceColor::WHITE) ? to - 8 : to + 8;
        ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? 
                                     ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        removePiece(captureSquare, enemyColor, ChessPieceType::PAWN);
    }
    
    // Update en passant square
    if (movingPiece == ChessPieceType::PAWN && abs(to - from) == 16) {
        epSquare = (from + to) / 2;
    } else {
        epSquare = 64;
    }
    
    // Handle castling
    if (movingPiece == ChessPieceType::KING && abs(to - from) == 2) {
        int rookFrom, rookTo;
        if (to > from) {  // Kingside
            rookFrom = to + 1;
            rookTo = to - 1;
        } else {  // Queenside
            rookFrom = to - 2;
            rookTo = to + 1;
        }
        
        removePiece(rookFrom, movingColor, ChessPieceType::ROOK);
        placePiece(rookTo, movingColor, ChessPieceType::ROOK);
    }
    
    // Update castling rights
    if (movingPiece == ChessPieceType::KING) {
        if (movingColor == ChessPieceColor::WHITE) {
            castlingRights &= ~0x3;  // Clear white castling rights
        } else {
            castlingRights &= ~0xC;  // Clear black castling rights
        }
    } else if (movingPiece == ChessPieceType::ROOK) {
        if (from == 0) castlingRights &= ~0x2;  // White queenside
        else if (from == 7) castlingRights &= ~0x1;  // White kingside
        else if (from == 56) castlingRights &= ~0x8;  // Black queenside
        else if (from == 63) castlingRights &= ~0x4;  // Black kingside
    }
    
    // Update move counters
    if (movingPiece == ChessPieceType::PAWN || capturedPiece != ChessPieceType::NONE) {
        halfmoveClock = 0;
    } else {
        halfmoveClock++;
    }
    
    if (sideToMove == BLACK) {
        fullmoveNumber++;
    }
    
    // Switch side to move
    sideToMove = 1 - sideToMove;
    hash ^= zobristSideToMove;
    
    // Update occupancy
    updateOccupancy();
    
    // Record move time
    recordMoveTime();
}

void BitboardPosition::unmakeMove(int from, int to, ChessPieceType captured, ChessPieceType promotion) {
    // This is a simplified version - full unmake would need more state saved
    ChessPieceType movedPiece = getPieceAt(to);
    ChessPieceColor movedColor = getColorAt(to);
    
    // Remove piece from destination
    removePiece(to, movedColor, movedPiece);
    
    // Restore original piece at source
    ChessPieceType originalPiece = (promotion != ChessPieceType::NONE) ? ChessPieceType::PAWN : movedPiece;
    placePiece(from, movedColor, originalPiece);
    
    // Restore captured piece
    if (captured != ChessPieceType::NONE) {
        ChessPieceColor capturedColor = (movedColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        placePiece(to, capturedColor, captured);
    }
    
    // Switch side back
    sideToMove = 1 - sideToMove;
    hash ^= zobristSideToMove;
    
    updateOccupancy();
}

void BitboardPosition::setFromFEN(const std::string& fen) {
    // Clear board
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            pieces[color][piece] = 0;
        }
    }
    
    std::istringstream ss(fen);
    std::string board, turn, castling, ep;
    
    ss >> board >> turn >> castling >> ep >> halfmoveClock >> fullmoveNumber;
    
    // Parse board
    int rank = 7, file = 0;
    for (char c : board) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += c - '0';
        } else {
            int square = rank * 8 + file;
            ChessPieceColor color = isupper(c) ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
            ChessPieceType type;
            
            switch (tolower(c)) {
                case 'p': type = ChessPieceType::PAWN; break;
                case 'n': type = ChessPieceType::KNIGHT; break;
                case 'b': type = ChessPieceType::BISHOP; break;
                case 'r': type = ChessPieceType::ROOK; break;
                case 'q': type = ChessPieceType::QUEEN; break;
                case 'k': type = ChessPieceType::KING; break;
                default: continue;
            }
            
            placePiece(square, color, type);
            file++;
        }
    }
    
    // Parse turn
    sideToMove = (turn == "w") ? WHITE : BLACK;
    
    // Parse castling rights
    castlingRights = 0;
    if (castling.find('K') != std::string::npos) castlingRights |= 1;
    if (castling.find('Q') != std::string::npos) castlingRights |= 2;
    if (castling.find('k') != std::string::npos) castlingRights |= 4;
    if (castling.find('q') != std::string::npos) castlingRights |= 8;
    
    // Parse en passant
    if (ep != "-" && ep.length() >= 2) {
        int file = ep[0] - 'a';
        int rank = ep[1] - '1';
        epSquare = rank * 8 + file;
    } else {
        epSquare = 64;
    }
    
    updateOccupancy();
    
    // Recalculate hash
    hash = 0;
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            Bitboard bb = pieces[color][piece];
            while (bb) {
                int square = __builtin_ctzll(bb);
                hash ^= zobristPieces[color][piece][square];
                bb &= bb - 1;
            }
        }
    }
    
    if (sideToMove == BLACK) hash ^= zobristSideToMove;
    hash ^= zobristCastling[castlingRights];
    if (epSquare < 64) hash ^= zobristEnPassant[epSquare & 7];
}

std::string BitboardPosition::toFEN() const {
    std::stringstream ss;
    
    // Board
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            ChessPieceType piece = getPieceAt(square);
            
            if (piece == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    ss << emptyCount;
                    emptyCount = 0;
                }
                
                char pieceChar;
                switch (piece) {
                    case ChessPieceType::PAWN: pieceChar = 'p'; break;
                    case ChessPieceType::KNIGHT: pieceChar = 'n'; break;
                    case ChessPieceType::BISHOP: pieceChar = 'b'; break;
                    case ChessPieceType::ROOK: pieceChar = 'r'; break;
                    case ChessPieceType::QUEEN: pieceChar = 'q'; break;
                    case ChessPieceType::KING: pieceChar = 'k'; break;
                    default: pieceChar = '?';
                }
                
                if (getColorAt(square) == ChessPieceColor::WHITE) {
                    pieceChar = toupper(pieceChar);
                }
                
                ss << pieceChar;
            }
        }
        
        if (emptyCount > 0) {
            ss << emptyCount;
        }
        
        if (rank > 0) {
            ss << '/';
        }
    }
    
    // Side to move
    ss << ' ' << (sideToMove == WHITE ? 'w' : 'b');
    
    // Castling rights
    ss << ' ';
    std::string castling;
    if (castlingRights & 1) castling += 'K';
    if (castlingRights & 2) castling += 'Q';
    if (castlingRights & 4) castling += 'k';
    if (castlingRights & 8) castling += 'q';
    ss << (castling.empty() ? "-" : castling);
    
    // En passant
    ss << ' ';
    if (epSquare < 64) {
        ss << char('a' + (epSquare & 7)) << char('1' + (epSquare >> 3));
    } else {
        ss << '-';
    }
    
    // Move counters
    ss << ' ' << int(halfmoveClock) << ' ' << fullmoveNumber;
    
    return ss.str();
}

std::string BitboardPosition::toString() const {
    std::stringstream ss;
    
    ss << "  a b c d e f g h\n";
    for (int rank = 7; rank >= 0; --rank) {
        ss << (rank + 1) << ' ';
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            ChessPieceType piece = getPieceAt(square);
            
            if (piece == ChessPieceType::NONE) {
                ss << ". ";
            } else {
                char pieceChar;
                switch (piece) {
                    case ChessPieceType::PAWN: pieceChar = 'p'; break;
                    case ChessPieceType::KNIGHT: pieceChar = 'n'; break;
                    case ChessPieceType::BISHOP: pieceChar = 'b'; break;
                    case ChessPieceType::ROOK: pieceChar = 'r'; break;
                    case ChessPieceType::QUEEN: pieceChar = 'q'; break;
                    case ChessPieceType::KING: pieceChar = 'k'; break;
                    default: pieceChar = '?';
                }
                
                if (getColorAt(square) == ChessPieceColor::WHITE) {
                    pieceChar = toupper(pieceChar);
                }
                
                ss << pieceChar << ' ';
            }
        }
        ss << (rank + 1) << '\n';
    }
    ss << "  a b c d e f g h\n";
    
    ss << "Turn: " << (sideToMove == WHITE ? "White" : "Black") << '\n';
    ss << "FEN: " << toFEN() << '\n';
    
    return ss.str();
}