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
    if (!SearchAndSetResourceDir("resources"))
    {
        std::cerr << "Error: resource directory 'resources' not found. Aborting.\n";
        CloseWindow();
        return 1;
    }

    Renderer renderer(WINDOW_WIDTH, WINDOW_HEIGHT, TILE_SIZE);
    if (!renderer.loadTextures())
    {
        std::cerr << "Error: failed to load required textures/fonts. Aborting.\n";
        CloseWindow();
        return 1;
    }
    renderer.setTheme(Renderer::BoardTheme::Grass);

    // Simple theme index for cycling UI (0=Classic,1=Wood,2=Ocean,3=Grass)
    int themeIndex = 0;

    // Create game controller which owns the Board, EngineManager, and audio
    GameController controller(WINDOW_WIDTH, WINDOW_HEIGHT, TILE_SIZE, SIDEBAR_WIDTH, &renderer);

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

    // Theme button moved to Renderer::render - main.cpp keeps main loop minimal

        EndDrawing();
    }

    // Controller destructor will shut down the engine and audio when it goes out of scope

    CloseWindow();
    return 0;
}
