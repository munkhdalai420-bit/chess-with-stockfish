#include "GameController.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include "Renderer.h"

// Define Elo options
const int GameController::ELO_OPTIONS[GameController::ELO_COUNT] = { 500, 1000, 1500, 2000 };

GameController::GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth, Renderer* renderer)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_tileSize(tileSize), m_sidebarWidth(sidebarWidth),
      m_sndCapture{}, m_sndCastle{}, m_sndMoveCheck{}, m_sndMoveSelf{}, m_sndPromote{}, m_renderer(renderer)
{
    // Initialize audio device and load sound effects
    InitAudioDevice();

    // Only attempt to load sounds if audio device is available
    m_audioEnabled = IsAudioDeviceReady();
    if (m_audioEnabled)
    {
        m_sndCapture = LoadSound("capture.mp3");
        m_sndCastle = LoadSound("castle.mp3");
        m_sndMoveCheck = LoadSound("move-check.mp3");
        m_sndMoveSelf = LoadSound("move-self.mp3");
        m_sndPromote = LoadSound("promote.mp3");

        // Validate loaded sounds: raylib Sound contains a data pointer when valid
        bool anyFailed = false;
        if (m_sndCapture.frameCount == 0) { std::printf("Warning: failed to load 'capture.mp3'\n"); anyFailed = true; }
        if (m_sndCastle.frameCount == 0) { std::printf("Warning: failed to load 'castle.mp3'\n"); anyFailed = true; }
        if (m_sndMoveCheck.frameCount == 0) { std::printf("Warning: failed to load 'move-check.mp3'\n"); anyFailed = true; }
        if (m_sndMoveSelf.frameCount == 0) { std::printf("Warning: failed to load 'move-self.mp3'\n"); anyFailed = true; }
        if (m_sndPromote.frameCount == 0) { std::printf("Warning: failed to load 'promote.mp3'\n"); anyFailed = true; }

        if (anyFailed)
        {
            // Unload any that did load and disable audio playback to avoid calling PlaySound on invalid sounds
            if (m_sndCapture.frameCount) UnloadSound(m_sndCapture);
            if (m_sndCastle.frameCount) UnloadSound(m_sndCastle);
            if (m_sndMoveCheck.frameCount) UnloadSound(m_sndMoveCheck);
            if (m_sndMoveSelf.frameCount) UnloadSound(m_sndMoveSelf);
            if (m_sndPromote.frameCount) UnloadSound(m_sndPromote);
            m_audioEnabled = false;
            std::printf("Audio disabled due to missing sound assets or load failure.\n");
        }
    }
    else
    {
        std::printf("Audio device not available: audio disabled.\n");
    }

    m_board.initializeStandardSetup();

    // Launch engine with default path (EngineManager will use its default if empty)
    m_engine.launch("");

    // Configure engine difficulty
    m_engine.setDifficulty(m_targetElo);

    // Ensure elo index matches target elo
    for (int i = 0; i < GameController::ELO_COUNT; ++i)
    {
        if (ELO_OPTIONS[i] == m_targetElo) { m_eloIndex = i; break; }
    }



    // Initialize evaluation history with current engine evaluation (may be 0.0 initially)
    m_evaluationHistory.clear();
    m_evaluationHistory.push_back(m_engine.getEvaluation());
    m_historyIndex = 0;

}
void GameController::endMatch()
{
    // Abort any running engine search and return to lobby state
    if (m_engineMode == EngineMode::AI_Thinking)
    {
        m_engine.stopSearch();
        m_engineMode = EngineMode::Idle;
    }
    // Exit match mode
    m_gameStarted = false;

    // Reset board and UI state similar to pressing R (return to initial position)
    m_board = Board();
    m_board.initializeStandardSetup();
    m_selected.reset();

    // Reconfigure engine difficulty to current selection and reset evaluation history
    m_engine.setDifficulty(m_targetElo);
    m_engine.clearMateDetection();
    m_evaluationHistory.clear();
    // Reset displayed evaluation to neutral (half/half)
    m_evaluationHistory.push_back(0.0f);
    m_historyIndex = 0;
}
GameController::~GameController()
{
    // Shutdown engine first
    m_engine.shutdown();

    // Unload sounds and close audio device if audio was enabled
    if (m_audioEnabled)
    {
        UnloadSound(m_sndCapture);
        UnloadSound(m_sndCastle);
        UnloadSound(m_sndMoveCheck);
        UnloadSound(m_sndMoveSelf);
        UnloadSound(m_sndPromote);

        // Close audio device
        CloseAudioDevice();
    }
}

