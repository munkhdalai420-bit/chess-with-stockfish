#include "GameController.h"

#include <iostream>
#include <algorithm>

GameController::GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_tileSize(tileSize), m_sidebarWidth(sidebarWidth),
      m_sndCapture{}, m_sndCastle{}, m_sndMoveCheck{}, m_sndMoveSelf{}, m_sndPromote{}
{
    // Initialize audio device and load sound effects
    InitAudioDevice();

    m_sndCapture = LoadSound("capture.mp3");
    m_sndCastle = LoadSound("castle.mp3");
    m_sndMoveCheck = LoadSound("move-check.mp3");
    m_sndMoveSelf = LoadSound("move-self.mp3");
    m_sndPromote = LoadSound("promote.mp3");

    m_board.initializeStandardSetup();

    // Launch engine with default path (EngineManager will use its default if empty)
    m_engine.launch("");

    // Initialize evaluation history with current engine evaluation (may be 0.0 initially)
    m_evaluationHistory.clear();
    m_evaluationHistory.push_back(m_engine.getEvaluation());
    m_historyIndex = 0;

    // Game will be started via startMatch() when player selects difficulty in lobby
    m_gameStarted = false;
}

GameController::~GameController()
{
    // Shutdown engine first
    m_engine.shutdown();

    // Unload sounds
    UnloadSound(m_sndCapture);
    UnloadSound(m_sndCastle);
    UnloadSound(m_sndMoveCheck);
    UnloadSound(m_sndMoveSelf);
    UnloadSound(m_sndPromote);

    // Close audio device
    CloseAudioDevice();
}

void GameController::startMatch()
{
    // Configure engine with selected difficulty
    m_engine.setDifficulty(m_targetElo);

    // Reset board to starting position
    m_board = Board();
    m_board.initializeStandardSetup();

    // Reset selection and history
    m_selected.reset();
    m_isReviewingHistory = false;
    m_evaluationHistory.clear();
    m_evaluationHistory.push_back(m_engine.getEvaluation());
    m_historyIndex = 0;

    // Mark the game as started
    m_gameStarted = true;

    // If player is Black, AI should think as White immediately
    if (m_playerColor == PieceColor::Black)
    {
        m_aiIsThinking = true;
        m_engine.startSearch(m_board.getFEN(), m_timePerMoveMs);
    }
}

Board& GameController::getBoard()
{
    return m_board;
}
void GameController::undo()
{
    if (m_aiIsThinking) return;

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
    if (m_aiIsThinking) return;

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
    if (m_aiIsThinking) return;

    m_historyIndex = index;
    m_selected.reset();
    m_isReviewingHistory = true;
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

void GameController::update()
{
    // Undo/Redo keys (only when AI is not thinking)
    if (!m_aiIsThinking)
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

    // Also support restart via R key (for convenience)
    if (IsKeyPressed(KEY_R))
    {
        // Return to lobby (game not started)
        m_gameStarted = false;
        m_board = Board();
        m_board.initializeStandardSetup();
        m_selected.reset();
        m_aiIsThinking = false;
        m_isReviewingHistory = false;
        // Reset evaluation history
        m_evaluationHistory.clear();
        m_evaluationHistory.push_back(m_engine.getEvaluation());
        m_historyIndex = 0;
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
                        PlaySound(m_sndPromote);
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
    else if (m_board.getGameState() == Board::GameState::Active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m_gameStarted)
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
            if (m_board.getCurrentTurn() == PieceColor::White)
            {
                int col = (int)(localX / m_tileSize);
                int row = (int)(localY / m_tileSize);

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
                        Board::MoveResult res = m_board.movePiece(sr, sc, row, col);
                        if (res != Board::MoveResult::Invalid)
                        {
                            m_selected.reset();
                            m_isReviewingHistory = false;

                            if (res == Board::MoveResult::Check)
                            {
                                PlaySound(m_sndMoveCheck);
                            }
                            else if (res == Board::MoveResult::Castle)
                            {
                                PlaySound(m_sndCastle);
                            }
                            else if (res == Board::MoveResult::Capture)
                            {
                                PlaySound(m_sndCapture);
                            }
                            else if (res == Board::MoveResult::Promotion)
                            {
                                // wait for promotion choice
                            }
                            else
                            {
                                PlaySound(m_sndMoveSelf);
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

    // AI trigger
    if (m_board.getGameState() == Board::GameState::Active &&
        !m_aiIsThinking &&
        !m_isReviewingHistory &&
        m_board.getCurrentTurn() == PieceColor::Black)
    {
        m_aiIsThinking = true;
        std::string currentFen = m_board.getFEN();
        m_engine.startSearch(currentFen, 1000);
    }

    // AI polling
    if (m_aiIsThinking)
    {
        std::string bestMove;
        if (m_engine.checkBestMove(bestMove))
        {
            Board::ChessMove engineMove = m_board.parseEngineMove(bestMove);
            Board::MoveResult res = m_board.movePiece(engineMove.r1, engineMove.c1, engineMove.r2, engineMove.c2);

            if (res != Board::MoveResult::Invalid)
            {
                if (res == Board::MoveResult::Check)
                {
                    PlaySound(m_sndMoveCheck);
                }
                else if (res == Board::MoveResult::Castle)
                {
                    PlaySound(m_sndCastle);
                }
                else if (res == Board::MoveResult::Capture)
                {
                    PlaySound(m_sndCapture);
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
                        PlaySound(m_sndPromote);
                    }
                }
                else
                {
                    PlaySound(m_sndMoveSelf);
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

                m_aiIsThinking = false;
        }
    }

    // Decrement message timer
    if (m_messageTimer > 0.0f)
    {
        m_messageTimer -= GetFrameTime();
        if (m_messageTimer < 0.0f) m_messageTimer = 0.0f;
    }
}
