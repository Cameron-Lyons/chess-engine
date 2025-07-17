#include "EndgameTablebase.h"
#include "SyzygyTablebase.h"
#include <iostream>
#include <cctype>

// Implementation of EndgameTablebase using Syzygy
class EndgameTablebase::Impl {
public:
    Impl(const std::string& path) : tablebasePath(path), type(TablebaseType::SYZYGY) {
        // Initialize Syzygy tablebases
        if (Syzygy::init(path)) {
            std::cout << "Syzygy tablebases initialized with " 
                      << Syzygy::maxPieces() << "-piece support" << std::endl;
        }
    }
    ~Impl() = default;
    
    std::string tablebasePath;
    TablebaseType type;
    std::unordered_map<std::string, TablebaseResult> cache;
};

EndgameTablebase::EndgameTablebase(const std::string& tablebasePath) 
    : pImpl(std::make_unique<Impl>(tablebasePath)) {
}

EndgameTablebase::~EndgameTablebase() = default;

bool EndgameTablebase::isInTablebase(const Board& board) {
    if (pImpl->type == TablebaseType::SYZYGY) {
        return Syzygy::canProbe(board);
    }
    return false;
}

bool EndgameTablebase::probe(const Board& board, TablebaseResult& result) {
    if (pImpl->type != TablebaseType::SYZYGY || !Syzygy::canProbe(board)) {
        return false;
    }
    
    // Check cache first
    std::string key = boardToTablebaseKey(board);
    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        result = it->second;
        return true;
    }
    
    // Probe WDL
    int success;
    Syzygy::ProbeResult wdl = Syzygy::probeWDL(board, &success);
    if (!success) return false;
    
    // Convert to our format
    result.isExact = true;
    result.bestMoves.clear();
    
    switch (wdl) {
        case Syzygy::PROBE_WIN:
        case Syzygy::PROBE_CURSED_WIN:
            result.distanceToMate = 100; // Winning
            break;
        case Syzygy::PROBE_LOSS:
        case Syzygy::PROBE_BLESSED_LOSS:
            result.distanceToMate = -100; // Losing
            break;
        default:
            result.distanceToMate = 0; // Draw
            break;
    }
    
    // Try to get DTZ for more accurate distance
    int dtz = Syzygy::probeDTZ(board, &success);
    if (success && dtz != 0) {
        result.distanceToMate = dtz;
    }
    
    // Cache result
    pImpl->cache[key] = result;
    return true;
}

bool EndgameTablebase::getBestMove(const Board& board, std::pair<int, int>& bestMove) {
    if (pImpl->type != TablebaseType::SYZYGY) {
        return false;
    }
    
    Syzygy::DTZResult dtzResult;
    if (Syzygy::probeRoot(board, dtzResult)) {
        bestMove.first = dtzResult.from;
        bestMove.second = dtzResult.to;
        return true;
    }
    
    return false;
}

bool EndgameTablebase::isWinning(const Board& board) {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate > 0;
    }
    return false;
}

bool EndgameTablebase::isLosing(const Board& board) {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate < 0;
    }
    return false;
}

bool EndgameTablebase::isDraw(const Board& board) {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate == 0;
    }
    return false;
}

void EndgameTablebase::loadTablebase(const std::string& pieceCombination) {
    (void)pieceCombination; // Suppress unused parameter warning
    // Stub implementation
}

std::vector<std::string> EndgameTablebase::getAvailableTablebases() {
    // Stub implementation - return empty vector
    return {};
}

void EndgameTablebase::setTablebaseType(TablebaseType type) {
    pImpl->type = type;
    if (type == TablebaseType::SYZYGY) {
        Syzygy::init(pImpl->tablebasePath);
    }
}

std::string EndgameTablebase::boardToTablebaseKey(const Board& board) {
    // Create a unique key for the position
    std::string key;
    key.reserve(64);
    
    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            key += '.';
        } else {
            char c = 'K';
            switch (piece.PieceType) {
                case ChessPieceType::PAWN:   c = 'P'; break;
                case ChessPieceType::KNIGHT: c = 'N'; break;
                case ChessPieceType::BISHOP: c = 'B'; break;
                case ChessPieceType::ROOK:   c = 'R'; break;
                case ChessPieceType::QUEEN:  c = 'Q'; break;
                default: break;
            }
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                c = tolower(c);
            }
            key += c;
        }
    }
    
    key += (board.turn == ChessPieceColor::WHITE) ? 'W' : 'B';
    return key;
}

bool EndgameTablebase::isValidEndgamePosition(const Board& board) {
    int pieces = countPieces(board);
    // Valid for tablebase if <= 7 pieces and no castling rights
    return pieces <= 7 && !board.whiteCanCastle && !board.blackCanCastle;
}

int EndgameTablebase::countPieces(const Board& board) {
    // Stub implementation - count all pieces
    int count = 0;
    for (int square = 0; square < 64; ++square) {
        if (board.squares[square].piece.PieceType != ChessPieceType::NONE) {
            count++;
        }
    }
    return count;
}

// Endgame knowledge implementation
int EndgameKnowledge::evaluateKPK(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::evaluateKRK(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::evaluateKQK(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::evaluateKBNK(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::evaluateKBBK(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::evaluateEndgame(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation
    return 0;
}

int EndgameKnowledge::getKingDistance(int king1, int king2) {
    int row1 = king1 / 8, col1 = king1 % 8;
    int row2 = king2 / 8, col2 = king2 % 8;
    return std::max(std::abs(row1 - row2), std::abs(col1 - col2));
}

bool EndgameKnowledge::isPawnPromoting(const Board& board, int pawnSquare) {
    (void)board;      // Suppress unused parameter warning
    (void)pawnSquare; // Suppress unused parameter warning
    // Stub implementation
    return false;
}

int EndgameKnowledge::getPawnAdvancement(const Board& board, int pawnSquare) {
    (void)board;      // Suppress unused parameter warning
    (void)pawnSquare; // Suppress unused parameter warning
    // Stub implementation
    return 0;
} 