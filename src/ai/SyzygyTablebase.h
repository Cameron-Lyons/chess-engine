#ifndef SYZYGY_TABLEBASE_H
#define SYZYGY_TABLEBASE_H

#include "../core/ChessBoard.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Syzygy {

enum ProbeResult {
    PROBE_FAIL = 0,
    PROBE_LOSS = 1,
    PROBE_BLESSED_LOSS = 2,
    PROBE_DRAW = 3,
    PROBE_CURSED_WIN = 4,
    PROBE_WIN = 5
};

struct DTZResult {
    int dtz;
    ProbeResult wdl;
    int from;
    int to;
    int promo;
};

bool init(const std::string& path);

ProbeResult probeWDL(const Board& board, int* success);

int probeDTZ(const Board& board, int* success);

bool probeRoot(const Board& board, DTZResult& result);

bool canProbe(const Board& board);

int maxPieces();

const std::string& getPath();

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

} 

#endif