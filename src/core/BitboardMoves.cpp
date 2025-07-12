#include "BitboardMoves.h"

std::array<Bitboard, 64> KnightAttacks;
std::array<Bitboard, 64> KingAttacks;

void initKnightAttacks() {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard attacks = 0;
        int rank = sq / 8, file = sq % 8;
        
        int moves[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
        
        for (int i = 0; i < 8; i++) {
            int newRank = rank + moves[i][0];
            int newFile = file + moves[i][1];
            if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                set_bit(attacks, newRank * 8 + newFile);
            }
        }
        KnightAttacks[sq] = attacks;
    }
}

void initKingAttacks() {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard attacks = 0;
        int rank = sq / 8, file = sq % 8;
        
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;
                int newRank = rank + dr;
                int newFile = file + df;
                if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                    set_bit(attacks, newRank * 8 + newFile);
                }
            }
        }
        KingAttacks[sq] = attacks;
    }
}

Bitboard pawnAttacks(ChessPieceColor color, int sq) {
    Bitboard attacks = 0;
    int rank = sq / 8, file = sq % 8;
    
    if (color == ChessPieceColor::WHITE) {
        if (file > 0 && rank < 7) set_bit(attacks, (rank + 1) * 8 + (file - 1));
        if (file < 7 && rank < 7) set_bit(attacks, (rank + 1) * 8 + (file + 1));
    } else {
        if (file > 0 && rank > 0) set_bit(attacks, (rank - 1) * 8 + (file - 1));
        if (file < 7 && rank > 0) set_bit(attacks, (rank - 1) * 8 + (file + 1));
    }
    return attacks;
}

Bitboard rookAttacks(int sq, Bitboard occupancy) {
    Bitboard attacks = 0;
    int rank = sq / 8, file = sq % 8;
    
    for (int f = file + 1; f < 8; f++) {
        int target = rank * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    for (int f = file - 1; f >= 0; f--) {
        int target = rank * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    
    for (int r = rank + 1; r < 8; r++) {
        int target = r * 8 + file;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    for (int r = rank - 1; r >= 0; r--) {
        int target = r * 8 + file;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    
    return attacks;
}

Bitboard bishopAttacks(int sq, Bitboard occupancy) {
    Bitboard attacks = 0;
    int rank = sq / 8, file = sq % 8;
    
    for (int r = rank + 1, f = file + 1; r < 8 && f < 8; r++, f++) {
        int target = r * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    for (int r = rank + 1, f = file - 1; r < 8 && f >= 0; r++, f--) {
        int target = r * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    for (int r = rank - 1, f = file + 1; r >= 0 && f < 8; r--, f++) {
        int target = r * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--) {
        int target = r * 8 + f;
        set_bit(attacks, target);
        if (get_bit(occupancy, target)) break;
    }
    
    return attacks;
}

Bitboard queenAttacks(int sq, Bitboard occupancy) {
    return rookAttacks(sq, occupancy) | bishopAttacks(sq, occupancy);
}

Bitboard knightMoves(Bitboard knights, Bitboard ownPieces) {
    Bitboard moves = 0;
    while (knights) {
        int sq = lsb(knights);
        moves |= KnightAttacks[sq];
        clear_bit(knights, sq);
    }
    return moves & ~ownPieces;
}

Bitboard kingMoves(Bitboard king, Bitboard ownPieces) {
    int sq = lsb(king);
    return KingAttacks[sq] & ~ownPieces;
}

Bitboard pawnPushes(Bitboard pawns, Bitboard empty, ChessPieceColor color) {
    Bitboard pushes = 0;
    if (color == ChessPieceColor::WHITE) {
        pushes = (pawns << 8) & empty;
        pushes |= ((pushes & 0x0000000000FF0000ULL) << 8) & empty;
    } else {
        pushes = (pawns >> 8) & empty;
        pushes |= ((pushes & 0x0000FF0000000000ULL) >> 8) & empty;
    }
    return pushes;
}

Bitboard pawnCaptures(Bitboard pawns, Bitboard enemyPieces, ChessPieceColor color) {
    Bitboard captures = 0;
    if (color == ChessPieceColor::WHITE) {
        captures = ((pawns << 7) & ~0x0101010101010101ULL) | ((pawns << 9) & ~0x8080808080808080ULL);
    } else {
        captures = ((pawns >> 7) & ~0x8080808080808080ULL) | ((pawns >> 9) & ~0x0101010101010101ULL);
    }
    return captures & enemyPieces;
}

Bitboard rookMoves(Bitboard rooks, Bitboard ownPieces, Bitboard occupancy) {
    Bitboard moves = 0;
    while (rooks) {
        int sq = lsb(rooks);
        moves |= rookAttacks(sq, occupancy);
        clear_bit(rooks, sq);
    }
    return moves & ~ownPieces;
}

Bitboard bishopMoves(Bitboard bishops, Bitboard ownPieces, Bitboard occupancy) {
    Bitboard moves = 0;
    while (bishops) {
        int sq = lsb(bishops);
        moves |= bishopAttacks(sq, occupancy);
        clear_bit(bishops, sq);
    }
    return moves & ~ownPieces;
}

Bitboard queenMoves(Bitboard queens, Bitboard ownPieces, Bitboard occupancy) {
    Bitboard moves = 0;
    while (queens) {
        int sq = lsb(queens);
        moves |= queenAttacks(sq, occupancy);
        clear_bit(queens, sq);
    }
    return moves & ~ownPieces;
} 