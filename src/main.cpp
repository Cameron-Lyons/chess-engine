#include "core/ChessEngine.h"
#include "ai/NeuralNetwork.h"
#include "ai/OpeningBook.h"
#include "core/BitboardMoves.h"
#include "core/MoveContent.h"
#include "evaluation/Evaluation.h"
#include "evaluation/EvaluationEnhanced.h"
#include "evaluation/PatternRecognition.h"
#include "protocol/uci.h"
#include "search/SearchImprovements.h"
#include "search/ValidMoves.h"
#include "search/search.h"
#include "utils/engine_globals.h"
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

PieceMoving MovingPiece(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
PieceMoving MovingPieceSecondary(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
bool PawnPromoted = false;

bool parseMove(std::string_view move, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    if (move.length() < 4) {
        return false;
    }
    srcCol = move[0] - 'a';
    srcRow = move[1] - '1';
    destCol = move[2] - 'a';
    destRow = move[3] - '1';
    bool valid = srcCol >= 0 && srcCol < 8 && srcRow >= 0 && srcRow < 8 && destCol >= 0 &&
                 destCol < 8 && destRow >= 0 && destRow < 8;
    return valid;
}

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    for (int row = 7; row >= 0; --row) {
        std::cout << (row + 1) << " ";
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                std::cout << ". ";
            } else {
                char pieceChar = ' ';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        pieceChar = 'P';
                        break;
                    case ChessPieceType::KNIGHT:
                        pieceChar = 'N';
                        break;
                    case ChessPieceType::BISHOP:
                        pieceChar = 'B';
                        break;
                    case ChessPieceType::ROOK:
                        pieceChar = 'R';
                        break;
                    case ChessPieceType::QUEEN:
                        pieceChar = 'Q';
                        break;
                    case ChessPieceType::KING:
                        pieceChar = 'K';
                        break;
                    default:
                        pieceChar = '?';
                        break;
                }
                if (piece.PieceColor == ChessPieceColor::BLACK) {
                    pieceChar = tolower(pieceChar);
                }
                std::cout << pieceChar << " ";
            }
        }
        std::cout << (row + 1) << "\n";
    }
    std::cout << "  a b c d e f g h\n";
    std::cout << "Turn: " << (board.turn == ChessPieceColor::WHITE ? "White" : "Black") << "\n";

    auto timeSinceLastMove = board.getTimeSinceLastMove();
    std::cout << "Time since last move: " << timeSinceLastMove.count() << "ms\n";
}

int calculateTimeForMove(Board& board, int totalTimeMs, int movesPlayed) {
    int baseTime = totalTimeMs / std::max(1, 40 - movesPlayed);

    int totalMaterial = 0;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType != ChessPieceType::NONE) {
            totalMaterial += board.squares[i].piece.PieceValue;
        }
    }

    float complexityMultiplier = 1.0f;

    if (movesPlayed < 10) {
        complexityMultiplier = 0.8f;
    } else if (totalMaterial > 3000) {
        complexityMultiplier = 3.0f;
    } else if (totalMaterial < 1500) {
        complexityMultiplier = 2.5f;
    }

    if (IsKingInCheck(board, board.turn)) {
        complexityMultiplier *= 1.5f;
    }

    return static_cast<int>(baseTime * complexityMultiplier);
}

