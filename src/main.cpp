#include <memory>
#include <optional>
#include <string>
#include <iostream>

#include "raylib.h"
#include "resource_dir.h" // utility to find assets folder

#include "Board.h"
#include "Renderer.h"
#include "EngineManager.h"
#include "GameController.h"

// Window and board constants
static constexpr int WINDOW_WIDTH = 1100; // extra space for sidebar
static constexpr int WINDOW_HEIGHT = 800;
static constexpr int TILE_SIZE = 80; // 8 * 80 = 640 board
static constexpr int SIDEBAR_WIDTH = 300;

int main()
{
    // Tell the window to use vsync, work on high DPI displays, and request 4x MSAA
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Chess - Rendering Foundation");

    // Set working directory to assets folder if available
    SearchAndSetResourceDir("resources");

    Renderer renderer(WINDOW_WIDTH, WINDOW_HEIGHT, TILE_SIZE);
    renderer.loadTextures();
    renderer.setTheme(Renderer::BoardTheme::Grass);

    // Simple theme index for cycling UI (0=Classic,1=Wood,2=Ocean,3=Grass)
    int themeIndex = 0;

    // Create game controller which owns the Board, EngineManager, and audio
    GameController controller(WINDOW_WIDTH, WINDOW_HEIGHT, TILE_SIZE, SIDEBAR_WIDTH);

    while (!WindowShouldClose())
    {
        // Let the controller handle inputs, AI, and game updates
        controller.update();

        BeginDrawing();
        ClearBackground(BLACK);

        renderer.render(controller.getBoard(), controller.getSelected(), controller.getDisplayedEvaluation(), controller);

        // Draw temporary move message if present (controller manages the timer)
        if (controller.getMessageTimer() > 0.0f)
        {
            const int MSG_FONT = 18;
            DrawText(controller.getMoveMessage().c_str(), 10, 40, MSG_FONT, RED);
        }

        // Simple dropdown-like UI at the bottom of the sidebar to cycle board themes
        const int panelX = WINDOW_WIDTH - SIDEBAR_WIDTH;
        const int padding = 12;
        const float boxW = 160.0f;
        const float boxH = 28.0f;
        float themeX = (float)(panelX + padding);
        float themeY = (float)(WINDOW_HEIGHT - padding - (int)boxH - 8); // bottom of sidebar
        Rectangle themeRect = { themeX, themeY, boxW, boxH };
        DrawRectangleRec(themeRect, Fade(GRAY, 0.6f));
        const char* themeNames[4] = { "Grass", "Wood", "Ocean", "Classic" };
        DrawText(themeNames[themeIndex], (int)themeRect.x + 8, (int)themeRect.y + 6, 18, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mp = GetMousePosition();
            if (CheckCollisionPointRec(mp, themeRect))
            {
                themeIndex = (themeIndex + 1) % 4;
                switch (themeIndex)
                {
                    case 0: renderer.setTheme(Renderer::BoardTheme::Grass); break;
                    case 1: renderer.setTheme(Renderer::BoardTheme::Wood); break;
                    case 2: renderer.setTheme(Renderer::BoardTheme::Ocean); break;
                    case 3: renderer.setTheme(Renderer::BoardTheme::Classic); break;
                }
            }
        }

        EndDrawing();
    }

    // Controller destructor will shut down the engine and audio when it goes out of scope

    CloseWindow();
    return 0;
}
