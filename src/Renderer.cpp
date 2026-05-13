#include "Renderer.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <vector>

Renderer::Renderer(int windowSize, int tileSize)
    : m_windowSize(windowSize), m_tileSize(tileSize)
{
    m_boardPixelSize = m_tileSize * Board::Tiles;
    m_boardOriginX = (m_windowSize - m_boardPixelSize) / 2;
    m_boardOriginY = (m_windowSize - m_boardPixelSize) / 2;
}

int Renderer::tileLeft(int col) const
{
    return m_boardOriginX + col * m_tileSize;
}

int Renderer::tileTop(int row) const
{
    return m_boardOriginY + row * m_tileSize;
}

Rectangle Renderer::tileRect(int row, int col) const
{
    return Rectangle{ (float)tileLeft(col), (float)tileTop(row), (float)m_tileSize, (float)m_tileSize };
}

Renderer::~Renderer()
{
    // Unload any loaded textures
    for (auto &kv : m_textures)
    {
        UnloadTexture(kv.second);
    }
}

std::string Renderer::textureFilenameForKey(const std::string& key) const
{
    // The project uses PNG assets only. Filenames follow the pattern
    // "Chess_<piece><color>t45.png" where <piece> is one of k,q,r,b,n,p and
    // <color> is 'l' for light (white) or 'd' for dark (black).
    // The main program sets the resource directory via
    // SearchAndSetResourceDir("resources"), so we return only the filename
    // and rely on raylib to load it from the current resource folder.
    if (key.size() < 2) return std::string();
    char type = key[0];
    char lightDark = key[1];

    std::string filename = "Chess_";
    filename.push_back(type);
    filename.push_back(lightDark);
    filename += "t45.png"; // PNG-only asset name
    return filename;
}

const Texture2D* Renderer::textureForKey(const std::string& key) const
{
    auto it = m_textures.find(key);
    if (it == m_textures.end()) return nullptr;
    return &it->second;
}

bool Renderer::loadTextures()
{
    // Iterate piece types and colors directly and load textures immediately.
    const std::vector<char> typeChars = {'k','q','r','b','n','p'}; // piece letters
    const std::vector<char> colors = {'l','d'}; // l = light (white), d = dark (black)

    for (char t : typeChars)
    {
        for (char c : colors)
        {
            // key used to store texture in m_textures map (e.g. "kl", "qd")
            std::string key;
            key.push_back(t);
            key.push_back(c);

            // Construct filename relative to the resource directory set by main.
            // Format: "Chess_<piece><color>t45.png" (e.g. "Chess_klt45.png").
            std::string filename = "Chess_";
            filename.push_back(t);
            filename.push_back(c);
            filename += "t45.png";

            // Load immediately from the resource directory (SearchAndSetResourceDir in main)
            Texture2D tex = LoadTexture(filename.c_str());
            if (tex.id == 0)
            {
                // Loading failed, print warning and skip storing this texture.
                std::printf("Warning: failed to load texture '%s'\n", filename.c_str());
                continue;
            }

            m_textures.emplace(key, tex);
        }
    }

    return true;
}

void Renderer::render(const Board& board, const std::optional<std::pair<int,int>>& selected) const
{
    drawBoard(board, selected);
    drawPieces(board);
    // Draw status text (turn, check, checkmate)
    // Keep this after pieces so it appears on top
    // Implemented below as inline here to keep file-local
    const int STATUS_FONT_SIZE = 22;
    const Color statusColor = WHITE;

    // Determine game state and text
    Board::GameState gs = board.getGameState();
    std::string status;
    if (gs == Board::GameState::Active)
    {
        status = (board.getCurrentTurn() == PieceColor::White) ? "White's Turn" : "Black's Turn";
        if (board.isInCheck(board.getCurrentTurn()))
        {
            status += " - CHECK!";
        }
    }
    else if (gs == Board::GameState::Checkmate)
    {
        // The current turn has no legal moves and is in check => they lost
        PieceColor winner = (board.getCurrentTurn() == PieceColor::White) ? PieceColor::Black : PieceColor::White;
        status = "CHECKMATE - ";
        status += (winner == PieceColor::White) ? "White Wins!" : "Black Wins!";
    }
    else // Stalemate
    {
        status = "Stalemate - Draw";
    }

    int sw = MeasureText(status.c_str(), STATUS_FONT_SIZE);
    int sx = (m_windowSize - sw) / 2;
    int sy = 8;
    DrawText(status.c_str(), sx, sy, STATUS_FONT_SIZE, statusColor);
}

