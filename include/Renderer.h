#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "raylib.h"
#include "Board.h"

class GameController;

class Renderer
{
public:
    enum class BoardTheme { Classic, Wood, Ocean, Grass, Disco };

    Renderer(int windowWidth, int windowHeight, int tileSize);
    ~Renderer();

    // Set visual theme for the board tiles
    void setTheme(BoardTheme theme);

    // Loads textures for all piece types/colors found in the assets folder.
    // If SVG files are present instead of PNGs, replace the extension in the implementation.
    bool loadTextures();

    // Render the board and pieces. 'selected' is an optional board coordinate {row, col}
    // 'evaluation' is a signed value (positive = White advantage) corresponding
    // to the currently displayed history index.
    void render(Board& board, const std::optional<std::pair<int,int>>& selected, float evaluation, GameController& controller);
    // Trigger a move animation (from tile X/Y to tile X/Y). X==col, Y==row
    void triggerMoveAnimation(int fromX, int fromY, int toX, int toY, char pieceChar, char pieceColor);

private:
    int m_windowWidth;
    int m_windowHeight;
    int m_tileSize;
    int m_boardPixelSize;
    int m_boardOriginX;
    int m_boardOriginY;
    int m_sidebarWidth = 300;

    std::unordered_map<std::string, Texture2D> m_textures; // key like "kl" => texture (e.g. "kl" == white king)
    Font m_mainFont; // custom font used for UI text (loaded at runtime)
    BoardTheme m_theme = BoardTheme::Ocean;
    int m_themeIndex = 2; // 0=Grass,1=Wood,2=Ocean,3=Classic (default Ocean)

    std::string textureFilenameForKey(const std::string& key) const;
    const Texture2D* textureForKey(const std::string& key) const;

    void drawBoard(Board& board, const std::optional<std::pair<int,int>>& selected) const;
    void drawPieces(Board& board);
    // Cycle the visual theme (used by UI)
    void cycleTheme();
    // Helpers
    int tileLeft(int col) const;
    int tileTop(int row) const;
    Rectangle tileRect(int row, int col) const;
    // Animation toggle and state
    struct PieceAnimation {
        bool isActive = false;
        int fromX = -1, fromY = -1; // Starting grid coordinates (col,row)
        int toX = -1, toY = -1;     // Ending grid coordinates (col,row)
        float progress = 0.0f;      // 0.0 .. 1.0
        char pieceChar = 0;         // e.g., 'p','k','q'
        char pieceColor = 0;        // 'l' or 'd'
    };

    bool m_animatePieces = true;    // toggle for animations
    PieceAnimation m_currentAnim;
    Rectangle m_animCheckboxBounds;
};
