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

    // History navigation
    void undo();
    void redo();
    void goToHistoryIndex(size_t index);

    // Start a new match with the currently selected difficulty
    void startMatch();

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
    bool m_gameStarted = false;
    std::optional<std::pair<int,int>> m_selected;

    // Cached evaluation history per ply (half-move). index 0 == initial position
    std::vector<float> m_evaluationHistory;
    size_t m_historyIndex = 0; // current displayed history index

    // Player/AI configuration
    int m_targetElo = 500; // allowed: 500,1000,1500,2000
    PieceColor m_playerColor = PieceColor::White;
    int m_timePerMoveMs = 1000;

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
