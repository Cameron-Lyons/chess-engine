#include "ChessEngine.h"
#include "ValidMoves.h"
#include "MoveContent.h"
#include "search.h"
#include <iostream>

// Global variable definitions
Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

// Global variables for attack boards
bool BlackAttackBoard[64];
bool WhiteAttackBoard[64];
int BlackKingPosition;
int WhiteKingPosition;

// Global variables for move tracking
PieceMoving MovingPiece(WHITE, PAWN, false);
PieceMoving MovingPieceSecondary(WHITE, PAWN, false);
bool PawnPromoted = false;

int main() {
    std::cout << "Testing chess engine initialization...\n";
    
    try {
        std::cout << "Step 1: Creating board...\n";
        Board testBoard;
        std::cout << "Board created successfully.\n";
        
        std::cout << "Step 2: Initializing from FEN...\n";
        testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::cout << "FEN initialization completed.\n";
        
        std::cout << "Step 3: Testing Engine() function...\n";
        Engine();
        std::cout << "Engine() completed successfully.\n";
        
        std::cout << "Step 4: Testing GenValidMoves...\n";
        GenValidMoves(ChessBoard);
        std::cout << "GenValidMoves completed successfully.\n";
        
        std::cout << "All tests passed!\n";
        
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception caught!" << std::endl;
        return 1;
    }
    
    return 0;
} 