Board& GameController::getBoard()
{
    return m_board;
}

int GameController::getTargetElo() const { return m_targetElo; }

void GameController::cycleTargetElo()
{
    m_eloIndex = (m_eloIndex + 1) % ELO_COUNT;
    m_targetElo = ELO_OPTIONS[m_eloIndex];
    m_engine.setDifficulty(m_targetElo);
}

bool GameController::isMatchStarted() const { return m_gameStarted; }

bool GameController::isMateDetected() const { return m_engine.isMateDetected(); }

int GameController::getMateInMoves() const { return m_engine.getMateInMoves(); }

bool GameController::canUndo() const
{
    if (m_engineMode == EngineMode::AI_Thinking) return false;
    return m_historyIndex > 0;
}

bool GameController::canRedo() const
{
    if (m_engineMode == EngineMode::AI_Thinking) return false;
    return (m_historyIndex + 1) < m_evaluationHistory.size();
}

void GameController::startMatch()
{
    // Initialize a fresh board and evaluation history
    m_board = Board();
    m_board.initializeStandardSetup();
    m_selected.reset();
    m_evaluationHistory.clear();
    // Configure engine difficulty for this match
    m_engine.setDifficulty(m_targetElo);
    m_engine.clearMateDetection();
    // Start with a neutral evaluation so the bar shows half/half at match start
    m_evaluationHistory.push_back(0.0f);
    m_historyIndex = 0;
    m_gameStarted = true;

    // If the human chooses to play Black, have the AI start thinking for White immediately
    if (m_playerColor == PlayerColor::Black)
    {
        m_engineMode = EngineMode::AI_Thinking;
        m_engine.startSearch(m_board.getFEN(), m_timePerMoveMs);
    }
}
void GameController::undo()
{
    // Clear any pending hint when user changes board state
    clearActiveHint();
    if (m_engineMode == EngineMode::AI_Thinking) return;

    m_board.undoMove();
    if (m_historyIndex > 0) --m_historyIndex;
    m_selected.reset();
    m_isReviewingHistory = true;
    std::cout << "FEN: " << m_board.getFEN() << std::endl;
    std::cout << "PGN: " << m_board.getFullPGNText() << std::endl;
    std::cout << "-------------------" << std::endl;
}

void GameController::redo()
{
    // Clear any pending hint when user changes board state
    clearActiveHint();
    if (m_engineMode == EngineMode::AI_Thinking) return;

    m_board.redoMove();
    if (m_historyIndex + 1 < m_evaluationHistory.size()) ++m_historyIndex;
    m_selected.reset();
    m_isReviewingHistory = true;
    std::cout << "FEN: " << m_board.getFEN() << std::endl;
    std::cout << "PGN: " << m_board.getFullPGNText() << std::endl;
    std::cout << "-------------------" << std::endl;
}

