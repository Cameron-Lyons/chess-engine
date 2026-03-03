#include "engine_globals.h"
#include "Bitboard.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "search/search.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr int kBoardSquares = BOARD_SIZE * BOARD_SIZE;
constexpr int kZobristPieceStates = 12;
constexpr int kBottomRank = 0;
constexpr int kTopRank = BOARD_SIZE - 1;
constexpr int kLeftFile = 0;
constexpr int kRightFile = BOARD_SIZE - 1;
constexpr int kKingStartFile = 4;
constexpr int kKingsideCastleFile = 6;
constexpr int kQueensideCastleFile = 2;
constexpr int kPieceNotationPrefixLength = 1;
constexpr int kDestinationLength = 2;
constexpr int kPawnCaptureNotationLength = 4;
constexpr int kPromotionCaptureNotationLength = 6;
constexpr int kWhitePawnStartRank = 1;
constexpr int kWhitePawnIntermediateRank = 2;
constexpr int kWhiteDoublePushDestinationRank = 3;
constexpr int kBlackPawnDoublePushDestinationRank = 4;
constexpr int kBlackPawnIntermediateRank = 5;
constexpr int kBlackPawnStartRank = 6;
constexpr int kWhitePromotionRank = kTopRank;
constexpr int kBlackPromotionRank = kBottomRank;
constexpr int kWhiteCaptureLeftOffset = 7;
constexpr int kWhiteCaptureRightOffset = 9;
constexpr int kBlackCaptureLeftOffset = -9;
constexpr int kBlackCaptureRightOffset = -7;
constexpr int kKnightLongStep = 2;
constexpr int kKnightShortStep = 1;
constexpr int kKingReach = 1;
constexpr int kFirstCandidateIndex = 0;
constexpr char kFileCharOffset = 'a';
constexpr char kRankCharOffset = '1';
constexpr char kCaptureMarker = 'x';
constexpr char kPromotionMarker = '=';
constexpr char kCheckMarker = '+';
constexpr char kMateMarker = '#';
constexpr char kRankSeparator = '/';
constexpr char kEmptySquareSymbol = '.';
constexpr const char* kWhiteTurnFenToken = " w";
constexpr const char* kBlackTurnFenToken = " b";
constexpr const char* kKingsideCastleNotation = "O-O";
constexpr const char* kKingsideCastleNotationAlt = "0-0";
constexpr const char* kQueensideCastleNotation = "O-O-O";
constexpr const char* kQueensideCastleNotationAlt = "0-0-0";

int toBoardIndex(const int row, const int col) {
    return (row * BOARD_SIZE) + col;
}

int parseFile(const char fileChar) {
    return fileChar - kFileCharOffset;
}

int parseRank(const char rankChar) {
    return rankChar - kRankCharOffset;
}

bool isWithinBoard(const int col, const int row) {
    return col >= kLeftFile && col <= kRightFile && row >= kBottomRank && row <= kTopRank;
}
} // namespace