std::pair<int, int> getComputerMove(Board& board, int timeLimitMs = 15000) {
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        std::cout << "Using opening book move: " << bookMove << "\n";
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            return {srcCol + srcRow * 8, destCol + destRow * 8};
        }
    }

    static int movesPlayed = 0;
    movesPlayed++;
    int adaptiveTime = calculateTimeForMove(board, timeLimitMs * 15, movesPlayed);
    adaptiveTime = std::min(adaptiveTime, timeLimitMs);

    std::cout << "Allocated " << adaptiveTime << "ms for this move\n";

    std::cout << "Using optimized single-threaded search...\n";

    int searchDepth = 8;
    if (adaptiveTime > 12000)
        searchDepth = 12;
    else if (adaptiveTime > 8000)
        searchDepth = 11;
    else if (adaptiveTime > 5000)
        searchDepth = 10;
    else if (adaptiveTime > 3000)
        searchDepth = 9;
    else if (adaptiveTime > 1500)
        searchDepth = 8;
    else if (adaptiveTime < 800)
        searchDepth = 6;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    int numMoves = moves.size();

    if (numMoves > 35)
        searchDepth += 3;
    else if (numMoves < 15)
        searchDepth -= 1;

    int numCaptures = 0;
    for (const auto& move : moves) {
        if (isCapture(board, move.first, move.second)) {
            numCaptures++;
        }
    }
    if (numCaptures > 5)
        searchDepth += 2;
    if (numCaptures > 8)
        searchDepth += 1;

    bool hasHangingPieces = false;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceType == ChessPieceType::PAWN)
            continue;

        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                         ? ChessPieceColor::BLACK
                                         : ChessPieceColor::WHITE;
        for (int j = 0; j < 64; j++) {
            const Piece& enemy = board.squares[j].piece;
            if (enemy.PieceColor == enemyColor && canPieceAttackSquare(board, j, i)) {
                hasHangingPieces = true;
                break;
            }
        }
        if (hasHangingPieces)
            break;
    }

    if (hasHangingPieces) {
        searchDepth += 1;
    }

    searchDepth = std::max(6, std::min(searchDepth, 12));

    std::cout << "Search depth: " << searchDepth << " (moves: " << numMoves
              << ", captures: " << numCaptures << ")\n";

    return findBestMove(board, searchDepth);
}

std::string positionToNotation(int pos) {
    if (pos < 0 || pos >= 64)
        return "??";
    int row = pos / 8;
    int col = pos % 8;
    return std::string(1, 'a' + col) + std::to_string(row + 1);
}

std::string to_string(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN:
            return "Pawn";
        case ChessPieceType::KNIGHT:
            return "Knight";
        case ChessPieceType::BISHOP:
            return "Bishop";
        case ChessPieceType::ROOK:
            return "Rook";
        case ChessPieceType::QUEEN:
            return "Queen";
        case ChessPieceType::KING:
            return "King";
        default:
            return "None";
    }
}

std::string to_string(ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? "White" : "Black";
}

enum class GameState {
    ONGOING,
    CHECKMATE_WHITE_WINS,
    CHECKMATE_BLACK_WINS,
    STALEMATE,
    DRAW_INSUFFICIENT_MATERIAL
};

GameState checkGameState(Board& board) {
    ChessPieceColor currentPlayer = board.turn;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, currentPlayer);

    std::vector<std::pair<int, int>> legalMoves;
    for (const auto& move : moves) {
        Board testBoard = board;
        if (testBoard.movePiece(move.first, move.second)) {
            if (!IsKingInCheck(testBoard, currentPlayer)) {
                legalMoves.push_back(move);
            }
        }
    }

    bool isInCheck = IsKingInCheck(board, currentPlayer);

    if (legalMoves.empty()) {
        if (isInCheck) {
            if (currentPlayer == ChessPieceColor::WHITE) {
                return GameState::CHECKMATE_BLACK_WINS;
            } else {
                return GameState::CHECKMATE_WHITE_WINS;
            }
        } else {
            return GameState::STALEMATE;
        }
    }

    std::vector<ChessPieceType> whitePieces, blackPieces;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whitePieces.push_back(piece.PieceType);
            } else {
                blackPieces.push_back(piece.PieceType);
            }
        }
    }

    bool insufficientMaterial = false;

    if (whitePieces.empty() && blackPieces.empty()) {
        insufficientMaterial = true;
    } else if ((whitePieces.size() == 1 && blackPieces.empty() &&
                (whitePieces[0] == ChessPieceType::BISHOP ||
                 whitePieces[0] == ChessPieceType::KNIGHT)) ||
               (blackPieces.size() == 1 && whitePieces.empty() &&
                (blackPieces[0] == ChessPieceType::BISHOP ||
                 blackPieces[0] == ChessPieceType::KNIGHT))) {
        insufficientMaterial = true;
    } else if (whitePieces.size() == 1 && blackPieces.size() == 1 &&
               whitePieces[0] == ChessPieceType::BISHOP &&
               blackPieces[0] == ChessPieceType::BISHOP) {
        insufficientMaterial = true;
    }

    if (insufficientMaterial) {
        return GameState::DRAW_INSUFFICIENT_MATERIAL;
    }

    return GameState::ONGOING;
}

