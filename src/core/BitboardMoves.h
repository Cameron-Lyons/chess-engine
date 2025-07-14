#ifndef BITBOARD_MOVES_H
#define BITBOARD_MOVES_H

#include "Bitboard.h"
#include "ChessPiece.h"
#include "MagicBitboards.h"
#include <array>

extern std::array<Bitboard, 64> KnightAttacks;
extern std::array<Bitboard, 64> KingAttacks;

void initKnightAttacks();
void initKingAttacks();

Bitboard pawnAttacks(ChessPieceColor color, int sq);

// Legacy functions for backward compatibility
Bitboard rookAttacks(int sq, Bitboard occupancy);
Bitboard bishopAttacks(int sq, Bitboard occupancy);
Bitboard queenAttacks(int sq, Bitboard occupancy);

// Fast magic bitboard functions
Bitboard fastRookAttacks(int sq, Bitboard occupancy);
Bitboard fastBishopAttacks(int sq, Bitboard occupancy);
Bitboard fastQueenAttacks(int sq, Bitboard occupancy);

Bitboard knightMoves(Bitboard knights, Bitboard ownPieces);
Bitboard kingMoves(Bitboard king, Bitboard ownPieces);
Bitboard pawnPushes(Bitboard pawns, Bitboard empty, ChessPieceColor color);
Bitboard pawnCaptures(Bitboard pawns, Bitboard enemyPieces, ChessPieceColor color);
Bitboard rookMoves(Bitboard rooks, Bitboard ownPieces, Bitboard occupancy);
Bitboard bishopMoves(Bitboard bishops, Bitboard ownPieces, Bitboard occupancy);
Bitboard queenMoves(Bitboard queens, Bitboard ownPieces, Bitboard occupancy);

#endif