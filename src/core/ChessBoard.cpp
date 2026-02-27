#include "ChessBoard.h"

#include <cctype>
#include <sstream>

void Board::clearBitboards() {
    whitePawns = whiteKnights = whiteBishops = whiteRooks = whiteQueens = whiteKings = 0;
    blackPawns = blackKnights = blackBishops = blackRooks = blackQueens = blackKings = 0;
    whitePieces = blackPieces = allPieces = 0;
}

void Board::updateBitboards() {
    clearBitboards();
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        set_bit(whitePawns, i);
                        break;
                    case ChessPieceType::KNIGHT:
                        set_bit(whiteKnights, i);
                        break;
                    case ChessPieceType::BISHOP:
                        set_bit(whiteBishops, i);
                        break;
                    case ChessPieceType::ROOK:
                        set_bit(whiteRooks, i);
                        break;
                    case ChessPieceType::QUEEN:
                        set_bit(whiteQueens, i);
                        break;
                    case ChessPieceType::KING:
                        set_bit(whiteKings, i);
                        break;
                    default:
                        break;
                }
            } else {
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        set_bit(blackPawns, i);
                        break;
                    case ChessPieceType::KNIGHT:
                        set_bit(blackKnights, i);
                        break;
                    case ChessPieceType::BISHOP:
                        set_bit(blackBishops, i);
                        break;
                    case ChessPieceType::ROOK:
                        set_bit(blackRooks, i);
                        break;
                    case ChessPieceType::QUEEN:
                        set_bit(blackQueens, i);
                        break;
                    case ChessPieceType::KING:
                        set_bit(blackKings, i);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    updateOccupancy();
}

void Board::updateOccupancy() {
    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKings;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKings;
    allPieces = whitePieces | blackPieces;
}

Bitboard Board::getPieceBitboard(ChessPieceType type, ChessPieceColor color) const {
    if (color == ChessPieceColor::WHITE) {
        switch (type) {
            case ChessPieceType::PAWN:
                return whitePawns;
            case ChessPieceType::KNIGHT:
                return whiteKnights;
            case ChessPieceType::BISHOP:
                return whiteBishops;
            case ChessPieceType::ROOK:
                return whiteRooks;
            case ChessPieceType::QUEEN:
                return whiteQueens;
            case ChessPieceType::KING:
                return whiteKings;
            default:
                return 0;
        }
    } else {
        switch (type) {
            case ChessPieceType::PAWN:
                return blackPawns;
            case ChessPieceType::KNIGHT:
                return blackKnights;
            case ChessPieceType::BISHOP:
                return blackBishops;
            case ChessPieceType::ROOK:
                return blackRooks;
            case ChessPieceType::QUEEN:
                return blackQueens;
            case ChessPieceType::KING:
                return blackKings;
            default:
                return 0;
        }
    }
}

