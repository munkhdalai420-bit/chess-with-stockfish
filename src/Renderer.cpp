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

void Renderer::render(Board& board, const std::optional<std::pair<int,int>>& selected) const
{
    drawBoard(board, selected);

    // Move hint overlays: show legal destinations for the currently selected piece
    if (selected.has_value())
    {
        auto [sr, sc] = selected.value();
        const Piece* sel = board.at(sr, sc);
        if (sel && sel->color() == board.getCurrentTurn())
        {
            // Colors for hints
            const Color hintFill = { 0, 0, 0, 40 };
            const Color hintRing = { 0, 0, 0, 80 };

            for (int tr = 0; tr < Board::Tiles; ++tr)
            {
                for (int tc = 0; tc < Board::Tiles; ++tc)
                {
                    if (tr == sr && tc == sc) continue;

                    // Use a full, transactional legality check via Board to ensure
                    // moves that leave the king in check are excluded. Board will
                    // simulate and restore its state.
                    if (!board.wouldMoveBeLegal(sr, sc, tr, tc)) continue;

                    // Destination center
                    int cx = tileLeft(tc) + m_tileSize / 2;
                    int cy = tileTop(tr) + m_tileSize / 2;

                    const Piece* dest = board.at(tr, tc);
                    if (dest && dest->color() != sel->color())
                    {
                        // Capture hint: draw a larger ring
                        float radius = (float)m_tileSize * 0.35f;
                        DrawCircleLines(cx, cy, (int)radius, hintRing);
                    }
                    else
                    {
                        // Normal move hint: small filled circle
                        float radius = (float)m_tileSize * 0.12f;
                        DrawCircle(cx, cy, radius, hintFill);
                    }
                }
            }
        }
    }

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
    else if (gs == Board::GameState::DrawBy50Moves)
    {
        status = "DRAW BY 50-MOVE RULE";
    }
    else if (gs == Board::GameState::DrawByRepetition)
    {
        status = "DRAW BY THREEFOLD REPETITION";
    }
    else if (gs == Board::GameState::DrawByMaterial)
    {
        status = "DRAW BY INSUFFICIENT MATERIAL";
    }
    else // Stalemate
    {
        status = "Stalemate - Draw";
    }

    int sw = MeasureText(status.c_str(), STATUS_FONT_SIZE);
    int sx = (m_windowSize - sw) / 2;
    int sy = 8;
    DrawText(status.c_str(), sx, sy, STATUS_FONT_SIZE, statusColor);

    // Game over overlay (checkmate or stalemate)
    if (gs == Board::GameState::Checkmate || gs == Board::GameState::Stalemate ||
        gs == Board::GameState::DrawBy50Moves || gs == Board::GameState::DrawByRepetition || gs == Board::GameState::DrawByMaterial)
    {
        // Full-screen translucent black
        DrawRectangle(0, 0, m_windowSize, m_windowSize, {0,0,0,160});

        // Centered message box
        const int BOX_FONT = 36;
        const int SUB_FONT = 20;
        int boxW = MeasureText(status.c_str(), BOX_FONT) + 40;
        int boxH = BOX_FONT + SUB_FONT + 36;
        int bx = (m_windowSize - boxW) / 2;
        int by = (m_windowSize - boxH) / 2;
        DrawRectangle(bx, by, boxW, boxH, Fade(BLACK, 0.6f));
        DrawText(status.c_str(), bx + 20, by + 10, BOX_FONT, WHITE);
        const char* sub = "Press R to Restart";
        int sw2 = MeasureText(sub, SUB_FONT);
        DrawText(sub, (m_windowSize - sw2) / 2, by + 10 + BOX_FONT + 8, SUB_FONT, WHITE);
    }

    // Promotion overlay
    if (board.isAwaitingPromotion())
    {
        // Draw a centered row of 4 piece icons for the player's color
        auto pending = board.getPendingPromotionSquare();
        const Piece* p = board.at(pending.first, pending.second);
        PieceColor color = p ? p->color() : board.getCurrentTurn();

        const int ICON_SIZE = 64;
        const int PAD = 12;
        const int COUNT = 4;
        int totalW = COUNT * ICON_SIZE + (COUNT - 1) * PAD;
        int startX = (m_windowSize - totalW) / 2;
        int y = (m_windowSize - ICON_SIZE) / 2;

        // Background: solid, slightly translucent rectangle so the board doesn't distract
        const Color promoBg = { 0, 0, 0, 220 };
        Rectangle bgRect = { (float)(startX - 8), (float)(y - 8), (float)(totalW + 16), (float)(ICON_SIZE + 16) };
        DrawRectangleRec(bgRect, promoBg);

        const std::vector<char> pieces = {'q','r','b','n'};
        char colorChar = (color == PieceColor::White) ? 'l' : 'd';
        for (int i = 0; i < COUNT; ++i)
        {
            int x = startX + i * (ICON_SIZE + PAD);
            std::string key;
            key.push_back(pieces[i]);
            key.push_back(colorChar);
            const Texture2D* tex = textureForKey(key);
            if (!tex) continue;

            float srcW = (float)tex->width;
            float srcH = (float)tex->height;
            float scale = std::min((float)ICON_SIZE / srcW, (float)ICON_SIZE / srcH);
            float drawW = srcW * scale;
            float drawH = srcH * scale;
            Rectangle srcRec = {0,0,srcW,srcH};
            Rectangle dstRec = {(float)(x + ICON_SIZE/2 - drawW/2), (float)(y + ICON_SIZE/2 - drawH/2), drawW, drawH};
            DrawTexturePro(*tex, srcRec, dstRec, {0,0}, 0.0f, WHITE);
        }
    }
}

