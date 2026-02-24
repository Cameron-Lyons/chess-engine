#include "core/ChessEngine.h"
#include "core/MoveContent.h"
#include "evaluation/EvaluationTuning.h"
#include "evaluation/HybridEvaluator.h"
#include "evaluation/PositionAnalysis.h"
#include "protocol/uci.h"
#include "utils/engine_globals.h"

using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

namespace {
constexpr int kBoardTopRow = BOARD_SIZE - 1;
constexpr int kNominalMovesToGo = 40;
constexpr int kOpeningMoveThreshold = 10;
constexpr int kHighMaterialThreshold = 3000;
constexpr int kLowMaterialThreshold = 1500;
constexpr float kOpeningComplexityMultiplier = 0.8F;
constexpr float kHighMaterialComplexityMultiplier = 3.0F;
constexpr float kLowMaterialComplexityMultiplier = 2.5F;
constexpr float kInCheckComplexityMultiplier = 1.5F;
constexpr int kAdaptiveTimeScale = 15;
constexpr int kDefaultComputerMoveTimeMs = 6000;
constexpr int kBaseSearchDepth = 8;
constexpr int kMinSearchDepth = 6;
constexpr int kMaxSearchDepth = 12;
constexpr int kDepth12TimeThresholdMs = 12000;
constexpr int kDepth11TimeThresholdMs = 8000;
constexpr int kDepth10TimeThresholdMs = 5000;
constexpr int kDepth9TimeThresholdMs = 3000;
constexpr int kDepth8TimeThresholdMs = 1500;
constexpr int kDepth6TimeThresholdMs = 800;
constexpr int kCrowdedPositionMoveThreshold = 35;
constexpr int kSparsePositionMoveThreshold = 15;
constexpr int kCrowdedDepthBonus = 3;
constexpr int kSparseDepthPenalty = 1;
constexpr int kCaptureRichThreshold = 5;
constexpr int kVeryCaptureRichThreshold = 8;
constexpr int kCaptureRichDepthBonus = 2;
constexpr int kVeryCaptureRichDepthBonus = 1;
constexpr int kHangingPieceDepthBonus = 1;
constexpr int kSearchThreads = 1;
constexpr int kSearchContempt = 0;
constexpr int kSearchMultiPv = 1;
constexpr int kInvalidSquare = -1;
constexpr int kTrainingBatchSize = 32;
constexpr int kTrainingEpochs = 5;
constexpr float kTrainingValidationSplit = 0.2F;
constexpr int kEarlyStoppingPatience = 3;
constexpr int kDefaultTrainingGames = 100;
constexpr int kDefaultDataGenerationGames = 50;
constexpr int kDefaultTuneIterations = 100;
constexpr const char* kStartingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
} // namespace

Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

PieceMoving MovingPiece(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
PieceMoving MovingPieceSecondary(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
bool PawnPromoted = false;

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    for (int row = kBoardTopRow; row >= 0; --row) {
        std::cout << (row + 1) << " ";
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int pos = (row * BOARD_SIZE) + col;
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
                    pieceChar =
                        static_cast<char>(std::tolower(static_cast<unsigned char>(pieceChar)));
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
    int baseTime = totalTimeMs / std::max(1, kNominalMovesToGo - movesPlayed);
    int totalMaterial = 0;
    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType != ChessPieceType::NONE) {
            totalMaterial += board.squares[i].piece.PieceValue;
        }
    }

    float complexityMultiplier = 1.0F;

    if (movesPlayed < kOpeningMoveThreshold) {
        complexityMultiplier = kOpeningComplexityMultiplier;
    } else if (totalMaterial > kHighMaterialThreshold) {
        complexityMultiplier = kHighMaterialComplexityMultiplier;
    } else if (totalMaterial < kLowMaterialThreshold) {
        complexityMultiplier = kLowMaterialComplexityMultiplier;
    }

    if (IsKingInCheck(board, board.turn)) {
        complexityMultiplier *= kInCheckComplexityMultiplier;
    }

    return static_cast<int>(static_cast<float>(baseTime) * complexityMultiplier);
}

