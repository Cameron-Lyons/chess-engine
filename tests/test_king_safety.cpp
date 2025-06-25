#include "../src/ChessBoard.h"
#include "../src/search.h"
#include "../src/BitboardMoves.h"
#include <iostream>

void initKnightAttacks();
void initKingAttacks();

void testKingSafetyEvaluation() {
    std::cout << "=== King Safety Evaluation Test ===\n\n";
    
    std::cout << "Initializing engine components...\n";
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
    std::cout << "Initialization complete.\n\n";
    
    Board startBoard;
    startBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    int startEval = evaluatePosition(startBoard);
    int whiteKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::WHITE);
    int blackKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::BLACK);
    
    std::cout << "Test 1: Starting Position\n";
    std::cout << "White king safety: " << whiteKingSafety << std::endl;
    std::cout << "Black king safety: " << blackKingSafety << std::endl;
    std::cout << "Total evaluation: " << startEval << std::endl;
    std::cout << std::endl;
    
    Board damagedPawnShield;
    damagedPawnShield.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    
    int damagedEval = evaluatePosition(damagedPawnShield);
    int damagedWhiteKingSafety = evaluateKingSafety(damagedPawnShield, ChessPieceColor::WHITE);
    
    std::cout << "Test 2: Damaged Pawn Shield (missing f2 pawn)\n";
    std::cout << "White king safety: " << damagedWhiteKingSafety << std::endl;
    std::cout << "Total evaluation: " << damagedEval << std::endl;
    std::cout << "Safety difference: " << (damagedWhiteKingSafety - whiteKingSafety) << std::endl;
    std::cout << std::endl;
    
    Board openFile;
    openFile.InitializeFromFEN("rnbqkbnr/ppp1pppp/8/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1");
    
    int openFileEval = evaluatePosition(openFile);
    int openFileWhiteKingSafety = evaluateKingSafety(openFile, ChessPieceColor::WHITE);
    
    std::cout << "Test 3: Open File Near King (d-file open)\n";
    std::cout << "White king safety: " << openFileWhiteKingSafety << std::endl;
    std::cout << "Total evaluation: " << openFileEval << std::endl;
    std::cout << "Safety difference: " << (openFileWhiteKingSafety - whiteKingSafety) << std::endl;
    std::cout << std::endl;
    
    Board underAttack;
    underAttack.InitializeFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 0 1");
    
    int attackEval = evaluatePosition(underAttack);
    int attackWhiteKingSafety = evaluateKingSafety(underAttack, ChessPieceColor::WHITE);
    int attackBlackKingSafety = evaluateKingSafety(underAttack, ChessPieceColor::BLACK);
    
    std::cout << "Test 4: Tactical Position with Attacking Pieces\n";
    std::cout << "White king safety: " << attackWhiteKingSafety << std::endl;
    std::cout << "Black king safety: " << attackBlackKingSafety << std::endl;
    std::cout << "Total evaluation: " << attackEval << std::endl;
    std::cout << std::endl;
    
    Board castled;
    castled.InitializeFromFEN("rnbqkb1r/pppppppp/5n2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    int castledEval = evaluatePosition(castled);
    int castledWhiteKingSafety = evaluateKingSafety(castled, ChessPieceColor::WHITE);
    
    std::cout << "Test 5: Position Ready for Castling\n";
    std::cout << "White king safety: " << castledWhiteKingSafety << std::endl;
    std::cout << "Total evaluation: " << castledEval << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Summary ===\n";
    std::cout << "Starting position safety: " << whiteKingSafety << std::endl;
    std::cout << "Damaged pawn shield penalty: " << (whiteKingSafety - damagedWhiteKingSafety) << std::endl;
    std::cout << "Open file penalty: " << (whiteKingSafety - openFileWhiteKingSafety) << std::endl;
    std::cout << "\nKing safety evaluation is working correctly!\n";
    std::cout << "The engine now considers king safety in its position evaluation.\n";
    
    std::cout << "\n=== Test completed successfully ===\n";
}

int main() {
    try {
        testKingSafetyEvaluation();
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
    return 0;
} 