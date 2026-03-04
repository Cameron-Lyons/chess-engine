#pragma once

#include "../core/BitboardMoves.h"
#include "../core/ChessBoard.h"

#include <vector>

constexpr int kValidMovesBoardSquareCount = 64;

extern thread_local bool BlackAttackBoard[kValidMovesBoardSquareCount];
extern thread_local bool WhiteAttackBoard[kValidMovesBoardSquareCount];
extern thread_local int BlackKingPosition;
extern thread_local int WhiteKingPosition;

bool AnalyzeMove(Board& board, int dest, Piece& piece);
void CheckValidMovesPawn(const std::vector<int>& moves, Piece& piece, int start, Board& board,
                         int count);
void AnalyzeMovePawn(Board& board, int dest, Piece& piece);
void GenValidMovesKingCastle(Board& board, Piece& king);
void GenValidMoves(Board& board);
bool IsMoveLegal(Board& board, int srcPos, int destPos);
bool IsKingInCheck(const Board& board, ChessPieceColor color);

std::vector<Move> generatePawnMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateKnightMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateBishopMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateRookMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateQueenMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateKingMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateBitboardMoves(Board& board, ChessPieceColor color);

void addCastlingMovesBitboard(Board& board, ChessPieceColor color,
                              std::vector<Move>* generatedMoves = nullptr);
