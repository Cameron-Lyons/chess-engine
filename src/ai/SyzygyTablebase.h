#ifndef SYZYGY_TABLEBASE_H
#define SYZYGY_TABLEBASE_H

#include "../core/ChessBoard.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Syzygy {

// Syzygy probe result types
enum ProbeResult {
    PROBE_FAIL = 0,
    PROBE_LOSS = 1,
    PROBE_BLESSED_LOSS = 2,
    PROBE_DRAW = 3,
    PROBE_CURSED_WIN = 4,
    PROBE_WIN = 5
};

// DTZ probe result
struct DTZResult {
    int dtz;           // Distance to zeroing move
    ProbeResult wdl;   // Win/Draw/Loss
    int from;          // Source square
    int to;            // Destination square
    int promo;         // Promotion piece (0 if none)
};

// Initialize Syzygy tablebases
bool init(const std::string& path);

// Probe WDL (Win/Draw/Loss) tables
ProbeResult probeWDL(const Board& board, int* success);

// Probe DTZ (Distance to Zero) tables
int probeDTZ(const Board& board, int* success);

// Get best move from DTZ tables
bool probeRoot(const Board& board, DTZResult& result);

// Check if position is in tablebase
bool canProbe(const Board& board);

// Get number of pieces supported
int maxPieces();

// Get tablebase path
const std::string& getPath();

// Internal representation for efficient probing
struct Position {
    uint64_t white;
    uint64_t black;
    uint64_t kings;
    uint64_t queens;
    uint64_t rooks;
    uint64_t bishops;
    uint64_t knights;
    uint64_t pawns;
    uint8_t castling;
    uint8_t ep;
    uint8_t rule50;
    bool turn;
    
    static Position fromBoard(const Board& board);
};

} // namespace Syzygy

#endif // SYZYGY_TABLEBASE_H