std::unordered_map<std::string, std::vector<std::string>> OpeningBookOptions = {

    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", {"e5", "c5", "e6", "c6", "Nf6"}},

    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"Nf3"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", {"Nc6"}},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", {"Bc4"}},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", {"Bc5"}},
    {"r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", {"O-O"}},

    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", {"Bb5"}},
    {"r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", {"a6"}},
    {"r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4", {"Ba4"}},
    {"r1bqkbnr/1ppp1ppp/p1n5/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 1 4", {"Nf6"}},
    {"r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 5", {"O-O"}},
    {"r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 3 5", {"Be7"}},

    {"rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1", {"d5"}},
    {"rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2", {"c4"}},
    {"rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"e6"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Nf6"}},
    {"rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"Bg5"}},

    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"Nf3"}},
    {"rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", {"d6"}},
    {"rnbqkbnr/pp2pppp/3p4/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3", {"d4"}},
    {"rnbqkbnr/pp2pppp/3p4/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3", {"cxd4"}},
    {"rnbqkbnr/pp2pppp/3p4/8/3pP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 4", {"Nxd4"}},
    {"rnbqkbnr/pp2pppp/3p4/8/3NP3/8/PPP2PPP/RNBQKB1R b KQkq - 0 4", {"Nf6"}},

    {"rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"d4"}},
    {"rnbqkbnr/pppp1ppp/4p3/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq - 0 2", {"d5"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", {"Bb4"}},

    {"rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", {"d4"}},
    {"rnbqkbnr/pp1ppppp/2p5/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq - 0 2", {"d5"}},
    {"rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkbnr/pp2pppp/2p5/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", {"dxe4"}},

    {"rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1", {"Nf6"}},
    {"rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2", {"c4"}},
    {"rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"g6"}},
    {"rnbqkb1r/pppppp1p/5np1/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkb1r/pppppp1p/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Bg7"}},
    {"rnbqk2r/ppppppbp/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"e4"}},

    {"rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq - 0 1", {"e5"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 0 2", {"Nc3"}},
    {"rnbqkbnr/pppp1ppp/8/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR b KQkq - 1 2", {"Nf6"}},
    {"rnbqkb1r/pppp1ppp/5n2/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR w KQkq - 2 3", {"g3"}},
    {"rnbqkb1r/pppp1ppp/5n2/4p3/2P5/2N3P1/PP1PPP1P/R1BQKBNR b KQkq - 0 3", {"d5"}},

    {"rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", {"e5"}},
    {"rnbqkb1r/pppppppp/5n2/4P3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 2 2", {"Nd5"}},
    {"rnbqkb1r/pppppppp/8/3nP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 3 3", {"d4"}},
    {"rnbqkb1r/pppppppp/8/3nP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3", {"d6"}},

    {"rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2", {"c4"}},
    {"rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2", {"e6"}},
    {"rnbqkb1r/pppp1ppp/4pn2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", {"Nc3"}},
    {"rnbqkb1r/pppp1ppp/4pn2/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", {"Bb4"}},
    {"rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", {"e3"}}};

std::unordered_map<std::string, std::string> OpeningBook;

uint64_t ZobristTable[kBoardSquares][kZobristPieceStates];
uint64_t ZobristBlackToMove;
uint64_t ZobristCastling[4];
uint64_t ZobristEnPassant[kBoardSquares];
TranspositionTableAdapter TransTable;

std::string getFEN(const Board& board) {
    std::string fen;
    for (int row = kTopRank; row >= kBottomRank; --row) {
        int emptyCount = kBottomRank;
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int pos = toBoardIndex(row, col);
            const Piece& piece = board.squares[pos].piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > kBottomRank) {
                    fen += std::to_string(emptyCount);
                    emptyCount = kBottomRank;
                }
                char symbol = kEmptySquareSymbol;
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p';
                        break;
                    case ChessPieceType::KNIGHT:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n';
                        break;
                    case ChessPieceType::BISHOP:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b';
                        break;
                    case ChessPieceType::ROOK:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r';
                        break;
                    case ChessPieceType::QUEEN:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q';
                        break;
                    case ChessPieceType::KING:
                        symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k';
                        break;
                    default:
                        symbol = kEmptySquareSymbol;
                        break;
                }
                fen += symbol;
            }
        }
        if (emptyCount > kBottomRank) {
            fen += std::to_string(emptyCount);
        }
        if (row > kBottomRank) {
            fen += kRankSeparator;
        }
    }
    fen += board.turn == ChessPieceColor::WHITE ? kWhiteTurnFenToken : kBlackTurnFenToken;
    std::string castling;
    if (board.whiteCanCastle) {
        castling += "KQ";
    }
    if (board.blackCanCastle) {
        castling += "kq";
    }
    fen += " " + (castling.empty() ? "-" : castling);

    if (board.enPassantSquare >= 0 && board.enPassantSquare < kBoardSquares) {
        int epFile = board.enPassantSquare % BOARD_SIZE;
        int epRank = board.enPassantSquare / BOARD_SIZE;
        fen += " ";
        fen += static_cast<char>(kFileCharOffset + epFile);
        fen += static_cast<char>(kRankCharOffset + epRank);
    } else {
        fen += " -";
    }

    fen += " 0 1";
    return fen;
}

static bool parseAlgebraicMoveLegacy(std::string_view move, const Board& board, int& srcCol,
                                     int& srcRow, int& destCol, int& destRow) {
    std::string cleanMove(move);
    if (!cleanMove.empty() &&
        (cleanMove.back() == kCheckMarker || cleanMove.back() == kMateMarker)) {
        cleanMove.pop_back();
    }

    if (cleanMove == kKingsideCastleNotation || cleanMove == kKingsideCastleNotationAlt) {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = kKingStartFile;
            srcRow = kBottomRank;
            destCol = kKingsideCastleFile;
            destRow = kBottomRank;
        } else {
            srcCol = kKingStartFile;
            srcRow = kTopRank;
            destCol = kKingsideCastleFile;
            destRow = kTopRank;
        }
        return true;
    }
    if (cleanMove == kQueensideCastleNotation || cleanMove == kQueensideCastleNotationAlt) {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = kKingStartFile;
            srcRow = kBottomRank;
            destCol = kQueensideCastleFile;
            destRow = kBottomRank;
        } else {
            srcCol = kKingStartFile;
            srcRow = kTopRank;
            destCol = kQueensideCastleFile;
            destRow = kTopRank;
        }
        return true;
    }

    ChessPieceType pieceType = ChessPieceType::PAWN;
    size_t startPos = kBottomRank;
    bool isCapture = false;
    if (!cleanMove.empty()) {
        switch (cleanMove[kBottomRank]) {
            case 'N':
                pieceType = ChessPieceType::KNIGHT;
                startPos = kPieceNotationPrefixLength;
                break;
            case 'B':
                pieceType = ChessPieceType::BISHOP;
                startPos = kPieceNotationPrefixLength;
                break;
            case 'R':
                pieceType = ChessPieceType::ROOK;
                startPos = kPieceNotationPrefixLength;
                break;
            case 'Q':
                pieceType = ChessPieceType::QUEEN;
                startPos = kPieceNotationPrefixLength;
                break;
            case 'K':
                pieceType = ChessPieceType::KING;
                startPos = kPieceNotationPrefixLength;
                break;
            default:
                pieceType = ChessPieceType::PAWN;
                startPos = kBottomRank;
                break;
        }
    }

    size_t xPos = cleanMove.find(kCaptureMarker);
    if (xPos != std::string::npos) {
        isCapture = true;
    }

    if (cleanMove.length() == kDestinationLength && pieceType == ChessPieceType::PAWN) {
        destCol = parseFile(cleanMove[kBottomRank]);
        destRow = parseRank(cleanMove[kPieceNotationPrefixLength]);

        if (!isWithinBoard(destCol, destRow)) {
            return false;
        }

        if (board.turn == ChessPieceColor::WHITE) {
            if (destRow > kBottomRank) {
                int checkRow = destRow - 1;
                int pos = toBoardIndex(checkRow, destCol);
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN &&
                    piece.PieceColor == ChessPieceColor::WHITE) {
                    int destPos = toBoardIndex(destRow, destCol);
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }

            if (destRow == kWhiteDoublePushDestinationRank) {
                int checkRow = kWhitePawnStartRank;
                int pos = toBoardIndex(checkRow, destCol);
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN &&
                    piece.PieceColor == ChessPieceColor::WHITE) {
                    int destPos = toBoardIndex(destRow, destCol);
                    int intermediatPos = toBoardIndex(kWhitePawnIntermediateRank, destCol);
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE &&
                        board.squares[intermediatPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        } else {
            if (destRow < kTopRank) {
                int checkRow = destRow + 1;
                int pos = toBoardIndex(checkRow, destCol);
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN &&
                    piece.PieceColor == ChessPieceColor::BLACK) {
                    int destPos = toBoardIndex(destRow, destCol);
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }

            if (destRow == kBlackPawnDoublePushDestinationRank) {
                int checkRow = kBlackPawnStartRank;
                int pos = toBoardIndex(checkRow, destCol);
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN &&
                    piece.PieceColor == ChessPieceColor::BLACK) {
                    int destPos = toBoardIndex(destRow, destCol);
                    int intermediatPos = toBoardIndex(kBlackPawnIntermediateRank, destCol);
                    if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE &&
                        board.squares[intermediatPos].piece.PieceType == ChessPieceType::NONE) {
                        srcCol = destCol;
                        srcRow = checkRow;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= kPawnCaptureNotationLength &&
        cleanMove[kPieceNotationPrefixLength] == kCaptureMarker) {
        destCol = parseFile(cleanMove[kKnightLongStep]);
        destRow = parseRank(cleanMove[kWhiteDoublePushDestinationRank]);
        srcCol = parseFile(cleanMove[kBottomRank]);
        if (!isWithinBoard(destCol, destRow) || srcCol < kLeftFile || srcCol > kRightFile) {
            return false;
        }

        for (int row = kBottomRank; row < BOARD_SIZE; row++) {
            int pos = toBoardIndex(row, srcCol);
            const Piece& piece = board.squares[pos].piece;
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                int from = pos;
                int to = toBoardIndex(destRow, destCol);
                if (board.turn == ChessPieceColor::WHITE) {
                    if ((to == from + kWhiteCaptureLeftOffset && srcCol > kLeftFile) ||
                        (to == from + kWhiteCaptureRightOffset && srcCol < kRightFile)) {
                        if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].piece.PieceColor == ChessPieceColor::BLACK) {
                            srcRow = row;
                            return true;
                        }
                    }
                } else {
                    if ((to == from + kBlackCaptureLeftOffset && srcCol > kLeftFile) ||
                        (to == from + kBlackCaptureRightOffset && srcCol < kRightFile)) {
                        if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].piece.PieceColor == ChessPieceColor::WHITE) {
                            srcRow = row;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= kPawnCaptureNotationLength) {
        size_t equalPos = cleanMove.find(kPromotionMarker);
        if (equalPos != std::string::npos && equalPos >= kDestinationLength) {
            destCol = parseFile(cleanMove[equalPos - kDestinationLength]);
            destRow = parseRank(cleanMove[equalPos - kPieceNotationPrefixLength]);

            if (!isWithinBoard(destCol, destRow)) {
                return false;
            }

            bool isPromotionRank =
                (destRow == kWhitePromotionRank && board.turn == ChessPieceColor::WHITE) ||
                (destRow == kBlackPromotionRank && board.turn == ChessPieceColor::BLACK);

            if (!isPromotionRank) {
                return false;
            }

            if (board.turn == ChessPieceColor::WHITE) {
                if (destRow > kBottomRank) {
                    int checkRow = destRow - 1;
                    int pos = toBoardIndex(checkRow, destCol);
                    const Piece& piece = board.squares[pos].piece;
                    if (piece.PieceType == ChessPieceType::PAWN &&
                        piece.PieceColor == ChessPieceColor::WHITE) {
                        int destPos = toBoardIndex(destRow, destCol);
                        if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                            srcCol = destCol;
                            srcRow = checkRow;
                            return true;
                        }
                    }
                }
            } else {
                if (destRow < kTopRank) {
                    int checkRow = destRow + 1;
                    int pos = toBoardIndex(checkRow, destCol);
                    const Piece& piece = board.squares[pos].piece;
                    if (piece.PieceType == ChessPieceType::PAWN &&
                        piece.PieceColor == ChessPieceColor::BLACK) {
                        int destPos = toBoardIndex(destRow, destCol);
                        if (board.squares[destPos].piece.PieceType == ChessPieceType::NONE) {
                            srcCol = destCol;
                            srcRow = checkRow;
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        if (cleanMove.length() >= kPromotionCaptureNotationLength &&
            cleanMove[kPieceNotationPrefixLength] == kCaptureMarker &&
            equalPos != std::string::npos) {
            destCol = parseFile(cleanMove[kKnightLongStep]);
            destRow = parseRank(cleanMove[kWhiteDoublePushDestinationRank]);
            srcCol = parseFile(cleanMove[kBottomRank]);

            if (!isWithinBoard(destCol, destRow) || srcCol < kLeftFile || srcCol > kRightFile) {
                return false;
            }

            bool isPromotionRank =
                (destRow == kWhitePromotionRank && board.turn == ChessPieceColor::WHITE) ||
                (destRow == kBlackPromotionRank && board.turn == ChessPieceColor::BLACK);

            if (!isPromotionRank) {
                return false;
            }

            for (int row = kBottomRank; row < BOARD_SIZE; row++) {
                int pos = toBoardIndex(row, srcCol);
                const Piece& piece = board.squares[pos].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                    int from = pos;
                    int to = toBoardIndex(destRow, destCol);
                    if (board.turn == ChessPieceColor::WHITE) {
                        if ((to == from + kWhiteCaptureLeftOffset && srcCol > kLeftFile) ||
                            (to == from + kWhiteCaptureRightOffset && srcCol < kRightFile)) {
                            if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[to].piece.PieceColor == ChessPieceColor::BLACK) {
                                srcRow = row;
                                return true;
                            }
                        }
                    } else {
                        if ((to == from + kBlackCaptureLeftOffset && srcCol > kLeftFile) ||
                            (to == from + kBlackCaptureRightOffset && srcCol < kRightFile)) {
                            if (board.squares[to].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[to].piece.PieceColor == ChessPieceColor::WHITE) {
                                srcRow = row;
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    }

    if (pieceType != ChessPieceType::PAWN) {
        if (cleanMove.length() < startPos + kDestinationLength) {
            return false;
        }

        if (isCapture) {
            size_t destStart = xPos + kPieceNotationPrefixLength;
            if (destStart + kPieceNotationPrefixLength >= cleanMove.length()) {
                return false;
            }
            destCol = parseFile(cleanMove[destStart]);
            destRow = parseRank(cleanMove[destStart + kPieceNotationPrefixLength]);
        } else {
            destCol = parseFile(cleanMove[startPos]);
            destRow = parseRank(cleanMove[startPos + kPieceNotationPrefixLength]);
        }

        if (!isWithinBoard(destCol, destRow)) {
            return false;
        }

        int destPos = toBoardIndex(destRow, destCol);
        const Piece& destPiece = board.squares[destPos].piece;

        if (isCapture) {
            if (destPiece.PieceType == ChessPieceType::NONE || destPiece.PieceColor == board.turn) {
                return false;
            }
        } else {
            if (destPiece.PieceType != ChessPieceType::NONE) {
                return false;
            }
        }

        std::vector<std::pair<int, int>> candidates;

        for (int row = kBottomRank; row < BOARD_SIZE; row++) {
            for (int col = kLeftFile; col < BOARD_SIZE; col++) {
                int pos = toBoardIndex(row, col);
                const Piece& piece = board.squares[pos].piece;

                if (piece.PieceType == pieceType && piece.PieceColor == board.turn) {
                    bool canReach = false;

                    switch (pieceType) {
                        case ChessPieceType::KNIGHT: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            canReach =
                                (rowDiff == kKnightLongStep && colDiff == kKnightShortStep) ||
                                (rowDiff == kKnightShortStep && colDiff == kKnightLongStep);
                            break;
                        }
                        case ChessPieceType::BISHOP: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            if (rowDiff == colDiff && rowDiff > kBottomRank) {

                                int rowStep = (destRow > row) ? kPieceNotationPrefixLength
                                                              : -kPieceNotationPrefixLength;
                                int colStep = (destCol > col) ? kPieceNotationPrefixLength
                                                              : -kPieceNotationPrefixLength;
                                canReach = true;
                                for (int i = kPieceNotationPrefixLength; i < rowDiff; i++) {
                                    int checkPos =
                                        toBoardIndex(row + (i * rowStep), col + (i * colStep));
                                    if (board.squares[checkPos].piece.PieceType !=
                                        ChessPieceType::NONE) {
                                        canReach = false;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::ROOK: {
                            if (row == destRow || col == destCol) {

                                canReach = true;
                                if (row == destRow) {
                                    int start = std::min(col, destCol) + kPieceNotationPrefixLength;
                                    int end = std::max(col, destCol);
                                    for (int c = start; c < end; c++) {
                                        if (board.squares[toBoardIndex(row, c)].piece.PieceType !=
                                            ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else {
                                    int start = std::min(row, destRow) + kPieceNotationPrefixLength;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[toBoardIndex(r, col)].piece.PieceType !=
                                            ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::QUEEN: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);

                            if (row == destRow || col == destCol || rowDiff == colDiff) {
                                canReach = true;

                                if (row == destRow) {
                                    int start = std::min(col, destCol) + kPieceNotationPrefixLength;
                                    int end = std::max(col, destCol);
                                    for (int c = start; c < end; c++) {
                                        if (board.squares[toBoardIndex(row, c)].piece.PieceType !=
                                            ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (col == destCol) {
                                    int start = std::min(row, destRow) + kPieceNotationPrefixLength;
                                    int end = std::max(row, destRow);
                                    for (int r = start; r < end; r++) {
                                        if (board.squares[toBoardIndex(r, col)].piece.PieceType !=
                                            ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                } else if (rowDiff == colDiff) {
                                    int rowStep = (destRow > row) ? kPieceNotationPrefixLength
                                                                  : -kPieceNotationPrefixLength;
                                    int colStep = (destCol > col) ? kPieceNotationPrefixLength
                                                                  : -kPieceNotationPrefixLength;
                                    for (int i = kPieceNotationPrefixLength; i < rowDiff; i++) {
                                        int checkPos =
                                            toBoardIndex(row + (i * rowStep), col + (i * colStep));
                                        if (board.squares[checkPos].piece.PieceType !=
                                            ChessPieceType::NONE) {
                                            canReach = false;
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case ChessPieceType::KING: {
                            int rowDiff = abs(destRow - row);
                            int colDiff = abs(destCol - col);
                            canReach = rowDiff <= kKingReach && colDiff <= kKingReach &&
                                       (rowDiff + colDiff) > kBottomRank;
                            break;
                        }
                        default:
                            break;
                    }

                    if (canReach) {
                        candidates.emplace_back(col, row);
                    }
                }
            }
        }

        if (!candidates.empty()) {
            srcCol = candidates[kFirstCandidateIndex].first;
            srcRow = candidates[kFirstCandidateIndex].second;
            return true;
        }
    }

    return false;
}

std::expected<ParsedAlgebraicMove, ParseAlgebraicMoveError> parseAlgebraicMove(
    std::string_view move, const Board& board) {
    ParsedAlgebraicMove parsed{};
    if (!parseAlgebraicMoveLegacy(move, board, parsed.srcCol, parsed.srcRow, parsed.destCol,
                                  parsed.destRow)) {
        return std::unexpected(ParseAlgebraicMoveError::InvalidNotation);
    }
    return parsed;
}

bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol,
                        int& destRow) {
    auto parsed = parseAlgebraicMove(move, static_cast<const Board&>(board));
    if (!parsed) {
        return false;
    }
    srcCol = parsed->srcCol;
    srcRow = parsed->srcRow;
    destCol = parsed->destCol;
    destRow = parsed->destRow;
    return true;
}

ChessPieceType getPromotionPiece(std::string_view move) {
    std::string cleanMove(move);
    if (!cleanMove.empty() &&
        (cleanMove.back() == kCheckMarker || cleanMove.back() == kMateMarker)) {
        cleanMove.pop_back();
    }

    size_t equalPos = cleanMove.find(kPromotionMarker);
    if (equalPos != std::string::npos &&
        equalPos + kPieceNotationPrefixLength < cleanMove.length()) {
        char promotionChar = cleanMove[equalPos + kPieceNotationPrefixLength];
        switch (promotionChar) {
            case 'Q':
            case 'q':
                return ChessPieceType::QUEEN;
            case 'R':
            case 'r':
                return ChessPieceType::ROOK;
            case 'B':
            case 'b':
                return ChessPieceType::BISHOP;
            case 'N':
            case 'n':
                return ChessPieceType::KNIGHT;
            default:
                return ChessPieceType::QUEEN;
        }
    }
    return ChessPieceType::QUEEN;
}
