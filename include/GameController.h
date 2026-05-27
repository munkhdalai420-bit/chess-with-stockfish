#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Board.h"
#include "EngineManager.h"
#include "raylib.h"

class GameController
{
public:
    GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth);
    ~GameController();

    // Elo options and configuration
    static const int ELO_COUNT = 4;
    static const int ELO_OPTIONS[ELO_COUNT];

    int getTargetElo() const;
    void cycleTargetElo();

    bool isMatchStarted() const;
    void startMatch();
    void endMatch();

    // History navigation
    void undo();
    void redo();
    void goToHistoryIndex(size_t index);

    // Returns the evaluation corresponding to the currently displayed history index
    float getDisplayedEvaluation() const;

    // Process input and advance game state. Should be called once per frame.
    void update();

    Board& getBoard();
    std::optional<std::pair<int,int>> getSelected() const;

    // Temporary move message to display in the UI
    std::string getMoveMessage() const;
    float getMessageTimer() const;

private:
    Board m_board;
    EngineManager m_engine;

    bool m_aiIsThinking = false;
    bool m_isReviewingHistory = false;
    std::optional<std::pair<int,int>> m_selected;

    // Cached evaluation history per ply (half-move). index 0 == initial position
    std::vector<float> m_evaluationHistory;
    size_t m_historyIndex = 0; // current displayed history index

    // Player/AI configuration
    int m_targetElo = 2000; // allowed: 500,1000,1500,2000
    PieceColor m_playerColor = PieceColor::White;
    int m_timePerMoveMs = 1000;
    int m_eloIndex = 3; // index into ELO_OPTIONS (default to 2000)
    bool m_gameStarted = false;

    std::string m_moveMessage;
    float m_messageTimer = 0.0f;

    int m_windowWidth;
    int m_windowHeight;
    int m_tileSize;
    int m_sidebarWidth;

    // Sound effects
    Sound m_sndCapture;
    Sound m_sndCastle;
    Sound m_sndMoveCheck;
    Sound m_sndMoveSelf;
    Sound m_sndPromote;
};
