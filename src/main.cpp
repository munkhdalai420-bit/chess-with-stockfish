#include <memory>
#include <optional>
#include <string>

#include "raylib.h"
#include "resource_dir.h" // utility to find assets folder

#include "Board.h"
#include "Renderer.h"

// Window and board constants
static constexpr int WINDOW_SIZE = 800;
static constexpr int TILE_SIZE = 80; // 8 * 80 = 640 board, centered in 800 window

int main()
{
    // Tell the window to use vsync and work on high DPI displays
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

    InitWindow(WINDOW_SIZE, WINDOW_SIZE, "Chess - Rendering Foundation");

    // Set working directory to assets folder if available
    SearchAndSetResourceDir("resources");

    Board board;
    board.initializeStandardSetup();

    Renderer renderer(WINDOW_SIZE, TILE_SIZE);
    renderer.loadTextures();

    std::optional<std::pair<int,int>> selected;
    std::string moveMessage;
    float messageTimer = 0.0f;

    while (!WindowShouldClose())
    {
        // Undo/Redo keys
        if (IsKeyPressed(KEY_U))
        {
            board.undoMove();
            selected.reset();
        }
        if (IsKeyPressed(KEY_R))
        {
            board.redoMove();
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
                int startX = (WINDOW_SIZE - totalW) / 2;
                int y = (WINDOW_SIZE - ICON_SIZE) / 2;

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
                        board.completePromotion(choice);
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
            int originX = (WINDOW_SIZE - boardPixelSize) / 2;
            int originY = (WINDOW_SIZE - boardPixelSize) / 2;

            float localX = mp.x - originX;
            float localY = mp.y - originY;

            if (localX >= 0 && localY >= 0 && localX < boardPixelSize && localY < boardPixelSize)
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
                        bool moved = board.movePiece(sr, sc, row, col);
                        if (moved)
                        {
                            selected.reset();
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
            else
            {
                selected.reset();
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

    CloseWindow();
    return 0;
}