void GameController::goToHistoryIndex(size_t index)
{
    // Clear any pending hint when user changes board state
    clearActiveHint();

    // Do nothing while the engine is thinking
    if (m_engineMode == EngineMode::AI_Thinking) return;

    // Enter review mode so the engine stays paused while we jump around history
    m_isReviewingHistory = true;

    // Clamp target to valid range
    if (m_evaluationHistory.empty())
    {
        m_selected.reset();
        return;
    }

    size_t target = index;
    if (target >= m_evaluationHistory.size()) target = m_evaluationHistory.size() - 1;

    // If current index is greater, undo until we reach target
    while (m_historyIndex > target)
    {
        // undo() will decrement m_historyIndex when possible
        undo();
        // Safety: if undo() didn't change m_historyIndex (shouldn't happen), break to avoid infinite loop
        if (m_historyIndex > 0 && m_historyIndex <= target) break;
        if (m_historyIndex == 0 && target == 0) break;
    }

    // If current index is smaller, redo until we reach target
    while (m_historyIndex < target)
    {
        // redo() will increment m_historyIndex when possible
        redo();
        // Safety: if redo() didn't change m_historyIndex, break
        if (m_historyIndex >= target) break;
    }

    // Clear any selection and leave review mode enabled
    m_selected.reset();
}

float GameController::getDisplayedEvaluation() const
{
    if (m_historyIndex < m_evaluationHistory.size()) return m_evaluationHistory[m_historyIndex];
    return 0.0f;
}

std::optional<std::pair<int,int>> GameController::getSelected() const
{
    return m_selected;
}

std::string GameController::getMoveMessage() const { return m_moveMessage; }
float GameController::getMessageTimer() const { return m_messageTimer; }

void GameController::requestHint()
{
    // Only allow a hint when engine is idle, a match is running, and it's the human's turn
    if (m_engineMode != EngineMode::Idle) return;
    if (!m_gameStarted) return;
    // Map PlayerColor to PieceColor for comparison with board state
    {
        PieceColor humanPieceColor = (m_playerColor == PlayerColor::White) ? PieceColor::White : PieceColor::Black;
        if (m_board.getCurrentTurn() != humanPieceColor) return; // it's AI's turn
    }

    m_engineMode = EngineMode::Hint_Calculating;
    m_hintMove.clear();
    // Short, fixed-time hint search (ms)
    constexpr int HINT_TIME_MS = 400;
    m_engine.startSearch(m_board.getFEN(), HINT_TIME_MS);
}

void GameController::clearActiveHint()
{
    m_hintMove.clear();
    if (m_engineMode == EngineMode::Hint_Calculating)
    {
        // Abort the ongoing hint search and reset mode
        m_engine.stopSearch();
        m_engineMode = EngineMode::Idle;
    }
}

bool GameController::canRequestHint() const
{
    PieceColor humanPieceColor = (m_playerColor == PlayerColor::White) ? PieceColor::White : PieceColor::Black;
    return (m_engineMode == EngineMode::Idle) && m_gameStarted && (m_board.getCurrentTurn() == humanPieceColor);
}

std::string GameController::getHintMove() const
{
    return m_hintMove;
}

