#ifndef UCI_H
#define UCI_H

#include "ChessBoard.h"
#include "search.h"
#include <string>
#include <vector>
#include <sstream>

class UCIEngine {
private:
    Board board;
    bool isRunning;
    int thinkingTime;
    int depth;
    
public:
    UCIEngine();
    void run();
    void processCommand(const std::string& command);
    
    void handleUCI();
    void handleIsReady();
    void handleUCINewGame();
    void handlePosition(const std::string& command);
    void handleGo(const std::string& command);
    void handleStop();
    void handleQuit();
    
    std::string moveToUCI(std::pair<int, int> move);
    std::pair<int, int> uciToMove(const std::string& uciMove);
    void sendBestMove(std::pair<int, int> move);
    void sendInfo(int depth, int score, int nodes, int time, const std::string& pv);
};

#endif 