void Renderer::drawBoard(Board& board, const std::optional<std::pair<int,int>>& selected) const
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

    // Last move highlighting: draw behind labels and pieces
    auto last = board.getLastMove();
    if (last.has_value())
    {
        const auto &mv = last.value();
        const Color hl = { 255, 255, 0, 77 }; // ~0.3 alpha
        DrawRectangle(tileLeft(mv.c1), tileTop(mv.r1), m_tileSize, m_tileSize, hl);
        DrawRectangle(tileLeft(mv.c2), tileTop(mv.r2), m_tileSize, m_tileSize, hl);
    }

    // Draw algebraic notation labels only on the leftmost file (rank numbers)
    // and on the bottom rank (file letters). Labels are subtle and use an
    // inverted color relative to the tile for legibility.
    const int LABEL_FONT_SIZE = 20;
    const int SMALL_FONT = LABEL_FONT_SIZE / 2; // use smaller font
    const int PADDING = 6;

    const char files[] = "abcdefgh";

    // Ranks on leftmost file 'a' (top-left of the tile)
    for (int r = 0; r < Board::Tiles; ++r)
    {
        int rank = Board::Tiles - r; // 8..1 from top to bottom
        std::string rankStr = std::to_string(rank);

        // Determine tile color for inversion
        bool isLight = ((r + 0) % 2) == 0; // c == 0 (file a)
        const Color textColor = isLight ? darkColor : lightColor;

        int x = tileLeft(0) + PADDING;
        int y = tileTop(r) + PADDING;
        DrawText(rankStr.c_str(), x, y, SMALL_FONT, textColor);
    }

    // Files on bottom-most rank '1' (bottom-right corner of the tile)
    int bottomRow = Board::Tiles - 1; // row index 7
    for (int c = 0; c < Board::Tiles; ++c)
    {
        char fileChar = files[c];
        char fileStr[2] = { fileChar, '\0' };

        bool isLight = ((bottomRow + c) % 2) == 0;
        const Color textColor = isLight ? darkColor : lightColor;

        int textW = MeasureText(fileStr, SMALL_FONT);
        int x = tileLeft(c) + m_tileSize - textW - PADDING;
        int y = tileTop(bottomRow) + m_tileSize - SMALL_FONT - PADDING / 2;
        DrawText(fileStr, x, y, SMALL_FONT, textColor);
    }
}

void Renderer::drawPieces(Board& board) const
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
