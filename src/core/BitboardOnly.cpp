#include "BitboardOnly.h"

#include <cctype>
#include <sstream>

static uint64_t zobristPieces[2][6][64];
static uint64_t zobristSideToMove;
static uint64_t zobristCastling[16];
static uint64_t zobristEnPassant[8];
static bool zobristInitialized = false;

static void initializeZobrist() {
    if (zobristInitialized) {
        return;
    }
    uint64_t seed = 0x123456789ABCDEF0ULL;
    auto next = [&seed]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    };

    for (auto& zobristPiece : zobristPieces) {
        for (auto& piece : zobristPiece) {
            for (auto& squareKey : piece) {
                squareKey = next();
            }
        }
    }

    zobristSideToMove = next();

    for (unsigned long long& i : zobristCastling) {
        i = next();
    }

    for (unsigned long long& i : zobristEnPassant) {
        i = next();
    }

    zobristInitialized = true;
}

BitboardPosition::BitboardPosition()
    : sideToMove(WHITE), castlingRights(CastlingConstants::kAllCastlingRightsMask),
      epSquare(CastlingConstants::kNoEnPassantSquareBitboard), halfmoveClock(0), fullmoveNumber(1),
      hash(0) {
    initializeZobrist();

    for (auto& color : pieces) {
        for (unsigned long long& piece : color) {
            piece = 0;
        }
    }

    for (unsigned long long& i : occupancy) {
        i = 0;
    }

    lastMoveTime = ChessClock::now();
}

void BitboardPosition::updateOccupancy() {
    occupancy[WHITE] = pieces[WHITE][PAWN] | pieces[WHITE][KNIGHT] | pieces[WHITE][BISHOP] |
                       pieces[WHITE][ROOK] | pieces[WHITE][QUEEN] | pieces[WHITE][KING];

    occupancy[BLACK] = pieces[BLACK][PAWN] | pieces[BLACK][KNIGHT] | pieces[BLACK][BISHOP] |
                       pieces[BLACK][ROOK] | pieces[BLACK][QUEEN] | pieces[BLACK][KING];

    occupancy[2] = occupancy[WHITE] | occupancy[BLACK];
}

void BitboardPosition::removePiece(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type);
    pieces[c][p] &= ~(1ULL << square);
    updateHash(square, color, type);
}

void BitboardPosition::placePiece(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type);
    pieces[c][p] |= (1ULL << square);
    updateHash(square, color, type);
}

void BitboardPosition::updateHash(int square, ChessPieceColor color, ChessPieceType type) {
    int c = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    int p = static_cast<int>(type);
    hash ^= zobristPieces[c][p][square];
}

void BitboardPosition::makeMove(int from, int to, ChessPieceType promotion) {
    ChessPieceType movingPiece = getPieceAt(from);
    ChessPieceColor movingColor = getColorAt(from);
    ChessPieceType capturedPiece = getPieceAt(to);
    removePiece(from, movingColor, movingPiece);

    if (capturedPiece != ChessPieceType::NONE) {
        ChessPieceColor capturedColor = getColorAt(to);
        removePiece(to, capturedColor, capturedPiece);
    }

    ChessPieceType finalPiece = (promotion != ChessPieceType::NONE) ? promotion : movingPiece;
    placePiece(to, movingColor, finalPiece);

    if (movingPiece == ChessPieceType::PAWN && to == epSquare &&
        epSquare < CastlingConstants::kNoEnPassantSquareBitboard) {
        int captureSquare = (movingColor == ChessPieceColor::WHITE) ? to - 8 : to + 8;
        ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE)
                                         ? ChessPieceColor::BLACK
                                         : ChessPieceColor::WHITE;
        removePiece(captureSquare, enemyColor, ChessPieceType::PAWN);
    }

    if (movingPiece == ChessPieceType::PAWN &&
        abs(to - from) == CastlingConstants::kPawnDoublePushDistance) {
        epSquare = (from + to) / 2;
    } else {
        epSquare = CastlingConstants::kNoEnPassantSquareBitboard;
    }

    if (movingPiece == ChessPieceType::KING &&
        abs(to - from) == CastlingConstants::kKingCastlingDistance) {
        int rookFrom = 0;
        int rookTo = 0;
        if (to > from) {
            rookFrom = to + 1;
            rookTo = to - 1;
        } else {
            rookFrom = to - 2;
            rookTo = to + 1;
        }

        removePiece(rookFrom, movingColor, ChessPieceType::ROOK);
        placePiece(rookTo, movingColor, ChessPieceType::ROOK);
    }

    if (movingPiece == ChessPieceType::KING) {
        if (movingColor == ChessPieceColor::WHITE) {
            castlingRights &= static_cast<uint8_t>(~CastlingConstants::kWhiteCastlingRightsMask);
        } else {
            castlingRights &= static_cast<uint8_t>(~CastlingConstants::kBlackCastlingRightsMask);
        }
    } else if (movingPiece == ChessPieceType::ROOK) {
        if (from == CastlingConstants::kWhiteQueensideRookSquare) {
            castlingRights &=
                static_cast<uint8_t>(~CastlingConstants::kWhiteQueensideCastlingRight);
        } else if (from == CastlingConstants::kWhiteKingsideRookSquare) {
            castlingRights &= static_cast<uint8_t>(~CastlingConstants::kWhiteKingsideCastlingRight);
        } else if (from == CastlingConstants::kBlackQueensideRookSquare) {
            castlingRights &=
                static_cast<uint8_t>(~CastlingConstants::kBlackQueensideCastlingRight);
        } else if (from == CastlingConstants::kBlackKingsideRookSquare) {
            castlingRights &= static_cast<uint8_t>(~CastlingConstants::kBlackKingsideCastlingRight);
        }
    }

    if (movingPiece == ChessPieceType::PAWN || capturedPiece != ChessPieceType::NONE) {
        halfmoveClock = 0;
    } else {
        halfmoveClock++;
    }

    if (sideToMove == BLACK) {
        fullmoveNumber++;
    }

    sideToMove = 1 - sideToMove;
    hash ^= zobristSideToMove;
    updateOccupancy();
    recordMoveTime();
}