bool Board::movePiece(int from, int to) {
    if (from < 0 || from >= NUM_SQUARES || to < 0 || to >= NUM_SQUARES) {
        return false;
    }
    Piece& fromPiece = squares[from].piece;
    Piece& toPiece = squares[to].piece;
    const Piece movingPieceBefore = fromPiece;
    const Piece capturedPieceBefore = toPiece;

    if (fromPiece.PieceType == ChessPieceType::NONE) {
        return false;
    }
    if (fromPiece.PieceColor == ChessPieceColor::WHITE) {
        switch (fromPiece.PieceType) {
            case ChessPieceType::PAWN:
                clear_bit(whitePawns, from);
                break;
            case ChessPieceType::KNIGHT:
                clear_bit(whiteKnights, from);
                break;
            case ChessPieceType::BISHOP:
                clear_bit(whiteBishops, from);
                break;
            case ChessPieceType::ROOK:
                clear_bit(whiteRooks, from);
                break;
            case ChessPieceType::QUEEN:
                clear_bit(whiteQueens, from);
                break;
            case ChessPieceType::KING:
                clear_bit(whiteKings, from);
                break;
            default:
                break;
        }
    } else {
        switch (fromPiece.PieceType) {
            case ChessPieceType::PAWN:
                clear_bit(blackPawns, from);
                break;
            case ChessPieceType::KNIGHT:
                clear_bit(blackKnights, from);
                break;
            case ChessPieceType::BISHOP:
                clear_bit(blackBishops, from);
                break;
            case ChessPieceType::ROOK:
                clear_bit(blackRooks, from);
                break;
            case ChessPieceType::QUEEN:
                clear_bit(blackQueens, from);
                break;
            case ChessPieceType::KING:
                clear_bit(blackKings, from);
                break;
            default:
                break;
        }
    }

    if (toPiece.PieceType != ChessPieceType::NONE) {
        if (toPiece.PieceColor == ChessPieceColor::WHITE) {
            switch (toPiece.PieceType) {
                case ChessPieceType::PAWN:
                    clear_bit(whitePawns, to);
                    break;
                case ChessPieceType::KNIGHT:
                    clear_bit(whiteKnights, to);
                    break;
                case ChessPieceType::BISHOP:
                    clear_bit(whiteBishops, to);
                    break;
                case ChessPieceType::ROOK:
                    clear_bit(whiteRooks, to);
                    break;
                case ChessPieceType::QUEEN:
                    clear_bit(whiteQueens, to);
                    break;
                case ChessPieceType::KING:
                    clear_bit(whiteKings, to);
                    break;
                default:
                    break;
            }
        } else {
            switch (toPiece.PieceType) {
                case ChessPieceType::PAWN:
                    clear_bit(blackPawns, to);
                    break;
                case ChessPieceType::KNIGHT:
                    clear_bit(blackKnights, to);
                    break;
                case ChessPieceType::BISHOP:
                    clear_bit(blackBishops, to);
                    break;
                case ChessPieceType::ROOK:
                    clear_bit(blackRooks, to);
                    break;
                case ChessPieceType::QUEEN:
                    clear_bit(blackQueens, to);
                    break;
                case ChessPieceType::KING:
                    clear_bit(blackKings, to);
                    break;
                default:
                    break;
            }
        }
    }

    toPiece = fromPiece;
    fromPiece = Piece();
    toPiece.moved = true;
    LastMove = to;
    recordMoveTime();

    if (toPiece.PieceColor == ChessPieceColor::WHITE) {
        switch (toPiece.PieceType) {
            case ChessPieceType::PAWN:
                set_bit(whitePawns, to);
                break;
            case ChessPieceType::KNIGHT:
                set_bit(whiteKnights, to);
                break;
            case ChessPieceType::BISHOP:
                set_bit(whiteBishops, to);
                break;
            case ChessPieceType::ROOK:
                set_bit(whiteRooks, to);
                break;
            case ChessPieceType::QUEEN:
                set_bit(whiteQueens, to);
                break;
            case ChessPieceType::KING:
                set_bit(whiteKings, to);
                break;
            default:
                break;
        }
    } else {
        switch (toPiece.PieceType) {
            case ChessPieceType::PAWN:
                set_bit(blackPawns, to);
                break;
            case ChessPieceType::KNIGHT:
                set_bit(blackKnights, to);
                break;
            case ChessPieceType::BISHOP:
                set_bit(blackBishops, to);
                break;
            case ChessPieceType::ROOK:
                set_bit(blackRooks, to);
                break;
            case ChessPieceType::QUEEN:
                set_bit(blackQueens, to);
                break;
            case ChessPieceType::KING:
                set_bit(blackKings, to);
                break;
            default:
                break;
        }
    }

    updateOccupancy();

    if (movingPieceBefore.PieceType == ChessPieceType::KING) {
        if (movingPieceBefore.PieceColor == ChessPieceColor::WHITE) {
            whiteCanCastle = false;
        } else {
            blackCanCastle = false;
        }
    }

    if (movingPieceBefore.PieceType == ChessPieceType::ROOK) {
        if (from == 0 || from == 7) {
            whiteCanCastle = false;
        } else if (from == 56 || from == 63) {
            blackCanCastle = false;
        }
    }

    if (capturedPieceBefore.PieceType == ChessPieceType::ROOK) {
        if (to == 0 || to == 7) {
            whiteCanCastle = false;
        } else if (to == 56 || to == 63) {
            blackCanCastle = false;
        }
    }

    enPassantSquare = -1;
    int moveDelta = to - from;
    if (movingPieceBefore.PieceType == ChessPieceType::PAWN &&
        (moveDelta == 16 || moveDelta == -16)) {
        enPassantSquare = (from + to) / 2;
    }
    return true;
}

ChessError Board::validateMove(int from, int to) const {
    if (from < 0 || from >= NUM_SQUARES || to < 0 || to >= NUM_SQUARES) {
        return ChessError::InvalidPosition;
    }

    const Piece& fromPiece = squares[from].piece;
    if (fromPiece.PieceType == ChessPieceType::NONE) {
        return ChessError::NoPieceAtSource;
    }

    if (fromPiece.PieceColor != turn) {
        return ChessError::WrongTurn;
    }

    const auto& validMoves = squares[from].ValidMoves;
    if (std::ranges::find(validMoves, to) == validMoves.end()) {
        return ChessError::InvalidMove;
    }

    return ChessError::InvalidMove;
}

