#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "raylib.h"
#include "Board.h"

class Renderer
{
public:
    Renderer(int windowWidth, int windowHeight, int tileSize);
    ~Renderer();

    // Loads textures for all piece types/colors found in the assets folder.
    // If SVG files are present instead of PNGs, replace the extension in the implementation.
    bool loadTextures();

    // Render the board and pieces. 'selected' is an optional board coordinate {row, col}
    void render(Board& board, const std::optional<std::pair<int,int>>& selected) const;

private:
    int m_windowWidth;
    int m_windowHeight;
    int m_tileSize;
    int m_boardPixelSize;
    int m_boardOriginX;
    int m_boardOriginY;
    int m_sidebarWidth = 300;

    std::unordered_map<std::string, Texture2D> m_textures; // key like "k_l" => texture

    std::string textureFilenameForKey(const std::string& key) const;
    const Texture2D* textureForKey(const std::string& key) const;

    void drawBoard(Board& board, const std::optional<std::pair<int,int>>& selected) const;
    void drawPieces(Board& board) const;
    // Helpers
    int tileLeft(int col) const;
    int tileTop(int row) const;
    Rectangle tileRect(int row, int col) const;
};
