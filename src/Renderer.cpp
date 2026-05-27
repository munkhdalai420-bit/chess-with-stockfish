#include "Renderer.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <vector>
#include <sstream>
#include "GameController.h"
#include <cmath>

static std::vector<std::string> wrapText(const std::string &text, int maxWidth, int fontSize, const Font &font)
{
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string word;
    std::string cur;
    while (iss >> word)
    {
        std::string tryLine = cur.empty() ? word : (cur + " " + word);
        int w = (int)MeasureTextEx(font, tryLine.c_str(), (float)fontSize, 0).x;
        if (w <= maxWidth)
        {
            cur = tryLine;
        }
        else
        {
            if (!cur.empty()) lines.push_back(cur);
            // if single word is too long, force it on a line
            if ((int)MeasureTextEx(font, word.c_str(), (float)fontSize, 0).x > maxWidth)
            {
                // split the word (naive): push as is
                lines.push_back(word);
                cur.clear();
            }
            else
            {
                cur = word;
            }
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

Renderer::Renderer(int windowWidth, int windowHeight, int tileSize)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_tileSize(tileSize)
{
    m_boardPixelSize = m_tileSize * Board::Tiles;
    // Position board centered in the area left of the sidebar
    int availableWidth = m_windowWidth - m_sidebarWidth;
    if (availableWidth < m_boardPixelSize) availableWidth = m_boardPixelSize; // avoid negative
    m_boardOriginX = (availableWidth - m_boardPixelSize) / 2;
    m_boardOriginY = (m_windowHeight - m_boardPixelSize) / 2;

    // Load custom main font for UI rendering. Raster size 32 by default.
    m_mainFont = LoadFontEx("google_font.ttf", 32, NULL, 0);
    if (m_mainFont.texture.id == 0)
    {
        std::printf("Warning: failed to load 'resources/google_font.ttf', falling back to default font\n");
        m_mainFont = GetFontDefault();
    }

}

void Renderer::setTheme(BoardTheme theme)
{
    m_theme = theme;
    // keep index in sync
    switch (m_theme)
    {
        case BoardTheme::Grass: m_themeIndex = 0; break;
        case BoardTheme::Wood: m_themeIndex = 1; break;
        case BoardTheme::Ocean: m_themeIndex = 2; break;
        case BoardTheme::Classic: m_themeIndex = 3; break;
        case BoardTheme::Disco: m_themeIndex = 4; break;
        default: m_themeIndex = 2; break;
    }
}

void Renderer::cycleTheme()
{
    m_themeIndex = (m_themeIndex + 1) % 5;
    switch (m_themeIndex)
    {
        case 0: setTheme(BoardTheme::Grass); break;
        case 1: setTheme(BoardTheme::Wood); break;
        case 2: setTheme(BoardTheme::Ocean); break;
        case 3: setTheme(BoardTheme::Classic); break;
        case 4: setTheme(BoardTheme::Disco); break;
    }
}

int Renderer::tileLeft(int col) const
{
    return m_boardOriginX + col * m_tileSize; // no-op adjustment
}

int Renderer::tileTop(int row) const
{
    return m_boardOriginY + row * m_tileSize; // no-op adjustment
}

Rectangle Renderer::tileRect(int row, int col) const
{
    return Rectangle{ (float)tileLeft(col), (float)tileTop(row), (float)m_tileSize, (float)m_tileSize }; // no-op adjustment
}

Renderer::~Renderer()
{
    // Unload any loaded textures
    for (auto &kv : m_textures)
    {
        UnloadTexture(kv.second);
    }

    // Unload custom font (if it was loaded)
    if (m_mainFont.texture.id != 0)
    {
        // If it's the default font, UnloadFont is a no-op for that font in raylib,
        // but calling UnloadFont on a loaded custom font frees GPU resources.
        UnloadFont(m_mainFont);
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
    m_textures.clear();
    std::vector<std::string> missingFiles;
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
            std::string filename = textureFilenameForKey(key);

            // Load immediately from the resource directory (SearchAndSetResourceDir in main)
            Texture2D tex = LoadTexture(filename.c_str());
            if (tex.id == 0)
            {
                // Loading failed
                std::printf("Warning: failed to load texture '%s'\n", filename.c_str());
                missingFiles.push_back(filename);
                continue;
            }

            m_textures.emplace(key, tex);
        }
    }

    const size_t expectedCount = typeChars.size() * colors.size();
    if (m_textures.size() != expectedCount)
    {
        std::printf("Error: %zu texture(s) failed to load. Required piece textures are missing.\n", missingFiles.size());
        for (const auto &f : missingFiles) std::printf("  Missing: %s\n", f.c_str());
        return false;
    }

    return true;
}

void Renderer::render(Board& board, const std::optional<std::pair<int,int>>& selected, float evaluation, GameController& controller)
{
    // If Disco mode is active, we let drawBoard pick colors dynamically based on time.
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
                        int radius = (int)m_tileSize * 0.35f;
                        DrawCircleLines(cx, cy, radius, hintRing);
                    }
                    else
                    {
                        // Normal move hint: small filled circle
                        float radius = (float)m_tileSize * 0.12f;
                        DrawCircle(cx, cy, (int)radius, hintFill);
                    }
                }
            }
        }
    }

    drawPieces(board);

    // Evaluation bar to the left of the board: white fills from bottom, black from top.
    {
        const int BAR_W = 24;
        const int BAR_PAD = 8;
        int barH = m_boardPixelSize;
        int barX = m_boardOriginX - BAR_W - BAR_PAD;
        if (barX < 8) barX = 8; // keep inside window
        int barY = m_boardOriginY;

        // Determine filled fraction for White (0.0..1.0) using a sigmoid mapping
        float whiteFrac = 0.5f;
        if (controller.isMateDetected())
        {
            int mate = controller.getMateInMoves();
            if (mate > 0) whiteFrac = 1.0f; // white mates -> full white
            else if (mate < 0) whiteFrac = 0.0f; // black mates -> full black
        }
        else
        {
            // evaluation is in pawns (e.g., +1.5 == 150 centipawns)
            double eval = (double)evaluation;
            const double SCALE = 2.0; // controls sensitivity
            double x = eval / SCALE;
            double s = 1.0 / (1.0 + std::exp(-x));
            // make small advantages visible but avoid fully saturating
            if (s < 0.01) s = 0.01;
            if (s > 0.99) s = 0.99;
            whiteFrac = (float)s;
        }

        int whiteH = (int)std::round(whiteFrac * (double)barH);

        // Draw background (black) then white overlay for the bottom portion
        DrawRectangle(barX, barY, BAR_W, barH, BLACK);
        if (whiteH > 0)
        {
            DrawRectangle(barX, barY + (barH - whiteH), BAR_W, whiteH, WHITE);
        }
        // Border
        DrawRectangleLines(barX, barY, BAR_W, barH, Fade(WHITE, 0.12f));
    }
    // Draw move history sidebar on the right
    const int panelX = m_windowWidth - m_sidebarWidth;
    const int panelY = 0;
    const int panelW = m_sidebarWidth;
    const int panelH = m_windowHeight;

    // Panel background
    const Color panelBg = { 30, 30, 30, 220 };
    DrawRectangle(panelX, panelY, panelW, panelH, panelBg);

    const int titleFont = 24;
    const int bodyFont = 20; // monospaced not available, use custom font
    const int padding = 12;
    const Color textCol = { 220, 220, 220, 255 };

    // Draw status (White's Turn / Black's Turn) at the top of the sidebar
    const int STATUS_FONT_SIZE = 22;
    const Color statusColor = WHITE;
    Board::GameState gs = board.getGameState();
    std::string status;
    if (gs == Board::GameState::Active)
    {
        status = (board.getCurrentTurn() == PieceColor::White) ? "White's Turn" : "Black's Turn";
        if (board.isInCheck(board.getCurrentTurn())) status += " - CHECK!";
    }
    else if (gs == Board::GameState::Checkmate)
    {
        PieceColor winner = (board.getCurrentTurn() == PieceColor::White) ? PieceColor::Black : PieceColor::White;
        status = "CHECKMATE - ";
        status += (winner == PieceColor::White) ? "White Wins!" : "Black Wins!";
    }
    else
    {
        status = "Stalemate - Draw";
    }

    int sw = (int)MeasureTextEx(m_mainFont, status.c_str(), (float)STATUS_FONT_SIZE, 0.0f).x;
    int sx = panelX + (panelW - sw) / 2;
    int sy = panelY + padding;
    DrawTextEx(m_mainFont, status.c_str(), { (float)sx, (float)sy }, (float)STATUS_FONT_SIZE, 0.0f, statusColor);

    // Controls area directly under the status
    const float CTRL_Y = (float)(sy + STATUS_FONT_SIZE + 8);
    const float ctrlPad = 8.0f;
    const float ctrlW = (float)(panelW - padding * 2);
    const float ctrlH = 36.0f;
    const float ctrlX = (float)(panelX + padding);

    // Elo selection button (left) and primary buttons row
    Rectangle eloRect = { ctrlX, CTRL_Y, ctrlW, ctrlH };
    const int ELO_FONT = 20;

    if (!controller.isMatchStarted())
    {
        Vector2 mousePos = GetMousePosition();
        bool hoverElo = CheckCollisionPointRec(mousePos, eloRect);
        DrawRectangleRec(eloRect, hoverElo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f));
        std::string eloText = std::string("AI Difficulty: ") + std::to_string(controller.getTargetElo()) + " Elo";
        DrawTextEx(m_mainFont, eloText.c_str(), { eloRect.x + 8, eloRect.y + 6 }, (float)ELO_FONT, 0.0f, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverElo)
        {
            controller.cycleTargetElo();
        }

        // Buttons row: UNDO | REDO | START MATCH | END MATCH (start enabled, others disabled visually)
        const float ROW_Y = CTRL_Y + ctrlH + ctrlPad;
        const float SP = 8.0f;
        const float btnW = (ctrlW - SP * 3) / 4.0f;
        Rectangle undoRect = { ctrlX, ROW_Y, btnW, ctrlH };
        Rectangle redoRect = { ctrlX + (btnW + SP), ROW_Y, btnW, ctrlH };
        Rectangle startRect = { ctrlX + 2 * (btnW + SP), ROW_Y, btnW, ctrlH };
        Rectangle endRect = { ctrlX + 3 * (btnW + SP), ROW_Y, btnW, ctrlH };

        bool canUndo = controller.canUndo();
        bool canRedo = controller.canRedo();
        bool hoverUndo = canUndo && CheckCollisionPointRec(mousePos, undoRect);
        bool hoverRedo = canRedo && CheckCollisionPointRec(mousePos, redoRect);
        bool hoverStart = CheckCollisionPointRec(mousePos, startRect);
        bool hoverEnd = CheckCollisionPointRec(mousePos, endRect);

        DrawRectangleRec(undoRect, canUndo ? (hoverUndo ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.3f)) : Fade(GRAY, 0.15f));
        DrawTextEx(m_mainFont, "UNDO", { undoRect.x + 8, undoRect.y + 6 }, (float)ELO_FONT, 0.0f, canUndo ? Fade(WHITE, 0.6f) : Fade(WHITE, 0.3f));

        DrawRectangleRec(redoRect, canRedo ? (hoverRedo ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.3f)) : Fade(GRAY, 0.15f));
        DrawTextEx(m_mainFont, "REDO", { redoRect.x + 8, redoRect.y + 6 }, (float)ELO_FONT, 0.0f, canRedo ? Fade(WHITE, 0.6f) : Fade(WHITE, 0.3f));

        DrawRectangleRec(startRect, hoverStart ? Fade(RED, 0.9f) : RED);
        DrawTextEx(m_mainFont, "START", { startRect.x + 8, startRect.y + 6 }, (float)ELO_FONT, 0.0f, WHITE);

        DrawRectangleRec(endRect, hoverEnd ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.3f));
        DrawTextEx(m_mainFont, "END", { endRect.x + 8, endRect.y + 6 }, (float)ELO_FONT, 0.0f, Fade(WHITE, 0.6f));

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (hoverStart) controller.startMatch();
            else if (hoverUndo && canUndo) controller.undo();
            else if (hoverRedo && canRedo) controller.redo();
            else if (hoverEnd) controller.endMatch();
        }
    }
    else
    {
        // Match active: Elo disabled (faded)
        Vector2 mousePos = GetMousePosition();
        bool hoverElo = CheckCollisionPointRec(mousePos, eloRect);
        DrawRectangleRec(eloRect, hoverElo ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.4f));
        std::string eloText = std::string("AI Difficulty: ") + std::to_string(controller.getTargetElo()) + " Elo";
        DrawTextEx(m_mainFont, eloText.c_str(), { eloRect.x + 8, eloRect.y + 6 }, (float)ELO_FONT, 0.0f, WHITE);

        // Buttons row: UNDO | REDO | START MATCH | END MATCH
        const float ROW_Y = CTRL_Y + ctrlH + ctrlPad;
        const float SP = 8.0f;
        const float btnW = (ctrlW - SP * 3) / 4.0f;
        Rectangle undoRect = { ctrlX, ROW_Y, btnW, ctrlH };
        Rectangle redoRect = { ctrlX + (btnW + SP), ROW_Y, btnW, ctrlH };
        Rectangle startRect = { ctrlX + 2 * (btnW + SP), ROW_Y, btnW, ctrlH };
        Rectangle endRect = { ctrlX + 3 * (btnW + SP), ROW_Y, btnW, ctrlH };

        bool canUndo = controller.canUndo();
        bool canRedo = controller.canRedo();
        bool hoverUndo = canUndo && CheckCollisionPointRec(mousePos, undoRect);
        bool hoverRedo = canRedo && CheckCollisionPointRec(mousePos, redoRect);
        bool hoverStart = CheckCollisionPointRec(mousePos, startRect);
        bool hoverEnd = CheckCollisionPointRec(mousePos, endRect);

        DrawRectangleRec(undoRect, canUndo ? (hoverUndo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f)) : Fade(GRAY, 0.3f));
        DrawTextEx(m_mainFont, "UNDO", { undoRect.x + 8, undoRect.y + 6 }, (float)ELO_FONT, 0.0f, canUndo ? WHITE : Fade(WHITE, 0.6f));

        DrawRectangleRec(redoRect, canRedo ? (hoverRedo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f)) : Fade(GRAY, 0.3f));
        DrawTextEx(m_mainFont, "REDO", { redoRect.x + 8, redoRect.y + 6 }, (float)ELO_FONT, 0.0f, canRedo ? WHITE : Fade(WHITE, 0.6f));

        DrawRectangleRec(startRect, hoverStart ? Fade(GRAY, 0.5f) : Fade(GRAY, 0.3f));
        DrawTextEx(m_mainFont, "START", { startRect.x + 8, startRect.y + 6 }, (float)ELO_FONT, 0.0f, Fade(WHITE, 0.6f));

        DrawRectangleRec(endRect, hoverEnd ? Fade(RED, 0.9f) : RED);
        DrawTextEx(m_mainFont, "END", { endRect.x + 8, endRect.y + 6 }, (float)ELO_FONT, 0.0f, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (hoverUndo && canUndo) controller.undo();
            else if (hoverRedo && canRedo) controller.redo();
            else if (hoverStart) controller.startMatch();
            else if (hoverEnd) controller.endMatch();
        }
    }

    // Move History title and PGN listing under controls
    DrawTextEx(m_mainFont, "Move History", { (float)(panelX + padding), (float)(CTRL_Y + ctrlH * 2 + 2 * ctrlPad) }, (float)titleFont, 0.0f, textCol);

    // PGN text
    std::string pgn = board.getFullPGNText();
    int innerW = panelW - padding * 2;
    std::vector<std::string> lines = wrapText(pgn, innerW, bodyFont, m_mainFont);

    int titleH = (int)MeasureTextEx(m_mainFont, "Move History", (float)titleFont, 0.0f).y;
    int headerArea = (int)(CTRL_Y + ctrlH * 2 + 2 * ctrlPad + titleH + padding);
    int availableH = panelH - headerArea - (padding + 48); // leave space for theme controls at bottom
    int lineH = bodyFont + 6;
    int totalH = (int)lines.size() * lineH;

    int startY;
    if (totalH > availableH)
    {
        // pin to top of history area (allow scrolling later)
        startY = panelY + headerArea;
    }
    else
    {
        startY = panelY + headerArea;
    }

    // Draw lines (clip to the area above theme button)
    for (size_t i = 0; i < lines.size(); ++i)
    {
        int y = startY + (int)i * lineH;
        if (y + lineH > panelY + panelH - padding - 64) break; // clip before bottom controls
        DrawTextEx(m_mainFont, lines[i].c_str(), { (float)(panelX + padding), (float)y }, (float)bodyFont, 0.0f, textCol);
    }

    // Theme button at bottom of sidebar (moved from main.cpp)
    {
        const int panelX_int = panelX;
        const int padding_int = padding;
        const float boxW = 160.0f;
        const float boxH = 28.0f;
        float themeX = (float)(panelX_int + padding_int);
        float themeY = (float)(m_windowHeight - padding_int - (int)boxH - 8); // bottom of sidebar
        Rectangle themeRect = { themeX, themeY, boxW, boxH };
        Vector2 mp = GetMousePosition();
        bool hover = CheckCollisionPointRec(mp, themeRect);
        // theme button always interactable
        DrawRectangleRec(themeRect, hover ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.5f));
        const char* themeNames[5] = { "Grass", "Wood", "Ocean", "Classic", "Disco" };
        DrawTextEx(m_mainFont, themeNames[m_themeIndex], Vector2{ themeRect.x + 8, themeRect.y + 6 }, 18.0f, 0.0f, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hover)
        {
            cycleTheme();
        }
    }

    // Game over overlay (checkmate or stalemate)
    if (gs == Board::GameState::Checkmate || gs == Board::GameState::Stalemate)
    {
        // Full-screen translucent black
        DrawRectangle(0, 0, m_windowWidth, m_windowHeight, {0,0,0,160});

        // Centered message box
        const int BOX_FONT = 36;
        const int SUB_FONT = 20;
        int boxW = MeasureText(status.c_str(), BOX_FONT) + 40;
        int boxH = BOX_FONT + SUB_FONT + 36;
        int bx = (m_windowWidth - boxW) / 2;
        int by = (m_windowHeight - boxH) / 2;
        DrawRectangle(bx, by, boxW, boxH, Fade(BLACK, 0.6f));
        DrawTextEx(m_mainFont, status.c_str(), { (float)(bx + 20), (float)(by + 10) }, (float)BOX_FONT, 0.0f, WHITE);
        const char* sub = "Press R to Restart";
        int sw2 = (int)MeasureTextEx(m_mainFont, sub, (float)SUB_FONT, 0.0f).x;
        DrawTextEx(m_mainFont, sub, { (float)((m_windowWidth - sw2) / 2), (float)(by + 10 + BOX_FONT + 8) }, (float)SUB_FONT, 0.0f, WHITE);
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
        int startX = (m_windowWidth - totalW) / 2;
        int y = (m_windowHeight - ICON_SIZE) / 2;

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
    // Select colors based on the currently selected theme
    Color lightColor;
    Color darkColor;
    // If Disco mode, cycle through the four base themes over time
    BoardTheme themeToUse = m_theme;
    if (m_theme == BoardTheme::Disco)
    {
        double t = GetTime(); // seconds since InitWindow
        const double INTERVAL = 0.1; // seconds per theme
        int idx = ((int)(t / INTERVAL)) % 4;
        switch (idx)
        {
            case 0: themeToUse = BoardTheme::Grass; break;
            case 1: themeToUse = BoardTheme::Wood; break;
            case 2: themeToUse = BoardTheme::Ocean; break;
            case 3: themeToUse = BoardTheme::Classic; break;
        }
    }

    switch (themeToUse)
    {
        case BoardTheme::Ocean:
        {
            const Color l = { 222, 227, 230, 255 };
            const Color d = { 140, 162, 173, 255 };
            lightColor = l; darkColor = d;
            break;
        }
        case BoardTheme::Wood:
        {
            const Color l = { 240, 217, 181, 255 };
            const Color d = { 181, 136,  99, 255 };
            lightColor = l; darkColor = d;
            break;
        }
        case BoardTheme::Grass:
        {
            const Color l = { 255, 255, 221, 255 };
            const Color d = { 134, 166,  102, 255 };
            lightColor = l; darkColor = d;
            break;
        }
        case BoardTheme::Classic:
        default:
        {
            const Color l = { 240, 240, 240, 255 };
            const Color d = { 85, 85, 85, 255 };
            lightColor = l; darkColor = d;
            break;
        }
    }

    const Color highlightColor = {0, 255, 0, 100};
    const Color danger = { 255, 0, 0, 150 };


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
        }
    }

    // If current player is in check, highlight their king's tile.
    // This is intentionally done once per frame (not per-tile) for performance.
    if (board.isInCheck(board.getCurrentTurn()))
    {
        // Find king position (single scan)
        int kr = -1, kc = -1;
        for (int r = 0; r < Board::Tiles; ++r)
        {
            for (int c = 0; c < Board::Tiles; ++c)
            {
                const Piece* p = board.at(r, c);
                if (p && p->type() == PieceType::King && p->color() == board.getCurrentTurn())
                {
                    kr = r; kc = c;
                    break;
                }
            }
            if (kr != -1) break;
        }

        if (kr != -1)
        {
            DrawRectangle(tileLeft(kc), tileTop(kr), m_tileSize, m_tileSize, danger);
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
    const int SMALL_FONT = 18; // use smaller font
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
        DrawTextEx(m_mainFont, rankStr.c_str(), { (float)x, (float)y }, (float)SMALL_FONT, 0.0f, textColor);
    }

    // Files on bottom-most rank '1' (bottom-right corner of the tile)
    int bottomRow = Board::Tiles - 1; // row index 7
    for (int c = 0; c < Board::Tiles; ++c)
    {
        char fileChar = files[c];
        char fileStr[2] = { fileChar, '\0' };

        bool isLight = ((bottomRow + c) % 2) == 0;
        const Color textColor = isLight ? darkColor : lightColor;

        int textW = (int)MeasureTextEx(m_mainFont, fileStr, (float)SMALL_FONT, 0.0f).x;
        int x = tileLeft(c) + m_tileSize - textW - PADDING;
        int y = tileTop(bottomRow) + m_tileSize - SMALL_FONT - PADDING / 2;
        DrawTextEx(m_mainFont, fileStr, { (float)x, (float)y }, (float)SMALL_FONT, 0.0f, textColor);
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
