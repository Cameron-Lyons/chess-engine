#pragma once

#include "../core/BitboardMoves.h"
#include "../core/ChessBoard.h"

#include <vector>

constexpr int kValidMovesBoardSquareCount = 64;

void UpdateCheckState(Board& board);
bool IsMoveLegal(Board& board, int srcPos, int destPos);
bool IsKingInCheck(const Board& board, ChessPieceColor color);

std::vector<Move> generatePawnMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateKnightMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateBishopMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateRookMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateQueenMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateKingMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateBitboardMoves(Board& board, ChessPieceColor color);
std::vector<Move> generateBitboardCaptureMoves(Board& board, ChessPieceColor color);
bool hasAnyLegalMove(Board& board, ChessPieceColor color);

void addCastlingMovesBitboard(Board& board, ChessPieceColor color,
                              std::vector<Move>* generatedMoves = nullptr);
