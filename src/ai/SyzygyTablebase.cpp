#include "SyzygyTablebase.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#ifdef __BMI2__
#include <x86intrin.h>
#endif

namespace Syzygy {

// Syzygy file format constants
constexpr int TB_PIECES_MAX = 7;
constexpr int TB_HASHBITS = 10;
constexpr int TB_MAX_PIECE = 6;
constexpr int TB_MAX_PAWN = 4;

// Global tablebase data
static std::string tbPath;
static int tbMaxPieces = 0;
static bool tbInitialized = false;

// Tablebase file entry
struct TBEntry {
    std::string name;
    void* data;
    uint64_t size;
    int pieceCount;
    bool hasPawns;
};

static std::vector<TBEntry> tbEntries;

// Helper functions
static int popcount(uint64_t bb) {
    return __builtin_popcountll(bb);
}

static int lsb(uint64_t bb) {
    return __builtin_ctzll(bb);
}

static uint64_t pext(uint64_t val, uint64_t mask) {
#ifdef __BMI2__
    return _pext_u64(val, mask);
#else
    uint64_t res = 0;
    for (uint64_t bb = 1; mask; bb += bb) {
        if (val & mask & -mask)
            res |= bb;
        mask &= mask - 1;
    }
    return res;
#endif
}

Position Position::fromBoard(const Board& board) {
    Position pos;
    pos.white = 0;
    pos.black = 0;
    pos.kings = 0;
    pos.queens = 0;
    pos.rooks = 0;
    pos.bishops = 0;
    pos.knights = 0;
    pos.pawns = 0;
    
    // Convert board to bitboards
    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        uint64_t bb = 1ULL << sq;
        
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            pos.white |= bb;
        } else {
            pos.black |= bb;
        }
        
        switch (piece.PieceType) {
            case ChessPieceType::PAWN:   pos.pawns |= bb; break;
            case ChessPieceType::KNIGHT: pos.knights |= bb; break;
            case ChessPieceType::BISHOP: pos.bishops |= bb; break;
            case ChessPieceType::ROOK:   pos.rooks |= bb; break;
            case ChessPieceType::QUEEN:  pos.queens |= bb; break;
            case ChessPieceType::KING:   pos.kings |= bb; break;
            default: break;
        }
    }
    
    pos.turn = (board.turn == ChessPieceColor::WHITE);
    pos.castling = 0;
    if (board.whiteCanCastle) pos.castling |= 0x3;
    if (board.blackCanCastle) pos.castling |= 0xC;
    pos.ep = 0; // No en passant in tablebases
    pos.rule50 = 0;
    
    return pos;
}

bool init(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    tbPath = path;
    tbEntries.clear();
    
    // Scan for tablebase files
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            
            // Check for Syzygy file extensions (.rtbw, .rtbz)
            if (filename.ends_with(".rtbw") || filename.ends_with(".rtbz")) {
                // Parse filename to get piece configuration
                TBEntry tb;
                tb.name = filename.substr(0, filename.find('.'));
                tb.data = nullptr;
                tb.size = entry.file_size();
                
                // Count pieces from filename (e.g., "KQvK" = 3 pieces)
                tb.pieceCount = 0;
                tb.hasPawns = false;
                for (char c : tb.name) {
                    if (c == 'K' || c == 'Q' || c == 'R' || 
                        c == 'B' || c == 'N') {
                        tb.pieceCount++;
                    } else if (c == 'P') {
                        tb.pieceCount++;
                        tb.hasPawns = true;
                    }
                }
                
                if (tb.pieceCount <= TB_PIECES_MAX) {
                    tbEntries.push_back(tb);
                    tbMaxPieces = std::max(tbMaxPieces, tb.pieceCount);
                }
            }
        }
    }
    
    tbInitialized = !tbEntries.empty();
    return tbInitialized;
}

bool canProbe(const Board& board) {
    if (!tbInitialized) return false;
    
    // Count pieces
    int pieceCount = 0;
    
    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            pieceCount++;
        }
    }
    
    // Check if we have tablebase for this piece count
    if (pieceCount > tbMaxPieces) return false;
    
    // Check for castling rights (not supported in tablebases)
    if (board.whiteCanCastle || board.blackCanCastle) return false;
    
    return true;
}

ProbeResult probeWDL(const Board& board, int* success) {
    *success = 0;
    
    if (!canProbe(board)) {
        return PROBE_FAIL;
    }
    
    Position pos = Position::fromBoard(board);
    
    // Simplified WDL probe
    // In a real implementation, this would:
    // 1. Find the correct tablebase file
    // 2. Calculate index based on piece positions
    // 3. Read compressed data
    // 4. Return actual WDL result
    
    // For now, return a simple heuristic based on material
    int whiteMaterial = popcount(pos.queens) * 9 + 
                       popcount(pos.rooks) * 5 + 
                       popcount(pos.bishops) * 3 + 
                       popcount(pos.knights) * 3 + 
                       popcount(pos.pawns);
    int blackMaterial = popcount(pos.queens & pos.black) * 9 + 
                       popcount(pos.rooks & pos.black) * 5 + 
                       popcount(pos.bishops & pos.black) * 3 + 
                       popcount(pos.knights & pos.black) * 3 + 
                       popcount(pos.pawns & pos.black);
    
    int whitePieces = popcount(pos.white);
    int blackPieces = popcount(pos.black);
    
    whiteMaterial -= blackMaterial;
    whitePieces -= blackPieces;
    
    *success = 1;
    
    // Basic endgame knowledge
    if (whiteMaterial == 0 && blackMaterial == 0) {
        // Only kings
        return PROBE_DRAW;
    }
    
    if (pos.turn) {
        if (whiteMaterial > 4 && whitePieces > blackPieces) return PROBE_WIN;
        if (whiteMaterial < -4 && whitePieces < blackPieces) return PROBE_LOSS;
    } else {
        if (blackMaterial > 4 && blackPieces > whitePieces) return PROBE_WIN;
        if (blackMaterial < -4 && blackPieces < whitePieces) return PROBE_LOSS;
    }
    
    return PROBE_DRAW;
}

int probeDTZ(const Board& board, int* success) {
    *success = 0;
    
    if (!canProbe(board)) {
        return 0;
    }
    
    // Simplified DTZ probe
    // Real implementation would probe actual DTZ tables
    ProbeResult wdl = probeWDL(board, success);
    
    if (*success && wdl != PROBE_DRAW) {
        // Return a dummy DTZ value
        return (wdl == PROBE_WIN || wdl == PROBE_CURSED_WIN) ? 10 : -10;
    }
    
    return 0;
}

bool probeRoot(const Board& board, DTZResult& result) {
    if (!canProbe(board)) {
        return false;
    }
    
    // This would normally:
    // 1. Generate all legal moves
    // 2. Probe DTZ for each resulting position
    // 3. Select the best move based on DTZ values
    
    // Initialize result with default values
    result.dtz = 0;
    result.wdl = PROBE_FAIL;
    result.from = -1;
    result.to = -1;
    result.promo = 0;
    
    // For now, return false to indicate no tablebase move found
    return false;
}

int maxPieces() {
    return tbMaxPieces;
}

const std::string& getPath() {
    return tbPath;
}

} // namespace Syzygy