void BitboardPosition::unmakeMove(int from, int to, ChessPieceType captured,
                                  ChessPieceType promotion) {

    ChessPieceType movedPiece = getPieceAt(to);
    ChessPieceColor movedColor = getColorAt(to);
    removePiece(to, movedColor, movedPiece);

    ChessPieceType originalPiece =
        (promotion != ChessPieceType::NONE) ? ChessPieceType::PAWN : movedPiece;
    placePiece(from, movedColor, originalPiece);

    if (captured != ChessPieceType::NONE) {
        ChessPieceColor capturedColor = (movedColor == ChessPieceColor::WHITE)
                                            ? ChessPieceColor::BLACK
                                            : ChessPieceColor::WHITE;
        placePiece(to, capturedColor, captured);
    }

    sideToMove = 1 - sideToMove;
    hash ^= zobristSideToMove;
    updateOccupancy();
}

void BitboardPosition::setFromFEN(const std::string& fen) {

    for (auto& color : pieces) {
        for (unsigned long long& piece : color) {
            piece = 0;
        }
    }

    std::istringstream ss(fen);
    std::string boardStr;
    std::string turn;
    std::string castling;
    std::string ep;
    ss >> boardStr >> turn >> castling >> ep >> halfmoveClock >> fullmoveNumber;
    int rank = 7;
    int file = 0;
    for (char c : boardStr) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
        } else {
            int square = (rank * 8) + file;
            ChessPieceColor color = std::isupper(static_cast<unsigned char>(c))
                                        ? ChessPieceColor::WHITE
                                        : ChessPieceColor::BLACK;
            ChessPieceType type = Piece::fromFenChar(c);
            if (type == ChessPieceType::NONE) {
                continue;
            }

            placePiece(square, color, type);
            file++;
        }
    }

    sideToMove = (turn == "w") ? WHITE : BLACK;
    castlingRights = 0;
    if (castling.contains('K')) {
        castlingRights |= CastlingConstants::kWhiteKingsideCastlingRight;
    }
    if (castling.contains('Q')) {
        castlingRights |= CastlingConstants::kWhiteQueensideCastlingRight;
    }
    if (castling.contains('k')) {
        castlingRights |= CastlingConstants::kBlackKingsideCastlingRight;
    }
    if (castling.contains('q')) {
        castlingRights |= CastlingConstants::kBlackQueensideCastlingRight;
    }
    if (ep != "-" && ep.length() >= 2) {
        int file = ep[0] - 'a';
        int rank = ep[1] - '1';
        epSquare = (rank * 8) + file;
    } else {
        epSquare = CastlingConstants::kNoEnPassantSquareBitboard;
    }

    updateOccupancy();
    hash = 0;
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            Bitboard bb = pieces[color][piece];
            while (bb) {
                int square = std::countr_zero(bb);
                hash ^= zobristPieces[color][piece][square];
                bb &= bb - 1;
            }
        }
    }

    if (sideToMove == BLACK) {
        hash ^= zobristSideToMove;
    }
    hash ^= zobristCastling[castlingRights];
    if (epSquare < CastlingConstants::kNoEnPassantSquareBitboard) {
        hash ^= zobristEnPassant[epSquare & 7];
    }
}

