#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Board.h"
#include "EngineManager.h"
#include "raylib.h"

class Renderer;

class GameController
{
public:
    // Player side selection (view from the human player's perspective)
    enum class PlayerColor { White, Black };

    /**
     * @brief Construct a GameController
     *
     * Initializes audio, launches the engine, configures the initial board
     * and evaluation history, and wires a non-owning Renderer pointer.
     * @param windowWidth Application window width in pixels.
     * @param windowHeight Application window height in pixels.
     * @param tileSize Size of one chess tile in pixels.
     * @param sidebarWidth Width reserved for the UI sidebar panel.
     * @param renderer Non-owning pointer to the Renderer used for drawing.
     */
    GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth, Renderer* renderer);

    /**
     * @brief Destructor
     *
     * Stops the engine and releases audio resources if enabled.
     */
    ~GameController();

    // Elo options and configuration
    static const int ELO_COUNT = 4;
    static const int ELO_OPTIONS[ELO_COUNT];

    /** @brief Get the currently selected engine Elo. */
    int getTargetElo() const;
    /** @brief Cycle to the next Elo option and update the engine difficulty. */
    void cycleTargetElo();

    /** @brief Returns true when a match is currently active. */
    bool isMatchStarted() const;
    /** @brief Start a new match (reset board, history and engine state). */
    void startMatch();
    /** @brief End the current match and return to lobby/review state. */
    void endMatch();

    // History navigation
    /** @brief Undo the last ply and enter review mode. */
    void undo();
    /** @brief Redo a previously undone ply and enter review mode. */
    void redo();
    /**
     * @brief Jump to a specific history/evaluation index.
     * @param index Target history index (0 == initial position).
     */
    void goToHistoryIndex(size_t index);

    // Returns the evaluation corresponding to the currently displayed history index
    /** @brief Evaluation value corresponding to the current history index. */
    float getDisplayedEvaluation() const;
    /** @brief True if the engine has reported a mate line. */
    bool isMateDetected() const;
    /** @brief If mate detected, mate distance in plies (signed by side). */
    int getMateInMoves() const;

    // Undo/Redo availability for UI
    bool canUndo() const;
    bool canRedo() const;

    // Process input and advance game state. Should be called once per frame.
    /**
     * @brief Per-frame update.
     *
     * Polls input, handles promotion UI, triggers or polls the engine
     * as required, updates timers and checks terminal state.
     */
    void update();
    // Request a short engine search to compute a hint move for the current position.
    /**
     * @brief Request a short, fixed-time engine search to compute a hint.
     *
     * This method will set the engine mode to Hint_Calculating and ask the
     * EngineManager to perform a short search. The result is captured via
     * pollHintCalculation() in the update loop.
     */
    void requestHint();
    // Save/load latest match to disk
    /** @brief Save the latest match to `latest_match.txt` (player color + UCI move list). */
    void saveLatestGame();
    /** @brief Load a saved match from `latest_match.txt` and replay moves. */
    void loadLatestGame();
    // Cancel any active hint and stop hint searches
    void clearActiveHint();
    // Whether a hint can be requested right now (engine idle, match started, human to move)
    bool canRequestHint() const;
    // Get last computed hint move (UCI string like "e2e4") or empty if none
    std::string getHintMove() const;

    Board& getBoard();
    std::optional<std::pair<int,int>> getSelected() const;

    // Temporary move message to display in the UI
    //std::string getMoveMessage() const;
    //float getMessageTimer() const;

private:
    Board m_board;
    EngineManager m_engine;

    // Engine mode state machine to track whether engine is idle, thinking for AI move,
    // or calculating a hint requested by the user.
    enum class EngineMode { Idle, AI_Thinking, Hint_Calculating };
    EngineMode m_engineMode = EngineMode::Idle;

    // Last computed hint move (in engine UCI format like "e2e4")
    std::string m_hintMove = "";
    bool m_isReviewingHistory = false;
    std::optional<std::pair<int,int>> m_selected;

    // Cached evaluation history per ply (half-move). index 0 == initial position
    std::vector<float> m_evaluationHistory;
    size_t m_historyIndex = 0; // current displayed history index

    // Player/AI configuration
    int m_targetElo = 2000; // allowed: 500,1000,1500,2000
    PlayerColor m_playerColor = PlayerColor::White;
    int m_timePerMoveMs = 1000;
    int m_eloIndex = 3; // index into ELO_OPTIONS (default to 2000)
    bool m_gameStarted = false;
    bool m_matchEnded = false; // true when a match has reached checkmate/stalemate (review mode)

    // Private helpers extracted from update() to improve readability and single-responsibility
    void handleKeyboardShortcuts();
    void pollHintCalculation();
    void handleRestartKey();
    void handlePromotionClick();
    void handleBoardClick(const Vector2& mp);
    bool screenToBoardCoords(const Vector2& mp, int& outRow, int& outCol) const;
    void maybeTriggerAI();
    void pollEngineForBestMove();
    void applyEngineMove(const std::string& bestMove);
    void playMoveSound(Board::MoveResult res);
    void recordEvaluationForNewPosition();
    void checkTerminalStateAndReset();

public:
    // Accessors to allow Renderer/UI to read and toggle the player's chosen side
    PlayerColor getPlayerColor() const { return m_playerColor; }
    void setPlayerColor(PlayerColor c) { m_playerColor = c; }
    void togglePlayerColor() { m_playerColor = (m_playerColor == PlayerColor::White) ? PlayerColor::Black : PlayerColor::White; }
    // Engine state query for UI: true if engine is currently idle
    bool isEngineIdle() const { return m_engineMode == EngineMode::Idle; }
    // Post-match review accessor
    bool isMatchEnded() const { return m_matchEnded; }

private:
    // Encapsulated UI/audio/layout members kept private to avoid external mutation
    //std::string m_moveMessage;
    //float m_messageTimer = 0.0f;

    int m_windowWidth;
    int m_windowHeight;
    int m_tileSize;
    int m_sidebarWidth;
    Renderer* m_renderer = nullptr; // non-owning pointer

    // Sound effects
    Sound m_sndCapture;
    Sound m_sndCastle;
    Sound m_sndMoveCheck;
    Sound m_sndMoveSelf;
    Sound m_sndPromote;
    bool m_audioEnabled = false; // false if audio device or assets unavailable
};
