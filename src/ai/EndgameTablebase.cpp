#include "EndgameTablebase.h"
#include "SyzygyTablebase.h"
#include <cctype>
#include <iostream>
#include <unordered_map>

class EndgameTablebase::Impl {
public:
    Impl(const std::string& path) : tablebasePath(path) {
        if (Syzygy::init(path)) {
            std::cout << "Syzygy tablebases initialized with " << Syzygy::maxPieces()
                      << "-piece support" << '\n';
        }
    }
    ~Impl() = default;
    Impl(const Impl&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;
    Impl(Impl&&) = delete;
    auto operator=(Impl&&) -> Impl& = delete;

    std::string tablebasePath;
    TablebaseType type{TablebaseType::SYZYGY};
    std::unordered_map<std::string, TablebaseResult> cache;
};

EndgameTablebase::EndgameTablebase(const std::string& tablebasePath)
    : m_pImpl(std::make_unique<Impl>(tablebasePath)) {}

EndgameTablebase::~EndgameTablebase() = default;

auto EndgameTablebase::isInTablebase(const Board& board) -> bool {
    if (m_pImpl->type == TablebaseType::SYZYGY) {
        return Syzygy::canProbe(board);
    }
    return false;
}

auto EndgameTablebase::probe(const Board& board, TablebaseResult& result) -> bool {
    if (m_pImpl->type != TablebaseType::SYZYGY || !Syzygy::canProbe(board)) {
        return false;
    }

    std::string key = boardToTablebaseKey(board);
    auto it = m_pImpl->cache.find(key);
    if (it != m_pImpl->cache.end()) {
        result = it->second;
        return true;
    }

    int success = 0;
    Syzygy::ProbeResult wdl = Syzygy::probeWDL(board, &success);
    if (!success)
        return false;

    result.isExact = true;
    result.bestMoves.clear();

    switch (wdl) {
        case Syzygy::PROBE_WIN:
        case Syzygy::PROBE_CURSED_WIN:
            result.distanceToMate = 100;
            break;
        case Syzygy::PROBE_LOSS:
        case Syzygy::PROBE_BLESSED_LOSS:
            result.distanceToMate = -100;
            break;
        default:
            result.distanceToMate = 0;
            break;
    }

    int dtz = Syzygy::probeDTZ(board, &success);
    if (success && dtz != 0) {
        result.distanceToMate = dtz;
    }

    m_pImpl->cache[key] = result;
    return true;
}

auto EndgameTablebase::getBestMove(const Board& board, std::pair<int, int>& bestMove) -> bool {
    if (m_pImpl->type != TablebaseType::SYZYGY) {
        return false;
    }

    Syzygy::DTZResult dtzResult{};
    if (Syzygy::probeRoot(board, dtzResult)) {
        bestMove.first = dtzResult.from;
        bestMove.second = dtzResult.to;
        return true;
    }

    return false;
}

auto EndgameTablebase::isWinning(const Board& board) -> bool {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate > 0;
    }
    return false;
}

auto EndgameTablebase::isLosing(const Board& board) -> bool {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate < 0;
    }
    return false;
}

auto EndgameTablebase::isDraw(const Board& board) -> bool {
    TablebaseResult result;
    if (probe(board, result)) {
        return result.distanceToMate == 0;
    }
    return false;
}

void EndgameTablebase::loadTablebase(const std::string& pieceCombination) {
    (void)pieceCombination;
}

auto EndgameTablebase::getAvailableTablebases() -> std::vector<std::string> {

    return {};
}

void EndgameTablebase::setTablebaseType(TablebaseType type) {
    m_pImpl->type = type;
    if (type == TablebaseType::SYZYGY) {
        Syzygy::init(m_pImpl->tablebasePath);
    }
}

auto EndgameTablebase::boardToTablebaseKey(const Board& board) -> std::string {

    std::string key;
    key.reserve(64);

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            key += '.';
        } else {
            char c = 'K';
            switch (piece.PieceType) {
                case ChessPieceType::PAWN:
                    c = 'P';
                    break;
                case ChessPieceType::KNIGHT:
                    c = 'N';
                    break;
                case ChessPieceType::BISHOP:
                    c = 'B';
                    break;
                case ChessPieceType::ROOK:
                    c = 'R';
                    break;
                case ChessPieceType::QUEEN:
                    c = 'Q';
                    break;
                default:
                    break;
            }
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
            key += c;
        }
    }

    key += (board.turn == ChessPieceColor::WHITE) ? 'W' : 'B';
    return key;
}

auto EndgameTablebase::isValidEndgamePosition(const Board& board) -> bool {
    int pieces = countPieces(board);

    return pieces <= 7 && !board.whiteCanCastle && !board.blackCanCastle;
}

auto EndgameTablebase::countPieces(const Board& board) -> int {

    int count = 0;
    for (int square = 0; square < 64; ++square) {
        if (board.squares[square].piece.PieceType != ChessPieceType::NONE) {
            count++;
        }
    }
    return count;
}

auto EndgameKnowledge::evaluateKPK(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::evaluateKRK(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::evaluateKQK(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::evaluateKBNK(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::evaluateKBBK(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::evaluateEndgame(const Board& board) -> int {
    (void)board;

    return 0;
}

auto EndgameKnowledge::getKingDistance(int king1, int king2) -> int {
    int row1 = king1 / 8, col1 = king1 % 8;
    int row2 = king2 / 8, col2 = king2 % 8;
    return std::max(std::abs(row1 - row2), std::abs(col1 - col2));
}

auto EndgameKnowledge::isPawnPromoting(const Board& board, int pawnSquare) -> bool {
    (void)board;
    (void)pawnSquare;

    return false;
}

auto EndgameKnowledge::getPawnAdvancement(const Board& board, int pawnSquare) -> int {
    (void)board;
    (void)pawnSquare;

    return 0;
}