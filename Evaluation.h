#include "ChessBoard.h"


class Evaluation{
    const short PawnTable[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10,-20,-20, 10, 10,  5,
        5, -5,-10,  0,  0,-10, -5,  5,
        0,  0,  0,  0,  0,  0,  0,  0,
        5,  5, 10, 10, 10, 10,  5,  5,
        10, 10, 20, 20, 20, 20, 10, 10,
        50, 50, 50, 50, 50, 50, 50, 50,
        0,  0,  0,  0,  0,  0,  0,  0
    };


    const short KnightTable[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };


    const short BishopTable[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };


    const short RookTable[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    };


    const short KingTable[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
        20, 20,  0,  0,  0,  0, 20, 20,
        20, 30, 10,  0,  0, 10, 30, 20
    };


    const short KingTableEndGame[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };


    const short QueenTable[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    int EvaluatePieceScore(Square square, int pos, bool castled,
                          bool endGame, int bishopCount,
                          bool insufficientMaterial){
        int score = 0;
        int idx = pos;

        if (square.Piece.PieceColor == WHITE){
            idx = 63 - idx;
        }

        score += square.Piece.PieceValue;
        score += square.Piece.DefendedValue;
        score -= square.Piece.AttackedValue;

        // penalize hung pieces
        if (square.Piece.DefendedValue < square.Piece.AttackedValue){
            score -= ((square.Piece.DefendedValue - square.Piece.AttackedValue) * 10);
        }

        // promote mobility
        if (square.Piece.ValidMoves != 0){
            score += square.Piece.ValidMoves.count;
        }

        if (square.Piece.PieceType == PAWN){
            insufficientMaterial = false;
            // pawns on edges are worth less
            if (position % 8 ==0 || position % 8 == 7){
                score -= 15;
            }
            if (square.Piece.PieceColor == WHITE){
            // doubled pawns are bad
                if whitePawnCount[position % 8] > 0){
                    score -= 16;
                }
                if (position >= 8 && position <= 15){
                    if (square.Piece.AttackedValue == 0){
                        whitePawnCount[position % 8] += 200;
                    if (square.Piece.DefendedValue != 0){
                        whitePawnCount[position % 8] += 50;
                    }
                    }
                    }
                else if (position >= 16 && position <= 23){
                    if (square.Piece.AttackedValue == 0){
                        whitePawnCount[position % 8] += 100;
                    if (square.Piece.DefendedValue != 0){
                        whitePawnCount[position % 8] += 25;
                    }
                    }
                    }
                whitePawnCount[position % 8] += 10;
            }
        else if (square.Piece.PieceColor == BLACK){
            // doubled pawns are bad
            if blackPawnCount[position % 8] > 0){
                score -= 16;
            }
            // pawns are in the 6th row and not attacked are good
            if (position >= 48 && position <= 55){
                if (square.Piece.AttackedValue == 0){
                    blackPawnCount[position % 8] += 200;
                if (square.Piece.DefendedValue != 0){
                    blackPawnCount[position % 8] += 50;
                }
                }
                }
            // pawns are in the 6th row and not attacked are good
            else if (position >= 40 && position <= 47){
                if (square.Piece.AttackedValue == 0){
                    blackPawnCount[position % 8] += 100;
                if (square.Piece.DefendedValue != 0){
                    blackPawnCount[position % 8] += 25;
                }
                }
                blackPawnCount[position % 8] += 10;
                }
    }
    else if (square.Piece.PieceType = KNIGHT){
        score += KnightTable[idx];
        // knights less useful in endgame
        if (endGame){
            score -= 10;
        }
    }
    else if (square.Piece.PieceType = BISHOP){
        bishopCount++;
        // both bishops are better than one
        if (bishopCount > 1){
            score += 10;
        }
        score += BishopTable[idx];
    }
    else if (square.Piece.PieceType = ROOK){
        insufficientMaterial = false;
        // encourage castling
        if (square.Piece.moved && castled == false){
            score -= 10;
        }
        score += RookTable[idx];
    }
    else if (square.Piece.PieceType == QUEEN){
        insufficientMaterial = false;
        score += QueenTable[idx];
        // discourage moving early
        if (square.Piece.moved && !endGame){
            score -= 10;
        }
    }
    else if (square.Piece.PieceType == KING){
        // make sure king has moves
        if (square.Piece.ValidMoves.count < 2){
            score -= 5;
        }
        if (endGame){
            score += KingTableEndGame[idx];
        }
        else{
            score += KingTable[idx];
        }
        // encourage castling
        if (square.Piece.moved && castled){
            score -= 30;
        }
    }
    }
    return score;

}
    void EvaluateBoardScore(Board board){
        board.Score = 0;
        bool insufficientMaterial = true;
        // draws
        if (board.StaleMate){
            return;
        }
        if (board.FiftyMoveCount){
            return;
        }
        if (board.RepeatedMoveCount == 3){
            return;
        }
        if (board.blackCheckmated){
            board.score = 32767;
            return;
        }
        if (board.whiteCheckmated){
            board.score = -32767;
            return;
        }
        if (board.blackChecked){
            board.score += 10;
        }
        else if (board.whiteChecked){
            board.score -= 10;
        }
        if (board.blackCastled){
            board.score -= 40
        }
        if (board.whiteCastled){
            board.score += 40
        }
        // small bonus for tempo
        if (board.whoseTurn == WHITE){
            board.score += 10;
        }
        else{
            board.score -= 10;
        }
        short blackBishopCount = 0;
        short whiteBishopCount = 0;
        short knightCount = 0;

        short RemainingPieces = 0;

        short blackPawnCount[8] = {};
        short whitePawnCount[8] = {};

        for (int i = 0; i < 64; i++){
            Square square = board.Squares[i];
            if (sqare.Piece.PieceType == NONE){
                continue;
            }
            RemainingPieces++;
            if (square.Piece.PieceColor == WHITE){
                board.score += EvaluatePieceScore(square, i, board.whiteCastled,
                                                  board.EndGame, whiteBishopCount,
                                                  insufficientMaterial);
            }
            else{
                board.score -= EvaluatePieceScore(square, i, board.blackCastled,
                                                  board.EndGame, blackBishopCount,
                                                  insufficientMaterial);
            }
            if (square.Piece.PieceType == KNIGHT){
                knightCount++;
            }
            else if (square.Piece.PieceType == PAWN){
                if (square.Piece.PieceColor == WHITE){
                    whitePawnCount[i % 8]++;
                }
                else{
                    blackPawnCount[i % 8]++;
                }
            }
            else if (square.Piece.PieceType == BISHOP){
                if (square.Piece.PieceColor == WHITE){
                    whiteBishopCount++;
                }
                else{
                    blackBishopCount++;
                }
            }
            if (insufficientMaterial) {
                board.Score = 0;
                board.StaleMate = true;
                board.InsufficientMaterial = true;
            }
            if (RemainingPieces < 10){
                board.isEndGame = true;
                if (board.blackChecked){
                    board.score += 10;
                }
                else if (board.whiteChecked){
                    board.score -= 10;
                }
            }
        }
    //Black Isolated Pawns
    if (blackPawnCount[0] >= 1 && blackPawnCount[1] == 0)
        {
        board.Score += 12;
        }
    if (blackPawnCount[1] >= 1 && blackPawnCount[0] == 0 &&
    blackPawnCount[2] == 0)
        {
        board.Score += 14;
        }
    if (blackPawnCount[2] >= 1 && blackPawnCount[1] == 0 &&
    blackPawnCount[3] == 0)
        {
        board.Score += 16;
        }
    if (blackPawnCount[3] >= 1 && blackPawnCount[2] == 0 &&
    blackPawnCount[4] == 0)
        {
        board.Score += 20;
        }
    if (blackPawnCount[4] >= 1 && blackPawnCount[3] == 0 &&
    blackPawnCount[5] == 0)
        {
        board.Score += 20;
        }
    if (blackPawnCount[5] >= 1 && blackPawnCount[4] == 0 &&
    blackPawnCount[6] == 0)
        {
        board.Score += 16;
        }
    if (blackPawnCount[6] >= 1 && blackPawnCount[5] == 0 &&
    blackPawnCount[7] == 0)
        {
        board.Score += 14;
        }
    if (blackPawnCount[7] >= 1 && blackPawnCount[6] == 0)
        {
        board.Score += 12;
        }
    //White Isolated Pawns
    if (whitePawnCount[0] >= 1 && whitePawnCount[1] == 0)
        {
        board.Score -= 12;
        }
    if (whitePawnCount[1] >= 1 && whitePawnCount[0] == 0 &&
        whitePawnCount[2] == 0)
        {
        board.Score -= 14;
        }
    if (whitePawnCount[2] >= 1 && whitePawnCount[1] == 0 &&
    whitePawnCount[3] == 0)
        {
        board.Score -= 16;
        }
    if (whitePawnCount[3] >= 1 && whitePawnCount[2] == 0 &&
    whitePawnCount[4] == 0)
        {
        board.Score -= 20;
        }
    if (whitePawnCount[4] >= 1 && whitePawnCount[3] == 0 &&
    whitePawnCount[5] == 0)
        {
        board.Score -= 20;
        }
    if (whitePawnCount[5] >= 1 && whitePawnCount[4] == 0 &&
    whitePawnCount[6] == 0)
        {
        board.Score -= 16;
        }
    if (whitePawnCount[6] >= 1 && whitePawnCount[5] == 0 &&
    whitePawnCount[7] == 0)
        {
        board.Score -= 14;
        }
    if (whitePawnCount[7] >= 1 && whitePawnCount[6] == 0)
        {
        board.Score -= 12;
        }
    //Black Passed Pawns
    if (blackPawnCount[0] >= 1 && whitePawnCount[0] == 0)
        {
        board.Score -= blackPawnCount[0];
        }
    if (blackPawnCount[1] >= 1 && whitePawnCount[1] == 0)
        {
        board.Score -= blackPawnCount[1];
        }
    if (blackPawnCount[2] >= 1 && whitePawnCount[2] == 0)
        {
        board.Score -= blackPawnCount[2];
        }
    if (blackPawnCount[3] >= 1 && whitePawnCount[3] == 0)
        {
        board.Score -= blackPawnCount[3];
        }
    if (blackPawnCount[4] >= 1 && whitePawnCount[4] == 0)
        {
        board.Score -= blackPawnCount[4];
        }
    if (blackPawnCount[5] >= 1 && whitePawnCount[5] == 0)
        {
        board.Score -= blackPawnCount[5];
        }
    if (blackPawnCount[6] >= 1 && whitePawnCount[6] == 0)
        {
        board.Score -= blackPawnCount[6];
        }
    if (blackPawnCount[7] >= 1 && whitePawnCount[7] == 0)
        {
        board.Score -= blackPawnCount[7];
        }
    //White Passed Pawns
    if (whitePawnCount[0] >= 1 && blackPawnCount[0] == 0)
        {
        board.Score += whitePawnCount[0];
        }
    if (whitePawnCount[1] >= 1 && blackPawnCount[1] == 0)
        {
        board.Score += whitePawnCount[1];
        }
    if (whitePawnCount[2] >= 1 && blackPawnCount[2] == 0)
        {
        board.Score += whitePawnCount[2];
        }
    if (whitePawnCount[3] >= 1 && blackPawnCount[3] == 0)
        {
        board.Score += whitePawnCount[3];
        }
    if (whitePawnCount[4] >= 1 && blackPawnCount[4] == 0)
        {
        board.Score += whitePawnCount[4];
        }
    if (whitePawnCount[5] >= 1 && blackPawnCount[5] == 0)
        {
        board.Score += whitePawnCount[5];
        }
    if (whitePawnCount[6] >= 1 && blackPawnCount[6] == 0)
        {
        board.Score += whitePawnCount[6];
        }
    if (whitePawnCount[7] >= 1 && blackPawnCount[7] == 0)
        {
        board.Score += whitePawnCount[7];
        }
    }
};