std::pair<int, int> getComputerMove(Board& board, int timeLimitMs = kDefaultComputerMoveTimeMs) {
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        std::cout << "Using opening book move: " << bookMove << "\n";
        int srcCol = 0;
        int srcRow = 0;
        int destCol = 0;
        int destRow = 0;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            return {(srcCol + (srcRow * BOARD_SIZE)), (destCol + (destRow * BOARD_SIZE))};
        }
    }

    static int movesPlayed = 0;
    movesPlayed++;
    int adaptiveTime = calculateTimeForMove(board, timeLimitMs * kAdaptiveTimeScale, movesPlayed);
    adaptiveTime = std::min(adaptiveTime, timeLimitMs);
    std::cout << "Allocated " << adaptiveTime << "ms for this move\n";
    std::cout << "Using optimized single-threaded search...\n";
    int searchDepth = kBaseSearchDepth;
    if (adaptiveTime > kDepth12TimeThresholdMs) {
        searchDepth = 12;
    } else if (adaptiveTime > kDepth11TimeThresholdMs) {
        searchDepth = 11;
    } else if (adaptiveTime > kDepth10TimeThresholdMs) {
        searchDepth = 10;
    } else if (adaptiveTime > kDepth9TimeThresholdMs) {
        searchDepth = 9;
    } else if (adaptiveTime > kDepth8TimeThresholdMs) {
        searchDepth = kBaseSearchDepth;
    } else if (adaptiveTime < kDepth6TimeThresholdMs) {
        searchDepth = kMinSearchDepth;
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    int numMoves = static_cast<int>(moves.size());

    if (numMoves > kCrowdedPositionMoveThreshold) {
        searchDepth += kCrowdedDepthBonus;
    } else if (numMoves < kSparsePositionMoveThreshold) {
        searchDepth -= kSparseDepthPenalty;
    }

    int numCaptures = 0;
    for (const auto& move : moves) {
        if (isCapture(board, move.first, move.second)) {
            numCaptures++;
        }
    }
    if (numCaptures > kCaptureRichThreshold) {
        searchDepth += kCaptureRichDepthBonus;
    }
    if (numCaptures > kVeryCaptureRichThreshold) {
        searchDepth += kVeryCaptureRichDepthBonus;
    }

    bool hasHangingPieces = false;
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceType == ChessPieceType::PAWN) {
            continue;
        }

        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                         ? ChessPieceColor::BLACK
                                         : ChessPieceColor::WHITE;
        for (int j = 0; j < NUM_SQUARES; j++) {
            const Piece& enemy = board.squares[j].piece;
            if (enemy.PieceColor == enemyColor && canPieceAttackSquare(board, j, i)) {
                hasHangingPieces = true;
                break;
            }
        }
        if (hasHangingPieces) {
            break;
        }
    }

    if (hasHangingPieces) {
        searchDepth += kHangingPieceDepthBonus;
    }

    searchDepth = std::max(kMinSearchDepth, std::min(searchDepth, kMaxSearchDepth));

    std::cout << "Search depth: " << searchDepth << " (moves: " << numMoves
              << ", captures: " << numCaptures << ")\n";

    SearchResult result =
        iterativeDeepeningParallel(board, searchDepth, adaptiveTime, kSearchThreads,
                                   kSearchContempt, kSearchMultiPv, adaptiveTime, adaptiveTime);

    if (result.bestMove.first != kInvalidSquare && result.bestMove.second != kInvalidSquare) {
        return result.bestMove;
    }

    if (!moves.empty()) {
        return moves.front();
    }

    return {kInvalidSquare, kInvalidSquare};
}

std::string positionToNotation(int pos) {
    if (pos < 0 || pos >= NUM_SQUARES) {
        return "??";
    }
    int row = pos / BOARD_SIZE;
    int col = pos % BOARD_SIZE;
    return std::string(1, static_cast<char>('a' + col)) + std::to_string(row + 1);
}

enum class GameState : std::uint8_t {
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