void Renderer::drawBoard(const Board& board, const std::optional<std::pair<int,int>>& selected) const
{
    // Colors for the board
    const Color lightColor = {240, 217, 181, 255};
    const Color darkColor  = {181, 136,  99, 255};
    const Color highlightColor = {0, 255, 0, 100};

    // Draw tiles
    for (int r = 0; r < Board::Tiles; ++r)
    {
        for (int c = 0; c < Board::Tiles; ++c)
        {
            int x = tileLeft(c);
            int y = tileTop(r);
            bool isLight = ((r + c) % 2) == 0;
            DrawRectangle(x, y, m_tileSize, m_tileSize, isLight ? lightColor : darkColor);

            if (selected.has_value())
            {
                auto [sr, sc] = selected.value();
                if (sr == r && sc == c)
                {
                    DrawRectangle(x, y, m_tileSize, m_tileSize, highlightColor);
                }
            }

    // If current player is in check, highlight their king's tile
    if (board.isInCheck(board.getCurrentTurn()))
    {
        // Find king position
        int kr = -1, kc = -1;
        for (int r = 0; r < Board::Tiles; ++r)
        {
            for (int c = 0; c < Board::Tiles; ++c)
            {
                const Piece* p = board.at(r, c);
                if (p && p->type() == PieceType::King && p->color() == board.getCurrentTurn())
                {
                    kr = r; kc = c; break;
                }
            }
            if (kr != -1) break;
        }

        if (kr != -1)
        {
            Color danger = { 255, 0, 0, 100 };
            DrawRectangle(tileLeft(kc), tileTop(kr), m_tileSize, m_tileSize, danger);
        }
    }
        }
    }

    // Draw algebraic notation labels around the board (white perspective):
    // files: a..h along bottom (rank 1) and top (rank 8)
    // ranks: 1..8 along left and right edges
    const int LABEL_MARGIN = 18; // distance from board edge
    const int LABEL_FONT_SIZE = 20;
    const Color labelColor = WHITE;

    // Files (letters)
    const char files[] = "abcdefgh";
    for (int c = 0; c < Board::Tiles; ++c)
    {
        char fileChar = files[c];
        char fileStr[2] = { fileChar, '\0' };
        int textW = MeasureText(fileStr, LABEL_FONT_SIZE);

        // Top
        int topX = tileLeft(c) + (m_tileSize / 2) - (textW / 2);
        int topY = m_boardOriginY - LABEL_MARGIN - LABEL_FONT_SIZE;
        DrawText(fileStr, topX, topY, LABEL_FONT_SIZE, labelColor);

        // Bottom
        int bottomX = topX;
        int bottomY = m_boardOriginY + m_boardPixelSize + LABEL_MARGIN / 2;
        DrawText(fileStr, bottomX, bottomY, LABEL_FONT_SIZE, labelColor);
    }

    // Ranks (numbers)
    for (int r = 0; r < Board::Tiles; ++r)
    {
        int rank = Board::Tiles - r; // 8..1 from top to bottom
        std::string rankStr = std::to_string(rank);
        int textW = MeasureText(rankStr.c_str(), LABEL_FONT_SIZE);

        int y = tileTop(r) + (m_tileSize / 2) - (LABEL_FONT_SIZE / 2);

        // Left (right-aligned)
        int leftX = m_boardOriginX - LABEL_MARGIN - textW;
        DrawText(rankStr.c_str(), leftX, y, LABEL_FONT_SIZE, labelColor);

        // Right (left-aligned)
        int rightX = m_boardOriginX + m_boardPixelSize + LABEL_MARGIN / 2;
        DrawText(rankStr.c_str(), rightX, y, LABEL_FONT_SIZE, labelColor);
    }
}

void Renderer::drawPieces(const Board& board) const
{
    for (int r = 0; r < Board::Tiles; ++r)
    {
        for (int c = 0; c < Board::Tiles; ++c)
        {
            const Piece* p = board.at(r, c);
            if (!p) continue;

            std::string key = p->assetKey();
            const Texture2D* tex = textureForKey(key);
            if (!tex) continue; // texture not loaded

            // Maintain aspect ratio and scale the texture to fit inside tile
            float srcW = (float)tex->width;
            float srcH = (float)tex->height;
            float maxW = (float)m_tileSize * 0.85f; // slight padding
            float maxH = (float)m_tileSize * 0.85f;
            float scale = std::min(maxW / srcW, maxH / srcH);
            float drawW = srcW * scale;
            float drawH = srcH * scale;

            Rectangle srcRec = { 0.0f, 0.0f, srcW, srcH };
            float centerX = (float)(m_boardOriginX + c * m_tileSize + m_tileSize / 2);
            float centerY = (float)(m_boardOriginY + r * m_tileSize + m_tileSize / 2);
            Rectangle dstRec = { centerX - drawW / 2.0f, centerY - drawH / 2.0f, drawW, drawH };

            // No rotation, white tint
            DrawTexturePro(*tex, srcRec, dstRec, {0,0}, 0.0f, WHITE);
        }
    }
}