std::string BitboardPosition::toFEN() const {
    std::stringstream ss;

    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            int square = (rank * 8) + file;
            ChessPieceType piece = getPieceAt(square);

            if (piece == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    ss << emptyCount;
                    emptyCount = 0;
                }

                char pieceChar = 0;
                switch (piece) {
                    case ChessPieceType::PAWN:
                        pieceChar = 'p';
                        break;
                    case ChessPieceType::KNIGHT:
                        pieceChar = 'n';
                        break;
                    case ChessPieceType::BISHOP:
                        pieceChar = 'b';
                        break;
                    case ChessPieceType::ROOK:
                        pieceChar = 'r';
                        break;
                    case ChessPieceType::QUEEN:
                        pieceChar = 'q';
                        break;
                    case ChessPieceType::KING:
                        pieceChar = 'k';
                        break;
                    default:
                        pieceChar = '?';
                }

                if (getColorAt(square) == ChessPieceColor::WHITE) {
                    pieceChar =
                        static_cast<char>(std::toupper(static_cast<unsigned char>(pieceChar)));
                }

                ss << pieceChar;
            }
        }

        if (emptyCount > 0) {
            ss << emptyCount;
        }

        if (rank > 0) {
            ss << '/';
        }
    }

    ss << ' ' << (sideToMove == WHITE ? 'w' : 'b');
    ss << ' ';
    std::string castling;
    if (castlingRights & CastlingConstants::kWhiteKingsideCastlingRight) {
        castling += 'K';
    }
    if (castlingRights & CastlingConstants::kWhiteQueensideCastlingRight) {
        castling += 'Q';
    }
    if (castlingRights & CastlingConstants::kBlackKingsideCastlingRight) {
        castling += 'k';
    }
    if (castlingRights & CastlingConstants::kBlackQueensideCastlingRight) {
        castling += 'q';
    }
    ss << (castling.empty() ? "-" : castling);
    ss << ' ';
    if (epSquare < CastlingConstants::kNoEnPassantSquareBitboard) {
        ss << char('a' + (epSquare & 7)) << char('1' + (epSquare >> 3));
    } else {
        ss << '-';
    }

    ss << ' ' << int(halfmoveClock) << ' ' << fullmoveNumber;
    return ss.str();
}

std::string BitboardPosition::toString() const {
    std::stringstream ss;
    ss << "  a b c d e f g h\n";
    for (int rank = 7; rank >= 0; --rank) {
        ss << (rank + 1) << ' ';
        for (int file = 0; file < 8; ++file) {
            int square = (rank * 8) + file;
            ChessPieceType piece = getPieceAt(square);

            if (piece == ChessPieceType::NONE) {
                ss << ". ";
            } else {
                char pieceChar = 0;
                switch (piece) {
                    case ChessPieceType::PAWN:
                        pieceChar = 'p';
                        break;
                    case ChessPieceType::KNIGHT:
                        pieceChar = 'n';
                        break;
                    case ChessPieceType::BISHOP:
                        pieceChar = 'b';
                        break;
                    case ChessPieceType::ROOK:
                        pieceChar = 'r';
                        break;
                    case ChessPieceType::QUEEN:
                        pieceChar = 'q';
                        break;
                    case ChessPieceType::KING:
                        pieceChar = 'k';
                        break;
                    default:
                        pieceChar = '?';
                }

                if (getColorAt(square) == ChessPieceColor::WHITE) {
                    pieceChar =
                        static_cast<char>(std::toupper(static_cast<unsigned char>(pieceChar)));
                }

                ss << pieceChar << ' ';
            }
        }
        ss << (rank + 1) << '\n';
    }
    ss << "  a b c d e f g h\n";
    ss << "Turn: " << (sideToMove == WHITE ? "White" : "Black") << '\n';
    ss << "FEN: " << toFEN() << '\n';
    return ss.str();
}