std::string Board::toFEN() const {
    std::string fen;

    for (int row = 7; row >= 0; --row) {
        int emptyCount = 0;
        for (int col = 0; col < BOARD_SIZE; ++col) {
            int pos = (row * BOARD_SIZE) + col;
            const Piece& piece = squares[pos].piece;

            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }

                constexpr char pieceChars[] = {'P', 'N', 'B', 'R', 'Q', 'K', '?'};
                char pieceChar = pieceChars[static_cast<int>(piece.PieceType)];

                if (piece.PieceColor == ChessPieceColor::BLACK) {
                    pieceChar =
                        static_cast<char>(std::tolower(static_cast<unsigned char>(pieceChar)));
                }
                fen += pieceChar;
            }
        }

        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }

        if (row > 0) {
            fen += "/";
        }
    }

    fen += " " + std::string(turn == ChessPieceColor::WHITE ? "w" : "b");
    std::string castling;
    if (whiteCanCastle) {
        castling += "K";
        castling += "Q";
    }
    if (blackCanCastle) {
        castling += "k";
        castling += "q";
    }
    fen += " " + (castling.empty() ? "-" : castling);
    if (enPassantSquare >= 0 && enPassantSquare < NUM_SQUARES) {
        int epFile = enPassantSquare % BOARD_SIZE;
        int epRank = enPassantSquare / BOARD_SIZE;
        fen += " ";
        fen += static_cast<char>('a' + epFile);
        fen += static_cast<char>('1' + epRank);
    } else {
        fen += " -";
    }
    fen += " 0 1";
    return fen;
}

bool Board::fromFEN(ChessString fen) {
    InitializeFromFEN(fen);
    return true;
}

void Board::InitializeFromFEN(ChessString fen) {
    for (int i = 0; i < NUM_SQUARES; i++) {
        squares[i] = Square(i);
        squares[i].piece = Piece();
    }

    turn = ChessPieceColor::WHITE;
    whiteCanCastle = false;
    blackCanCastle = false;
    enPassantSquare = -1;
    whiteChecked = false;
    blackChecked = false;
    LastMove = 0;

    std::string fenString(fen);
    std::istringstream iss(fenString);
    std::string placement;
    std::string activeColor;
    std::string castling;
    std::string enPassant;
    if (!(iss >> placement)) {
        updateBitboards();
        return;
    }
    if (!(iss >> activeColor)) {
        activeColor = "w";
    }
    if (!(iss >> castling)) {
        castling = "-";
    }
    if (!(iss >> enPassant)) {
        enPassant = "-";
    }

    size_t pos = 0;
    int fenRank = 7;
    int file = 0;

    while (pos < placement.length()) {
        char c = placement[pos];
        if (c == '/') {
            fenRank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += (c - '0');
        } else {
            ChessPieceType type = ChessPieceType::NONE;
            ChessPieceColor color = ChessPieceColor::WHITE;

            switch (std::tolower(static_cast<unsigned char>(c))) {
                case 'p':
                    type = ChessPieceType::PAWN;
                    break;
                case 'n':
                    type = ChessPieceType::KNIGHT;
                    break;
                case 'b':
                    type = ChessPieceType::BISHOP;
                    break;
                case 'r':
                    type = ChessPieceType::ROOK;
                    break;
                case 'q':
                    type = ChessPieceType::QUEEN;
                    break;
                case 'k':
                    type = ChessPieceType::KING;
                    break;
                default:
                    break;
            }

            if (isupper(c)) {
                color = ChessPieceColor::WHITE;
            } else {
                color = ChessPieceColor::BLACK;
            }
            if (type != ChessPieceType::NONE && fenRank >= 0 && fenRank < BOARD_SIZE && file >= 0 &&
                file < BOARD_SIZE) {
                int boardRow = fenRank;
                int idx = (boardRow * BOARD_SIZE) + file;
                if (idx >= 0 && idx < NUM_SQUARES) {
                    squares[idx].piece = Piece(color, type);
                }
            }
            file++;
        }
        pos++;
    }

    turn = (activeColor == "b") ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    if (castling != "-") {
        whiteCanCastle = (castling.find('K') != std::string::npos) ||
                         (castling.find('Q') != std::string::npos);
        blackCanCastle = (castling.find('k') != std::string::npos) ||
                         (castling.find('q') != std::string::npos);
    }

    if (enPassant != "-" && enPassant.size() == 2) {
        int epFile = static_cast<int>(enPassant[0] - 'a');
        int epRank = static_cast<int>(enPassant[1] - '1');
        if (epFile >= 0 && epFile < BOARD_SIZE && epRank >= 0 && epRank < BOARD_SIZE) {
            enPassantSquare = (epRank * BOARD_SIZE) + epFile;
        }
    }

    updateBitboards();
}
