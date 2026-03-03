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

std::vector<std::pair<int, int>> generatePawnMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateKnightMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateBishopMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateRookMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateQueenMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateKingMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> generateBitboardMoves(Board& board, ChessPieceColor color);

void addCastlingMovesBitboard(Board& board, ChessPieceColor color,
                              std::vector<std::pair<int, int>>* generatedMoves = nullptr);