    std::vector<ChessPieceType> whitePieces;
    std::vector<ChessPieceType> blackPieces;
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whitePieces.push_back(piece.PieceType);
            } else {
                blackPieces.push_back(piece.PieceType);
            }
        }
    }

    const bool bareKings = whitePieces.empty() && blackPieces.empty();
    const bool loneMinorVsKing =
        (whitePieces.size() == 1 && blackPieces.empty() &&
         (whitePieces[0] == ChessPieceType::BISHOP || whitePieces[0] == ChessPieceType::KNIGHT)) ||
        (blackPieces.size() == 1 && whitePieces.empty() &&
         (blackPieces[0] == ChessPieceType::BISHOP || blackPieces[0] == ChessPieceType::KNIGHT));
    const bool bishopsOnly = whitePieces.size() == 1 && blackPieces.size() == 1 &&
                             whitePieces[0] == ChessPieceType::BISHOP &&
                             blackPieces[0] == ChessPieceType::BISHOP;

    if (bareKings || loneMinorVsKing || bishopsOnly) {
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
    try {
        if (argc > 1) {
            std::string mode = argv[1];

            if (mode == "uci") {
                UCIEngine engine;
                engine.run();
                return 0;
            } else if (mode == "train") {
                std::cout << "Neural Network Training Mode\n";
                std::cout << "============================\n\n";
                HybridEvaluator::EvaluationConfig config;
                config.useNeuralNetwork = true;
                config.nnWeight = 0.7F;
                config.useTraditionalEval = true;
                config.traditionalWeight = 0.3F;
                initializeHybridEvaluator(config);
                NNTrainer::TrainingConfig trainConfig;
                trainConfig.batchSize = kTrainingBatchSize;
                trainConfig.epochs = kTrainingEpochs;
                trainConfig.validationSplit = kTrainingValidationSplit;
                trainConfig.earlyStoppingPatience = kEarlyStoppingPatience;
                trainConfig.modelPath = "models/chess_nn.bin";
                trainConfig.trainingDataPath = "data/training_data.bin";
                NNTrainer trainer(*getHybridEvaluator()->getNeuralNetwork(), trainConfig);
                int numGames = kDefaultTrainingGames;
                if (argc > 2) {
                    numGames = std::stoi(argv[2]);
                }

                std::cout << "Generating " << numGames << " self-play games for training...\n";
                trainer.trainOnSelfPlayData(numGames);
                trainer.generateTrainingReport("training_report.txt");

                std::cout << "\nTraining completed! Model saved to: " << trainConfig.modelPath
                          << '\n';
                return 0;
            } else if (mode == "test") {
                std::cout << "Neural Network Test Mode\n";
                std::cout << "========================\n\n";
                HybridEvaluator::EvaluationConfig config;
                config.useNeuralNetwork = true;
                config.nnWeight = 0.7F;
                config.useTraditionalEval = true;
                config.traditionalWeight = 0.3F;
                initializeHybridEvaluator(config);

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
                    std::cout << "Position " << (i + 1) << ": " << positionNames[i] << '\n';
                    std::cout << "FEN: " << testFens[i] << '\n';
                    int traditionalEval = evaluatePosition(board);
                    std::cout << "Traditional evaluation: " << traditionalEval << " centipawns"
                              << '\n';

                    float nnEval = evaluateWithNeuralNetwork(board);
                    std::cout << "Neural network evaluation: " << nnEval << " centipawns" << '\n';
                    float hybridEval = evaluateHybrid(board);
                    std::cout << "Hybrid evaluation: " << hybridEval << " centipawns" << '\n';
                    std::cout << '\n';
                }

                return 0;
            } else if (mode == "generate") {
                std::cout << "Training Data Generation Mode\n";
                std::cout << "==============================\n\n";
                TrainingDataGenerator generator;
                int numGames = kDefaultDataGenerationGames;
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
                std::cout << "Training data saved to: " << dataPath << '\n';
                std::cout << "Generated " << trainingData.size() << " training examples" << '\n';
                return 0;
            } else if (mode == "--tune" || mode == "tune") {
                if (argc < 3) {
                    std::cout << "Usage: chess_engine --tune <positions_file> [iterations]\n";
                    return 1;
                }
                std::string posFile = argv[2];
                int iterations = kDefaultTuneIterations;
                if (argc > 3) {
                    iterations = std::stoi(argv[3]);
                }

                InitZobrist();
                TexelTuner tuner;
                if (!tuner.loadPositions(posFile)) {
                    std::cout << "Failed to load positions from: " << posFile << "\n";
                    return 1;
                }
                std::cout << "Loaded positions, starting Texel tuning...\n";
                tuner.optimize(iterations);
                tuner.exportParams("tuned_params.txt");
                std::cout << "Tuned parameters exported to tuned_params.txt\n";
                return 0;
            } else if (mode == "analyze") {
                std::cout << "Position Analysis Mode\n";
                std::cout << "======================\n\n";
                std::string fen = kStartingFen;
                if (argc > 2) {
                    fen = argv[2];
                }

                Board board;
                board.InitializeFromFEN(fen);
                std::cout << "Analyzing position from FEN: " << fen << "\n\n";
                printBoard(board);
                std::cout << "\n";
                PositionAnalysis analysis = PositionAnalyzer::analyzePosition(board);
                PositionAnalyzer::printDetailedAnalysis(analysis);
                return 0;
            }
        }

        ChessTimePoint startTime = ChessClock::now();
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
        initializeHybridEvaluator();
        std::cout << "Chess Engine v2.0 - Advanced Features Edition\n";
        std::cout << "=============================================\n";
        std::cout << "Features: Magic bitboards, Neural network evaluation, Pattern recognition\n";
        std::cout << "Use './chess_engine uci' for UCI mode\n\n";
        ChessBoard.InitializeFromFEN(kStartingFen);
        std::string input;
        while (true) {
            printBoard(ChessBoard);
            GameState gameState = checkGameState(ChessBoard);
            if (gameState != GameState::ONGOING) {
                announceGameResult(gameState);
                std::string dummy;
                std::getline(std::cin, dummy);
                (void)dummy;
                break;
            }

            std::string checkIndicator;
            if (IsKingInCheck(ChessBoard, ChessBoard.turn)) {
                checkIndicator = " [CHECK!] ";
            }

            std::cout << "\nEnter move (e.g., e4, Nf3, O-O) or 'quit':" << checkIndicator << " ";
            std::getline(std::cin, input);

            if (input == "quit" || input == "exit") {
                break;
            }

            ChessTimePoint moveStartTime = ChessClock::now();
            int srcCol = 0;
            int srcRow = 0;
            int destCol = 0;
            int destRow = 0;
            if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
                ChessPieceType promotionPiece = ChessPieceType::QUEEN;
                if (input.contains('=')) {
                    promotionPiece = getPromotionPiece(input);
                }

                if (MovePiece(srcCol, srcRow, destCol, destRow, promotionPiece)) {
                    std::cout << "✓ Move played successfully!\n";
                } else {
                    std::cout << "❌ Invalid move. Try again.\n";
                }
            } else {
                std::cout << "❌ Could not parse move. Use algebraic notation (e.g., e4, Nf3, O-O, "
                             "e8=Q).\n";
            }

            if (ChessBoard.turn == ChessPieceColor::BLACK) {
                std::cout << "\nComputer is thinking...\n";
                ChessTimePoint computerStartTime = ChessClock::now();
                auto computerMove = getComputerMove(ChessBoard, kDefaultComputerMoveTimeMs);
                auto computerTime = ChessClock::now() - computerStartTime;

                if (computerMove.first != kInvalidSquare && computerMove.second != kInvalidSquare) {
                    int from = computerMove.first;
                    int to = computerMove.second;
                    int srcCol = from % BOARD_SIZE;
                    int srcRow = from / BOARD_SIZE;
                    int destCol = to % BOARD_SIZE;
                    int destRow = to / BOARD_SIZE;
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
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
