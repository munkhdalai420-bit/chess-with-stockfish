#include <memory>
#include <optional>
#include <string>
#include <iostream>

#include "raylib.h"
#include "resource_dir.h" // utility to find assets folder

#include "Board.h"
#include "Renderer.h"
#include "EngineManager.h"

// Window and board constants
static constexpr int WINDOW_WIDTH = 1100; // extra space for sidebar
static constexpr int WINDOW_HEIGHT = 800;
static constexpr int TILE_SIZE = 80; // 8 * 80 = 640 board
static constexpr int SIDEBAR_WIDTH = 300;

int main()
{
    // Tell the window to use vsync and work on high DPI displays
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Chess - Rendering Foundation");

    // Set working directory to assets folder if available
    SearchAndSetResourceDir("resources");

    // Initialize audio
    InitAudioDevice();

    // Load sounds
    Sound sndCapture = LoadSound("capture.mp3");
    Sound sndCastle = LoadSound("castle.mp3");
    Sound sndMoveCheck = LoadSound("move-check.mp3");
    Sound sndMoveSelf = LoadSound("move-self.mp3");
    Sound sndPromote = LoadSound("promote.mp3");

    Board board;
    // Run self-tests for FEN round-trip then initialize the standard setup
    //board.runFENTests();
    board.initializeStandardSetup();

    Renderer renderer(WINDOW_WIDTH, WINDOW_HEIGHT, TILE_SIZE);
    renderer.loadTextures();

    // Initialize EngineManager and launch Stockfish
    EngineManager engine;
    engine.launch("D:/Downloads/raylib-quickstart-main2026/raylib-quickstart-main/engines/stockfish.exe");

    std::optional<std::pair<int,int>> selected;
    std::string moveMessage;
    float messageTimer = 0.0f;
    bool aiIsThinking = false;

    while (!WindowShouldClose())
    {
        // Undo/Redo keys
        if (IsKeyPressed(KEY_LEFT))
        {
            board.undoMove();
            selected.reset();
            std::cout << "FEN: " << board.getFEN() << std::endl;
            std::cout << "PGN: " << board.getFullPGNText() << std::endl;
            std::cout << "-------------------" << std::endl;
        }
        if (IsKeyPressed(KEY_RIGHT))
        {
            board.redoMove();
            selected.reset();
            std::cout << "FEN: " << board.getFEN() << std::endl;
            std::cout << "PGN: " << board.getFullPGNText() << std::endl;
            std::cout << "-------------------" << std::endl;
        }

        // Restart
        if (IsKeyPressed(KEY_R))
        {
            board = Board();
            board.initializeStandardSetup();
            selected.reset();
        }

        // Promotion selection handling: if awaiting promotion, capture clicks for promotion choice
        if (board.isAwaitingPromotion())
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                Vector2 mp = GetMousePosition();
                const int ICON_SIZE = 64;
                const int PAD = 12;
                const int COUNT = 4;
                int totalW = COUNT * ICON_SIZE + (COUNT - 1) * PAD;
                int startX = (WINDOW_WIDTH - totalW) / 2;
                int y = (WINDOW_HEIGHT - ICON_SIZE) / 2;

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
                        Board::MoveResult pres = board.completePromotion(choice);
                        if (pres == Board::MoveResult::Promotion)
                        {
                            PlaySound(sndPromote);
                            // Record SAN for the completed promotion move and print history
                            auto last = board.getLastMove();
                            if (last.has_value())
                            {
                                std::string san = board.moveToSAN(last.value());
                                board.setLastMoveSAN(san);
                                std::cout << "Move played: " << san << std::endl;
                            }
                            std::cout << "FEN: " << board.getFEN() << std::endl;
                            std::cout << "PGN: " << board.getFullPGNText() << std::endl;
                            std::cout << "-------------------" << std::endl;
                        }
                        selected.reset();
                        break;
                    }
                }
            }
        }
        else if (board.getGameState() == Board::GameState::Active && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mp = GetMousePosition();
                int boardPixelSize = TILE_SIZE * Board::Tiles;
                int availableWidth = WINDOW_WIDTH - SIDEBAR_WIDTH;
                int originX = (availableWidth - boardPixelSize) / 2;
                int originY = (WINDOW_HEIGHT - boardPixelSize) / 2;

            float localX = mp.x - originX;
            float localY = mp.y - originY;

			if (localX >= 0 && localY >= 0 && localX < boardPixelSize && localY < boardPixelSize)
			{
				// Only allow board interaction if it's White's turn
				if (board.getCurrentTurn() == PieceColor::White)
				{
					int col = (int)(localX / TILE_SIZE);
					int row = (int)(localY / TILE_SIZE);

					if (!selected.has_value())
					{
						// First click: select piece if present
						if (board.at(row, col) != nullptr)
							selected = std::make_pair(row, col);
					}
					else
					{
						auto [sr, sc] = selected.value();
						// If clicking the same square, deselect
						if (sr == row && sc == col)
						{
							selected.reset();
						}
						else
						{
							// Attempt move from selected -> clicked
							Board::MoveResult res = board.movePiece(sr, sc, row, col);
							if (res != Board::MoveResult::Invalid)
							{
								selected.reset();

								// Play sounds for non-promotion moves immediately
								if (res == Board::MoveResult::Check)
								{
									PlaySound(sndMoveCheck);
								}
								else if (res == Board::MoveResult::Castle)
								{
									PlaySound(sndCastle);
								}
								else if (res == Board::MoveResult::Capture)
								{
									PlaySound(sndCapture);
								}
								else if (res == Board::MoveResult::Promotion)
								{
									// Promotion pending; wait for user to confirm choice and play promote then
								}
								else
								{
									PlaySound(sndMoveSelf);
								}
								// Record SAN for this move (except promotions; those are recorded on completion)
								if (res != Board::MoveResult::Promotion)
								{
									auto last = board.getLastMove();
									if (last.has_value())
									{
										std::string san = board.moveToSAN(last.value());
										board.setLastMoveSAN(san);
										//std::cout << "Move played: " << san << std::endl;
									}
									std::cout << "FEN: " << board.getFEN() << std::endl;
									std::cout << "PGN: " << board.getFullPGNText() << std::endl;
									std::cout << "-------------------" << std::endl;
								}
							}
							else
							{
								// Show move error temporarily
								moveMessage = board.getLastMoveError();
								if (moveMessage.empty()) moveMessage = "Illegal move";
								messageTimer = 2.5f;
								std::printf("Move failed: %s\n", moveMessage.c_str());

								// If move failed but clicked another piece, change selection
								if (board.at(row, col) != nullptr)
									selected = std::make_pair(row, col);
								else
									selected.reset();
							}
						}
					}
				}
			}
            else
            {
                selected.reset();
            }
        }

        // Phase A (Trigger): Check if it's Black's turn and AI is not already thinking
        if (board.getGameState() == Board::GameState::Active && 
            board.getCurrentTurn() == PieceColor::Black && 
            !aiIsThinking)
        {
            aiIsThinking = true;
            std::string currentFen = board.getFEN();
            engine.startSearch(currentFen, 1000);
        }

        // Phase B (Polling): Check if the AI has finished thinking
        if (aiIsThinking)
        {
            std::string bestMove;
            if (engine.checkBestMove(bestMove))
            {
                // AI has produced a move
                Board::ChessMove engineMove = board.parseEngineMove(bestMove);
                Board::MoveResult res = board.movePiece(engineMove.r1, engineMove.c1, engineMove.r2, engineMove.c2);

                if (res != Board::MoveResult::Invalid)
                {
                    // Move was successful
                    if (res == Board::MoveResult::Check)
                    {
                        PlaySound(sndMoveCheck);
                    }
                    else if (res == Board::MoveResult::Castle)
                    {
                        PlaySound(sndCastle);
                    }
                    else if (res == Board::MoveResult::Capture)
                    {
                        PlaySound(sndCapture);
                    }
                    else if (res == Board::MoveResult::Promotion)
                    {
                        // Handle promotion: check if the move string is 5 chars (includes promotion piece)
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
                            board.completePromotion(chosenType);
                            PlaySound(sndPromote);
                        }
                    }
                    else
                    {
                        PlaySound(sndMoveSelf);
                    }

                    // Record SAN for this move
                    if (res != Board::MoveResult::Promotion)
                    {
                        auto last = board.getLastMove();
                        if (last.has_value())
                        {
                            std::string san = board.moveToSAN(last.value());
                            board.setLastMoveSAN(san);
                            std::cout << "AI Move: " << san << std::endl;
                        }
                    }
                    else
                    {
                        auto last = board.getLastMove();
                        if (last.has_value())
                        {
                            std::string san = board.moveToSAN(last.value());
                            board.setLastMoveSAN(san);
                            std::cout << "AI Move (Promotion): " << san << std::endl;
                        }
                    }

                    std::cout << "FEN: " << board.getFEN() << std::endl;
                    std::cout << "PGN: " << board.getFullPGNText() << std::endl;
                    std::cout << "-------------------" << std::endl;
                }

                aiIsThinking = false;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        renderer.render(board, selected);

        // Draw temporary move message if present
        if (messageTimer > 0.0f)
        {
            const int MSG_FONT = 18;
            DrawText(moveMessage.c_str(), 10, 40, MSG_FONT, RED);
            messageTimer -= GetFrameTime();
        }

        EndDrawing();
    }
    // Unload sounds and shut down audio
    UnloadSound(sndCapture);
    UnloadSound(sndCastle);
    UnloadSound(sndMoveCheck);
    UnloadSound(sndMoveSelf);
    UnloadSound(sndPromote);
    CloseAudioDevice();

    // Shutdown the engine and clean up resources
    engine.shutdown();

    CloseWindow();
    return 0;
}
