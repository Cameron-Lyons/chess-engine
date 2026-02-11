#include "OpeningBook.h"

#include <memory>

std::unique_ptr<OpeningBook> g_openingBook;

OpeningBook::OpeningBook(const BookConfig& config) : config(config), rng(std::random_device{}()) {}

std::string OpeningBook::getMove(const Board& board) {
    (void)board;

    return "";
}

bool OpeningBook::isInBook(const Board& board) const {
    (void)board;

    return false;
}

void initializeOpeningBook(const OpeningBook::BookConfig& config) {
    g_openingBook = std::make_unique<OpeningBook>(config);
}

OpeningBook* getOpeningBook() {
    return g_openingBook.get();
}

std::string getBookMove(const Board& board) {
    if (g_openingBook)
        return g_openingBook->getMove(board);
    return "";
}

bool isBookMove(const Board& board) {
    if (g_openingBook)
        return g_openingBook->isInBook(board);
    return false;
}