void GameController::update()
{
    // Undo/Redo keys (only when AI is not thinking)
    if (m_engineMode != EngineMode::AI_Thinking)
    {
        if (IsKeyPressed(KEY_LEFT))
        {
            undo();
        }
        if (IsKeyPressed(KEY_RIGHT))
        {
            redo();
        }
    }

    // If a hint calculation was requested, poll engine for bestmove and capture it
    if (m_engineMode == EngineMode::Hint_Calculating)
    {
        std::string best;
        if (m_engine.checkBestMove(best))
        {
            // Store hint and return engine to idle
            m_hintMove = best;
            m_engineMode = EngineMode::Idle;
        }
    }

    // Also support restart via R key (for convenience) -> start a new match
    if (IsKeyPressed(KEY_R))
    {
        startMatch();
    }

    // Promotion handling
    if (m_board.isAwaitingPromotion())
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mp = GetMousePosition();
            const int ICON_SIZE = 64;
            const int PAD = 12;
            const int COUNT = 4;
            int totalW = COUNT * ICON_SIZE + (COUNT - 1) * PAD;
            int startX = (m_windowWidth - totalW) / 2;
            int y = (m_windowHeight - ICON_SIZE) / 2;

            for (int i = 0; i < COUNT; ++i)
            {
                int x = startX + i * (ICON_SIZE + PAD);
                Rectangle rect = { (float)x, (float)y, (float)ICON_SIZE, (float)ICON_SIZE };
                if (CheckCollisionPointRec(mp, rect))
                {
                    PieceType choice = PieceType::Queen;
                    switch (i)
                    {
                    case 0: choice = PieceType::Queen; break;
                    case 1: choice = PieceType::Rook; break;
                    case 2: choice = PieceType::Bishop; break;
                    case 3: choice = PieceType::Knight; break;
                    }
                    Board::MoveResult pres = m_board.completePromotion(choice);
                        if (pres == Board::MoveResult::Promotion)
                    {
                        if (m_audioEnabled) PlaySound(m_sndPromote);
                        auto last = m_board.getLastMove();
                        if (last.has_value())
                        {
                            std::string san = m_board.moveToSAN(last.value());
                            m_board.setLastMoveSAN(san);
                            std::cout << "Move played: " << san << std::endl;
                        }
                        std::cout << "FEN: " << m_board.getFEN() << std::endl;
                        std::cout << "PGN: " << m_board.getFullPGNText() << std::endl;
                        std::cout << "-------------------" << std::endl;
                    }
                    m_selected.reset();
                    break;
                }
            }
        }
    }
    else if (m_gameStarted && m_board.getGameState() == Board::GameState::Active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mp = GetMousePosition();
        int boardPixelSize = m_tileSize * Board::Tiles;
        int availableWidth = m_windowWidth - m_sidebarWidth;
        int originX = (availableWidth - boardPixelSize) / 2;
        int originY = (m_windowHeight - boardPixelSize) / 2;

        float localX = mp.x - originX;
        float localY = mp.y - originY;

        if (localX >= 0 && localY >= 0 && localX < boardPixelSize && localY < boardPixelSize)
        {
            // Only allow human input when it's the human's turn (map PlayerColor -> PieceColor)
            PieceColor humanPieceColor = (m_playerColor == PlayerColor::White) ? PieceColor::White : PieceColor::Black;
            if (m_board.getCurrentTurn() == humanPieceColor)
            {
                int col = (int)(localX / m_tileSize);
                int row = (int)(localY / m_tileSize);
                // If the human chose Black, flip coordinates to match rotated board view
                if (m_playerColor == PlayerColor::Black)
                {
                    col = Board::Tiles - 1 - col;
                    row = Board::Tiles - 1 - row;
                }

                if (!m_selected.has_value())
                {
                    if (m_board.at(row, col) != nullptr)
                        m_selected = std::make_pair(row, col);
                }
                else
                {
                    auto [sr, sc] = m_selected.value();
                    if (sr == row && sc == col)
                    {
                        m_selected.reset();
                    }
                    else
                    {
                        // Trigger animation (renderer expects X=col, Y=row) before applying the move
                        const Piece* moving = m_board.at(sr, sc);
                        if (moving && m_renderer)
                        {
                            std::string ak = moving->assetKey();
                            if (ak.size() >= 2)
                            {
                                // Detect castling: king moves two columns horizontally
                                bool isCastle = false;
                                int secFromX = -1, secToX = -1;
                                char secPieceChar = 'r';
                                if (moving->type() == PieceType::King && std::abs(col - sc) == 2)
                                {
                                    isCastle = true;
                                    if (col > sc)
                                    {
                                        // King-side castle: rook moves from the h-file to f-file
                                        secFromX = sc + 3; // typically 7
                                        secToX = sc + 1;   // typically 5
                                    }
                                    else
                                    {
                                        // Queen-side castle: rook moves from the a-file to d-file
                                        secFromX = sc - 4; // typically 0
                                        secToX = sc - 1;   // typically 3
                                    }
                                }

                                m_renderer->triggerMoveAnimation(sc, sr, col, row, ak[0], ak[1], isCastle, secFromX, secToX, secPieceChar);
                            }
                        }

                        Board::MoveResult res = m_board.movePiece(sr, sc, row, col);
                        if (res != Board::MoveResult::Invalid)
                        {
                            m_selected.reset();
                            // Clear any active hint immediately when a human move is executed
                            clearActiveHint();
                            m_isReviewingHistory = false;

                            if (res == Board::MoveResult::Check)
                            {
                                if (m_audioEnabled) PlaySound(m_sndMoveCheck);
                            }
                            else if (res == Board::MoveResult::Castle)
                            {
                                if (m_audioEnabled) PlaySound(m_sndCastle);
                            }
                            else if (res == Board::MoveResult::Capture)
                            {
                                if (m_audioEnabled) PlaySound(m_sndCapture);
                            }
                            else if (res == Board::MoveResult::Promotion)
                            {
                                // wait for promotion choice
                            }
                            else
                            {
                                if (m_audioEnabled) PlaySound(m_sndMoveSelf);
                            }

                            if (res != Board::MoveResult::Promotion)
                            {
                                auto last = m_board.getLastMove();
                                if (last.has_value())
                                {
                                    std::string san = m_board.moveToSAN(last.value());
                                    m_board.setLastMoveSAN(san);
                                }
                                std::cout << "FEN: " << m_board.getFEN() << std::endl;
                                std::cout << "PGN: " << m_board.getFullPGNText() << std::endl;
                                std::cout << "-------------------" << std::endl;
                                // Record evaluation for this new position
                                float eval = m_engine.getEvaluation();
                                // Truncate any redo history if we were viewing past moves
                                if (m_historyIndex + 1 < m_evaluationHistory.size())
                                {
                                    m_evaluationHistory.resize(m_historyIndex + 1);
                                }
                                m_evaluationHistory.push_back(eval);
                                m_historyIndex = m_evaluationHistory.size() - 1;
                            }
                        }
                        else
                        {
                            m_moveMessage = m_board.getLastMoveError();
                            if (m_moveMessage.empty()) m_moveMessage = "Illegal move";
                            m_messageTimer = 2.5f;
                            std::printf("Move failed: %s\n", m_moveMessage.c_str());

                            if (m_board.at(row, col) != nullptr)
                                m_selected = std::make_pair(row, col);
                            else
                                m_selected.reset();
                        }
                    }
                }
            }
        }
        else
        {
            m_selected.reset();
        }
    }

    // AI trigger: only consider starting AI when a match is running and the engine is idle.
    // Strict guard prevents AI from being triggered by stray state changes in the lobby.
    if (!m_gameStarted || m_engineMode != EngineMode::Idle)
    {
        // Do nothing: AI must not be started when no game is active or engine already busy
    }
    else
    {
        // Existing logic: start engine search for AI move when it's the opponent's turn
        if (m_board.getGameState() == Board::GameState::Active &&
            !m_isReviewingHistory &&
            // If it's not the human's piece color, let the engine think
            (m_board.getCurrentTurn() != ((m_playerColor == PlayerColor::White) ? PieceColor::White : PieceColor::Black)))
        {
            m_engineMode = EngineMode::AI_Thinking;
            std::string currentFen = m_board.getFEN();
            m_engine.startSearch(currentFen, 1000);
        }
    }

    // AI polling: when engine is thinking for an AI move, check for bestmove
    if (m_engineMode == EngineMode::AI_Thinking)
    {
        std::string bestMove;
        if (m_engine.checkBestMove(bestMove))
        {
            Board::ChessMove engineMove = m_board.parseEngineMove(bestMove);
            // Trigger animation (renderer expects X=col, Y=row) before applying the move
            const Piece* moving = m_board.at(engineMove.r1, engineMove.c1);
            if (moving && m_renderer)
            {
                std::string ak = moving->assetKey();
                if (ak.size() >= 2)
                    m_renderer->triggerMoveAnimation(engineMove.c1, engineMove.r1, engineMove.c2, engineMove.r2, ak[0], ak[1]);
            }
            Board::MoveResult res = m_board.movePiece(engineMove.r1, engineMove.c1, engineMove.r2, engineMove.c2);

            if (res != Board::MoveResult::Invalid)
            {
                if (res == Board::MoveResult::Check)
                {
                    if (m_audioEnabled) PlaySound(m_sndMoveCheck);
                }
                else if (res == Board::MoveResult::Castle)
                {
                    if (m_audioEnabled) PlaySound(m_sndCastle);
                }
                else if (res == Board::MoveResult::Capture)
                {
                    if (m_audioEnabled) PlaySound(m_sndCapture);
                }
                else if (res == Board::MoveResult::Promotion)
                {
                    if (bestMove.length() == 5)
                    {
                        char promotionChar = bestMove[4];
                        PieceType chosenType = PieceType::Queen;
                        switch (promotionChar)
                        {
                        case 'q': chosenType = PieceType::Queen; break;
                        case 'r': chosenType = PieceType::Rook; break;
                        case 'b': chosenType = PieceType::Bishop; break;
                        case 'n': chosenType = PieceType::Knight; break;
                        }
                        m_board.completePromotion(chosenType);
                        if (m_audioEnabled) PlaySound(m_sndPromote);
                    }
                }
                else
                {
                    if (m_audioEnabled) PlaySound(m_sndMoveSelf);
                }

                if (res != Board::MoveResult::Promotion)
                {
                    auto last = m_board.getLastMove();
                    if (last.has_value())
                    {
                        std::string san = m_board.moveToSAN(last.value());
                        m_board.setLastMoveSAN(san);
                        std::cout << "AI Move: " << san << std::endl;
                    }
                }
                else
                {
                    auto last = m_board.getLastMove();
                    if (last.has_value())
                    {
                        std::string san = m_board.moveToSAN(last.value());
                        m_board.setLastMoveSAN(san);
                        std::cout << "AI Move (Promotion): " << san << std::endl;
                    }
                }

                std::cout << "FEN: " << m_board.getFEN() << std::endl;
                std::cout << "PGN: " << m_board.getFullPGNText() << std::endl;
                std::cout << "-------------------" << std::endl;
            }

                // Record evaluation after AI move
                float eval = m_engine.getEvaluation();
                if (m_historyIndex + 1 < m_evaluationHistory.size())
                {
                    m_evaluationHistory.resize(m_historyIndex + 1);
                }
                m_evaluationHistory.push_back(eval);
                m_historyIndex = m_evaluationHistory.size() - 1;

                m_engineMode = EngineMode::Idle;
                std::cout << "Evaluation of move: " << eval << std::endl;
        }
    }

    // Decrement message timer
    if (m_messageTimer > 0.0f)
    {
        m_messageTimer -= GetFrameTime();
        if (m_messageTimer < 0.0f) m_messageTimer = 0.0f;
    }

    // If the game has reached a terminal state, exit match mode so lobby is available again
    {
        Board::GameState gs = m_board.getGameState();
        if (gs == Board::GameState::Checkmate || gs == Board::GameState::Stalemate)
        {
            m_gameStarted = false;
            if (m_engineMode == EngineMode::AI_Thinking)
            {
                m_engine.stopSearch();
                m_engineMode = EngineMode::Idle;
            }
            // Reset mate detection and evaluation history so evaluation bar is neutral
            m_engine.clearMateDetection();
            m_evaluationHistory.clear();
            m_evaluationHistory.push_back(0.0f);
            m_historyIndex = 0;
        }
    }
}