void announceGameResult(GameState state) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "                GAME OVER                \n";
    std::cout << std::string(50, '=') << "\n";

    switch (state) {
        case GameState::CHECKMATE_WHITE_WINS:
            std::cout << "🏆 CHECKMATE! WHITE WINS! 🏆\n";
            std::cout << "Black king is in checkmate.\n";
            std::cout << "White has successfully cornered the black king!\n";
            break;

        case GameState::CHECKMATE_BLACK_WINS:
            std::cout << "🏆 CHECKMATE! BLACK WINS! 🏆\n";
            std::cout << "White king is in checkmate.\n";
            std::cout << "Black has successfully cornered the white king!\n";
            break;

        case GameState::STALEMATE:
            std::cout << "🤝 STALEMATE - DRAW! 🤝\n";
            std::cout << "The current player has no legal moves but is not in check.\n";
            std::cout << "The game ends in a draw by stalemate.\n";
            break;

        case GameState::DRAW_INSUFFICIENT_MATERIAL:
            std::cout << "🤝 DRAW - INSUFFICIENT MATERIAL! 🤝\n";
            std::cout << "Neither side has enough material to force checkmate.\n";
            std::cout << "The game ends in a draw.\n";
            break;

        case GameState::ONGOING:
            break;
    }

    if (state != GameState::ONGOING) {
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Thank you for playing!\n";
        std::cout << "Press Enter to exit...\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string mode = argv[1];

        if (mode == "uci") {
            UCIEngine engine;
            engine.run();
            return 0;
        } else if (mode == "train") {
            std::cout << "Neural Network Training Mode\n";
            std::cout << "============================\n\n";

            EnhancedEvaluator::EvaluationConfig config;
            config.useNeuralNetwork = true;
            config.nnWeight = 0.7f;
            config.useTraditionalEval = true;
            config.traditionalWeight = 0.3f;

            initializeEnhancedEvaluator(config);

            NNTrainer::TrainingConfig trainConfig;
            trainConfig.batchSize = 32;
            trainConfig.epochs = 5;
            trainConfig.validationSplit = 0.2f;
            trainConfig.earlyStoppingPatience = 3;
            trainConfig.modelPath = "models/chess_nn.bin";
            trainConfig.trainingDataPath = "data/training_data.bin";

            NNTrainer trainer(*getEnhancedEvaluator()->getNeuralNetwork(), trainConfig);

            int numGames = 100;
            if (argc > 2) {
                numGames = std::stoi(argv[2]);
            }

            std::cout << "Generating " << numGames << " self-play games for training...\n";
            trainer.trainOnSelfPlayData(numGames);

            trainer.generateTrainingReport("training_report.txt");

            std::cout << "\nTraining completed! Model saved to: " << trainConfig.modelPath
                      << std::endl;
            return 0;
        } else if (mode == "test") {
            std::cout << "Neural Network Test Mode\n";
            std::cout << "========================\n\n";

            EnhancedEvaluator::EvaluationConfig config;
            config.useNeuralNetwork = true;
            config.nnWeight = 0.7f;
            config.useTraditionalEval = true;
            config.traditionalWeight = 0.3f;

            initializeEnhancedEvaluator(config);

            std::vector<std::string> testFens = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
                "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
                "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
            };

            std::vector<std::string> positionNames = {"Starting position", "e4 e5", "e4 e5 Nf3",
                                                      "e4 e5 Nf3 Nc6"};

            for (size_t i = 0; i < testFens.size(); ++i) {
                Board board;
                board.InitializeFromFEN(testFens[i]);

                std::cout << "Position " << (i + 1) << ": " << positionNames[i] << std::endl;
                std::cout << "FEN: " << testFens[i] << std::endl;

                int traditionalEval = evaluatePosition(board);
                std::cout << "Traditional evaluation: " << traditionalEval << " centipawns"
                          << std::endl;

                float nnEval = evaluateWithNeuralNetwork(board);
                std::cout << "Neural network evaluation: " << nnEval << " centipawns" << std::endl;

                float hybridEval = evaluateHybrid(board);
                std::cout << "Hybrid evaluation: " << hybridEval << " centipawns" << std::endl;

                std::cout << std::endl;
            }

            return 0;
        } else if (mode == "generate") {
            std::cout << "Training Data Generation Mode\n";
            std::cout << "==============================\n\n";

            TrainingDataGenerator generator;

            int numGames = 50;
            if (argc > 2) {
                numGames = std::stoi(argv[2]);
            }

            std::cout << "Generating " << numGames << " self-play games...\n";
            auto trainingData = generator.generateSelfPlayData(numGames);

            std::string dataPath = "data/training_data.bin";
            if (argc > 3) {
                dataPath = argv[3];
            }

            generator.saveTrainingData(trainingData, dataPath);

            std::cout << "Training data saved to: " << dataPath << std::endl;
            std::cout << "Generated " << trainingData.size() << " training examples" << std::endl;

            return 0;
        }
    }

    ChessTimePoint startTime = ChessClock::now();

    initKnightAttacks();
    initKingAttacks();

    InitZobrist();

    initializeEnhancedEvaluator();
    initializeOpeningBook();

    std::cout << "Chess Engine v2.0 - Advanced Features Edition\n";
    std::cout << "=============================================\n";
    std::cout << "Features: Magic bitboards, Neural network evaluation, Pattern recognition\n";
    std::cout << "Use './chess_engine uci' for UCI mode\n\n";

    ChessBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::string input;
    std::string input2;
    while (true) {
        printBoard(ChessBoard);

        GameState gameState = checkGameState(ChessBoard);
        if (gameState != GameState::ONGOING) {
            announceGameResult(gameState);
            std::string dummy;
            std::getline(std::cin, dummy);
            break;
        }

        std::string checkIndicator = "";
        if (IsKingInCheck(ChessBoard, ChessBoard.turn)) {
            checkIndicator = " [CHECK!] ";
        }

        std::cout << "\nEnter move (e.g., e4, Nf3, O-O) or 'quit':" << checkIndicator << " ";
        std::getline(std::cin, input);

        if (input == "quit" || input == "exit") {
            break;
        }

        ChessTimePoint moveStartTime = ChessClock::now();

        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
            ChessPieceType promotionPiece = ChessPieceType::QUEEN;
            if (input.find('=') != std::string::npos) {
                promotionPiece = getPromotionPiece(input);
            }

            if (MovePiece(srcCol, srcRow, destCol, destRow, promotionPiece)) {
                std::cout << "✓ Move played successfully!\n";
            } else {
                std::cout << "❌ Invalid move. Try again.\n";
            }
        } else {
            std::cout
                << "❌ Could not parse move. Use algebraic notation (e.g., e4, Nf3, O-O, e8=Q).\n";
        }

        if (ChessBoard.turn == ChessPieceColor::BLACK) {
            std::cout << "\nComputer is thinking...\n";

            ChessTimePoint computerStartTime = ChessClock::now();
            auto computerMove = getComputerMove(ChessBoard, 20000);
            auto computerTime = ChessClock::now() - computerStartTime;

            if (computerMove.first != -1 && computerMove.second != -1) {
                int from = computerMove.first;
                int to = computerMove.second;
                int srcCol = from % 8;
                int srcRow = from / 8;
                int destCol = to % 8;
                int destRow = to / 8;

                ChessPieceType computerPromotionPiece = ChessPieceType::QUEEN;
                const Piece& movingPiece = ChessBoard.squares[from].piece;
                if (movingPiece.PieceType == ChessPieceType::PAWN &&
                    (destRow == 0 || destRow == 7)) {
                    computerPromotionPiece = ChessPieceType::QUEEN;
                }

                if (MovePiece(srcCol, srcRow, destCol, destRow, computerPromotionPiece)) {
                    std::cout << "Computer played: " << positionToNotation(computerMove.first)
                              << " to " << positionToNotation(computerMove.second) << " (took "
                              << std::chrono::duration_cast<ChessDuration>(computerTime).count()
                              << "ms)\n";

                    GameState postMoveState = checkGameState(ChessBoard);
                    if (postMoveState != GameState::ONGOING) {
                        printBoard(ChessBoard);
                        announceGameResult(postMoveState);
                        std::string dummy;
                        std::getline(std::cin, dummy);
                        return 0;
                    }
                } else {
                    std::cout << "Computer move failed!\n";
                }
            } else {
                std::cout << "Computer couldn't find a valid move!\n";
                GameState state = checkGameState(ChessBoard);
                if (state != GameState::ONGOING) {
                    announceGameResult(state);
                    std::string dummy;
                    std::getline(std::cin, dummy);
                    return 0;
                }
            }
        }

        auto moveTime = ChessClock::now() - moveStartTime;
        std::cout << "Move completed in "
                  << std::chrono::duration_cast<ChessDuration>(moveTime).count() << "ms\n";
    }

    auto totalTime = ChessClock::now() - startTime;
    std::cout << "\nGame completed in "
              << std::chrono::duration_cast<ChessDuration>(totalTime).count() << "ms\n";
    std::cout << "Thanks for playing!\n";

    return 0;
}