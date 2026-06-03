#include "Renderer.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <vector>
#include <sstream>
#include "GameController.h"
#include <cmath>
#include "raymath.h"

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
void Renderer::triggerMoveAnimation(int fromX, int fromY, int toX, int toY, char pieceChar, char pieceColor,
                                    bool hasSecondary, int secFromX, int secToX, char secPieceChar)
{
    if (!m_animatePieces) return;
    m_currentAnim.isActive = true;
    m_currentAnim.fromX = fromX;
    m_currentAnim.fromY = fromY;
    m_currentAnim.toX = toX;
    m_currentAnim.toY = toY;
    m_currentAnim.progress = 0.0f;
    m_currentAnim.pieceChar = pieceChar;
    m_currentAnim.pieceColor = pieceColor;

    // Secondary piece (rook) for castling
    m_currentAnim.hasSecondaryPiece = hasSecondary;
    m_currentAnim.secFromX = secFromX;
    m_currentAnim.secToX = secToX;
    m_currentAnim.secPieceChar = secPieceChar;
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
        default: m_themeIndex = 1; break;
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
    int screenCol = m_flip ? (Board::Tiles - 1 - col) : col;
    return m_boardOriginX + screenCol * m_tileSize;
}

int Renderer::tileTop(int row) const
{
    int screenRow = m_flip ? (Board::Tiles - 1 - row) : row;
    return m_boardOriginY + screenRow * m_tileSize;
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

    std::string filename = "piece_";
    filename.push_back(type);
    filename.push_back(lightDark);
    filename += ".png"; // PNG-only asset name
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

void Renderer::render(const Board& board, const std::optional<std::pair<int,int>>& selected, float evaluation, GameController& controller)
{
    // Update flip state from controller (true when human chose Black)
    m_flip = (controller.getPlayerColor() == GameController::PlayerColor::Black);

    // Update animation progress (if active)
    const float ANIM_DURATION = 0.14f; // seconds for a single piece move
    if (m_currentAnim.isActive)
    {
        if (!m_animatePieces)
        {
            m_currentAnim.isActive = false;
        }
        else
        {
            // Verify last move matches the animation target; if not, cancel animation to avoid desync
            auto last = board.getLastMove();
            if (!last.has_value() || last->r1 != m_currentAnim.fromY || last->c1 != m_currentAnim.fromX ||
                last->r2 != m_currentAnim.toY || last->c2 != m_currentAnim.toX)
            {
                // If the board state changed (undo/redo), cancel the animation
                m_currentAnim.isActive = false;
            }
            // Note: progress is advanced during the piece-drawing stage so we can use
            // Vector2Lerp and draw the moving sprite on top of the static board.
        }
    }

    // If Disco mode is active, we let drawBoard pick colors dynamically based on time.
    drawBoard(board, selected, controller);

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

        // Draw evaluation bar. When not flipped, white fills from bottom; when flipped, black fills from bottom.
        if (!m_flip)
        {
            // Standard: background black, white fills from bottom up
            DrawRectangle(barX, barY, BAR_W, barH, BLACK);
            if (whiteH > 0)
            {
                DrawRectangle(barX, barY + (barH - whiteH), BAR_W, whiteH, WHITE);
            }
        }
        else
        {
            // Flipped perspective: background white, black fills from bottom up
            int blackH = barH - whiteH;
            DrawRectangle(barX, barY, BAR_W, barH, WHITE);
            if (blackH > 0)
            {
                DrawRectangle(barX, barY + (barH - blackH), BAR_W, blackH, BLACK);
            }
        }
        // Border
        DrawRectangleLines(barX, barY, BAR_W, barH, WHITE);
    }
    // Draw move history sidebar on the right
    const int panelX = m_windowWidth - m_sidebarWidth;
    const int panelY = 0;
    const int panelW = m_sidebarWidth;
    const int panelH = m_windowHeight;

    // Panel background (rounded)
    const Color panelBg = { 30, 30, 30, 220 };
    Rectangle panelRec = { (float)panelX, (float)panelY, (float)panelW, (float)panelH };
    DrawRectangleRec(panelRec, panelBg);

    const int titleFont = 24;
    const int bodyFont = 20; // monospaced not available, use custom font
    const int padding = 12;
    const Color textCol = { 220, 220, 220, 255 };

    // Draw status (White's Turn / Black's Turn) at the top of the sidebar
    const int STATUS_FONT_SIZE = 24;
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
    const float ctrlPad = 15.0f;
    const float ctrlW = (float)(panelW - padding * 2);
    const float ctrlH = 36.0f;
    const float ctrlX = (float)(panelX + padding);

    // Elo selection button (left) and Side toggle button (right) share the top control row
    const int ELO_FONT = 20;

    // Unified controls area: Elo selector occupies left half, Side toggle occupies right half.
    {
        Vector2 mousePos = GetMousePosition();
        bool matchStarted = controller.isMatchStarted();

        const float midGap = 8.0f;
        float btnW = (ctrlW - midGap) * 0.5f;
        Rectangle eloRect = { ctrlX, CTRL_Y, btnW, ctrlH };
        Rectangle sideRect = { ctrlX + btnW + midGap, CTRL_Y, btnW, ctrlH };

        // Elo selector: allow changing only when no match is running
        bool canChangeDifficulty = !controller.isMatchStarted();
        if (!canChangeDifficulty)
        {
            DrawRectangleRounded(eloRect, 0.2f, 6, Fade(GRAY, 0.4f));
            std::string eloText = std::string("ELO:") + std::to_string(controller.getTargetElo()) + " Elo";
            DrawTextEx(m_mainFont, eloText.c_str(), { eloRect.x + 8, eloRect.y + 10 }, (float)ELO_FONT, 0.0f, Fade(WHITE, 0.6f));
        }
        else
        {
            bool hoverElo = CheckCollisionPointRec(mousePos, eloRect);
            DrawRectangleRounded(eloRect, 0.2f, 6, hoverElo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f));
            std::string eloText = std::string("ELO:") + std::to_string(controller.getTargetElo()) + " Elo";
            DrawTextEx(m_mainFont, eloText.c_str(), { eloRect.x + 8, eloRect.y + 10 }, (float)ELO_FONT, 0.0f, WHITE);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverElo)
            {
                controller.cycleTargetElo();
            }
        }

        // Side toggle button: shows chosen side. Interactive only pre-game and when engine is idle.
        GameController::PlayerColor pc = controller.getPlayerColor();
        const char* sideLabel = (pc == GameController::PlayerColor::White) ? "Side: White" : "Side: Black";
        bool canSwitch = (!controller.isMatchStarted() && controller.isEngineIdle());

        // Draw faded when switching is not allowed; avoid any mouse checks when disabled so it's physically unclickable
        if (!canSwitch)
        {
            DrawRectangleRounded(sideRect, 0.2f, 6, Fade(GRAY, 0.3f));
            DrawTextEx(m_mainFont, sideLabel, { sideRect.x + 8, sideRect.y + 10 }, (float)ELO_FONT, 0.0f, Fade(WHITE, 0.6f));
        }
        else
        {
            bool hoverSide = CheckCollisionPointRec(mousePos, sideRect);
            DrawRectangleRounded(sideRect, 0.2f, 6, hoverSide ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f));
            DrawTextEx(m_mainFont, sideLabel, { sideRect.x + 8, sideRect.y + 10 }, (float)ELO_FONT, 0.0f, WHITE);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverSide)
            {
                controller.togglePlayerColor();
            }
        }

        // Single horizontal control row with 4 buttons: Undo | Start/End | Redo | Hint
        const float buttonHeight = 40.0f;
        const float pad = 10.0f;
        const float gap = 8.0f;
        const float iconBtnWidth = buttonHeight;
        const float sidebarWf = (float)panelW;
        const float middleBtnWidth = sidebarWf - (2.0f * pad) - (3.0f * iconBtnWidth) - (3.0f * gap);
        const float rowY = CTRL_Y + ctrlH + ctrlPad;

        float bx = (float)panelX + pad;
        Rectangle undoRect = { bx, rowY, iconBtnWidth, buttonHeight };
        Rectangle startRect = { undoRect.x + iconBtnWidth + gap, rowY, middleBtnWidth, buttonHeight };
        Rectangle redoRect = { startRect.x + middleBtnWidth + gap, rowY, iconBtnWidth, buttonHeight };
        Rectangle hintRect = { redoRect.x + iconBtnWidth + gap, rowY, iconBtnWidth, buttonHeight };

        bool enabledGlobal = controller.isMatchStarted();
        bool canUndoBtn = enabledGlobal && controller.canUndo();
        bool canRedoBtn = enabledGlobal && controller.canRedo();
        bool canHintBtn = controller.canRequestHint();

        Vector2 mp = GetMousePosition();
        bool hoverUndo = canUndoBtn && CheckCollisionPointRec(mp, undoRect);
        bool hoverStart = CheckCollisionPointRec(mp, startRect);
        bool hoverRedo = canRedoBtn && CheckCollisionPointRec(mp, redoRect);
        bool hoverHint = canHintBtn && CheckCollisionPointRec(mp, hintRect);

        // Draw buttons (rounded)
        DrawRectangleRounded(undoRect, 0.6f, 6, canUndoBtn ? (hoverUndo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f)) : Fade(GRAY, 0.2f));
        DrawRectangleRounded(startRect, 0.6f, 6, hoverStart ? Fade(RED, 0.9f) : RED);
        DrawRectangleRounded(redoRect, 0.6f, 6, canRedoBtn ? (hoverRedo ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f)) : Fade(GRAY, 0.2f));
        DrawRectangleRounded(hintRect, 0.6f, 6, canHintBtn ? (hoverHint ? Fade(GRAY, 0.8f) : Fade(GRAY, 0.6f)) : Fade(GRAY, 0.2f));

        // Labels
        const char* startLabel = matchStarted ? " END " : "START";
        const int BTN_FONT = 22;
        // Undo label "<"
        DrawTextEx(m_mainFont, "<", { undoRect.x + undoRect.width/2 - MeasureTextEx(m_mainFont, "<", (float)BTN_FONT, 0).x/2, undoRect.y + (undoRect.height - BTN_FONT)/2 }, (float)BTN_FONT, 0.0f, canUndoBtn ? WHITE : Fade(WHITE, 0.4f));
        // Start/End label
        DrawTextEx(m_mainFont, startLabel, { startRect.x + 38, startRect.y + (startRect.height - BTN_FONT)/2 + 2 }, (float)BTN_FONT, 0.0f, WHITE);
        // Redo label ">"
        DrawTextEx(m_mainFont, ">", { redoRect.x + redoRect.width/2 - MeasureTextEx(m_mainFont, ">", (float)BTN_FONT, 0).x/2, redoRect.y + (redoRect.height - BTN_FONT)/2 }, (float)BTN_FONT, 0.0f, canRedoBtn ? WHITE : Fade(WHITE, 0.4f));

        // Hint icon label: try emoji, fallback to yellow circle
        const char* hintLabel = "?";
        float hintTextW = MeasureTextEx(m_mainFont, hintLabel, 20.0f, 0).x;
        if (hintTextW > 0.0f)
        {
            DrawTextEx(m_mainFont, hintLabel, { hintRect.x + hintRect.width/2 - hintTextW/2, hintRect.y + (hintRect.height - 20.0f)/2 }, 20.0f, 0.0f, canHintBtn ? WHITE : Fade(WHITE, 0.4f));
        }
        else
        {
            // draw a small yellow circle centered
            Vector2 center = { hintRect.x + hintRect.width/2, hintRect.y + hintRect.height/2 };
            DrawCircleV(center, hintRect.width*0.2f, Color{ 255, 215, 0, (unsigned char)(canHintBtn ? 255 : 100) });
        }

        // Click handling for the four buttons
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (hoverUndo && canUndoBtn) controller.undo();
            else if (hoverStart)
            {
                if (matchStarted) controller.endMatch(); else controller.startMatch();
            }
            else if (hoverRedo && canRedoBtn) controller.redo();
            else if (hoverHint && canHintBtn) controller.requestHint();
        }
    }

    // Move History title and dedicated scrollable list under controls
    DrawTextEx(m_mainFont, "Move History", { (float)(panelX + padding), (float)(CTRL_Y + ctrlH * 2 + 2 * ctrlPad) }, (float)titleFont, 0.0f, textCol);

    // Prepare history box geometry
    std::string pgn = board.getFullPGNText();
    int innerW = panelW - padding * 2;
    int titleH = (int)MeasureTextEx(m_mainFont, "Move History", (float)titleFont, 0.0f).y;
    int historyX = panelX + padding;
    int historyY = (int)(CTRL_Y + ctrlH * 2 + 2 * ctrlPad + titleH + padding);
    int historyW = innerW;
    int historyH = panelH - historyY - (padding + 48); // leave space for theme controls at bottom
    Rectangle historyBox = { (float)historyX, (float)historyY, (float)historyW, (float)historyH };

    // Draw background for history area (subtle, rounded)
    DrawRectangleRec(historyBox, Fade(BLACK, 0.25f));

    // Build move-pair lines from PGN (e.g. "1. e4 e5", "2. Nf3 Nc6")
    std::vector<std::string> moveLines;
    std::vector<bool> hasBlack; // parallel array: does this line include a black move?
    {
        std::istringstream iss(pgn);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);

        for (size_t i = 0; i < tokens.size(); )
        {
            // Expect a move number token like "1." or "12."
            if (tokens[i].find('.') != std::string::npos)
            {
                std::string num = tokens[i];
                std::string white = "";
                std::string black = "";
                if (i + 1 < tokens.size()) white = tokens[i+1];
                if (i + 2 < tokens.size() && tokens[i+2].find('.') == std::string::npos) black = tokens[i+2];

                std::string line = num;
                if (!white.empty()) line += " " + white;
                if (!black.empty()) line += " " + black;
                moveLines.push_back(line);
                hasBlack.push_back(!black.empty());

                // Advance: consume move number + white (+ black)
                i += 1;
                if (!white.empty()) i += 1;
                if (!black.empty()) i += 1;
            }
            else
            {
                // Malformed token sequence: bail by treating remaining tokens as single lines
                std::string rest;
                for (; i < tokens.size(); ++i)
                {
                    if (!rest.empty()) rest += " ";
                    rest += tokens[i];
                }
                if (!rest.empty()) { moveLines.push_back(rest); hasBlack.push_back(false); }
                break;
            }
        }
    }

    int lineH = bodyFont + 6;
    int totalH = (int)moveLines.size() * lineH;

    // Mouse wheel scrolling when pointer over history box
    Vector2 mp_hist = GetMousePosition();
    if (CheckCollisionPointRec(mp_hist, historyBox))
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            m_historyScrollOffset -= wheel * 24.0f; // 24px per scroll step
        }
    }

    // Clamp scroll offset between 0 and max (so user can't scroll into empty space)
    if (m_historyScrollOffset < 0.0f) m_historyScrollOffset = 0.0f;
    if (totalH > historyH)
    {
        float maxOff = (float)(totalH - historyH);
        if (m_historyScrollOffset > maxOff) m_historyScrollOffset = maxOff;
    }
    else
    {
        m_historyScrollOffset = 0.0f;
    }

    // Clip drawing to historyBox and draw lines with offset; support clicking to jump to history index
    BeginScissorMode(historyX, historyY, historyW, historyH);
    for (int idx = 0; idx < (int)moveLines.size(); ++idx)
    {
        float itemY = (float)historyY + (float)idx * lineH - m_historyScrollOffset;
        DrawTextEx(m_mainFont, moveLines[idx].c_str(), { (float)historyX, itemY }, (float)bodyFont, 0.0f, textCol);

        // Click handling: require a double-click to jump to history
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mp = GetMousePosition();
            Rectangle itemRect = { (float)historyX, itemY, (float)historyW - 12.0f, (float)lineH };
            if (CheckCollisionPointRec(mp, itemRect))
            {
                // Determine target history index (ply). For line idx (0-based):
                // white ply = idx*2 + 1, black ply = idx*2 + 2 (if present)
                size_t targetPly = idx * 2 + 1;
                if (hasBlack[idx]) targetPly = idx * 2 + 2;

                double currentTime = GetTime();
                double dt = currentTime - m_lastHistoryClickTime;
                if (dt < 0.25 && idx == m_lastHistoryClickedIndex)
                {
                    // Double-click detected: navigate to this history ply
                    controller.goToHistoryIndex(targetPly);
                }

                // Update tracking (use this click as the last click)
                m_lastHistoryClickTime = currentTime;
                m_lastHistoryClickedIndex = idx;
            }
        }
    }
    EndScissorMode();

    // Simple scrollbar to the right of the history box when content exceeds height
    if (totalH > historyH)
    {
        int sbX = historyX + historyW - 8;
        int sbY = historyY;
        int sbW = 6;
        int sbH = historyH;
        Rectangle sbRect = { (float)sbX, (float)sbY, (float)sbW, (float)sbH };
        DrawRectangleRounded(sbRect, 0.6f, 6, Fade(WHITE, 0.08f));
        float thumbH = std::max(20.0f, (float)historyH * (float)historyH / (float)totalH);
        float thumbY = sbY + (m_historyScrollOffset / (float)(totalH - historyH)) * (sbH - thumbH);
        Rectangle thumbRect = { (float)sbX, (float)thumbY, (float)sbW, (float)thumbH };
        DrawRectangleRounded(thumbRect, 0.6f, 6, Fade(WHITE, 0.25f));
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
        // theme button always interactable (rounded)
        DrawRectangleRounded(themeRect, 0.6f, 6, hover ? Fade(GRAY, 0.6f) : Fade(GRAY, 0.5f));
        const char* themeNames[5] = { "Grass", "Wood", "Ocean", "Classic", "Disco" };
        DrawTextEx(m_mainFont, themeNames[m_themeIndex], Vector2{ themeRect.x + 8, themeRect.y + 6 }, 18.0f, 0.0f, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hover)
        {
            cycleTheme();
        }

        // Animation toggle checkbox to the right of the theme button
        const float cbSize = 20.0f;
        float cbX = themeRect.x + boxW + 12.0f;
        float cbY = themeRect.y + (boxH - cbSize) / 2.0f;
        m_animCheckboxBounds = { cbX, cbY, cbSize, cbSize };
        // checkbox background (rounded)
        DrawRectangleRounded(m_animCheckboxBounds, 10.0f, 6, m_animatePieces ? Fade(RED, 0.95f) : Fade(WHITE, 0.15f));
        // label
        DrawTextEx(m_mainFont, "Animation", Vector2{ themeRect.x + boxW + 20.0f + cbSize, themeRect.y + 8.0f }, 16.0f, 0.0f, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mp2 = GetMousePosition();
            if (CheckCollisionPointRec(mp2, m_animCheckboxBounds))
            {
                m_animatePieces = !m_animatePieces;
                if (!m_animatePieces) m_currentAnim.isActive = false; // stop any running animation
            }
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
        DrawRectangleRounded(bgRect, 0.6f, 6, promoBg);

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

void Renderer::drawBoard(const Board& board, const std::optional<std::pair<int,int>>& selected, const GameController& controller) const
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

        // Draw hint overlay if controller provided and has an active hint
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
    DrawRectangleLines(m_boardOriginX, m_boardOriginY, m_tileSize * 8, m_tileSize * 8, WHITE);

    {
        std::string hint = controller.getHintMove();
        if (!hint.empty())
        {
            // Expect at least 4 characters: e.g., e2e4
            if (hint.size() >= 4)
            {
                char f1 = hint[0]; char r1c = hint[1];
                char f2 = hint[2]; char r2c = hint[3];
                if (f1 >= 'a' && f1 <= 'h' && r1c >= '1' && r1c <= '8' && f2 >= 'a' && f2 <= 'h' && r2c >= '1' && r2c <= '8')
                {
                    int c1 = f1 - 'a';
                    int r1 = 8 - (r1c - '0');
                    int c2 = f2 - 'a';
                    int r2 = 8 - (r2c - '0');
                    // logical coords -> tileLeft/top will map to screen when m_flip is set
                    Color fillCol = Fade(GREEN, 0.25f);
                    Rectangle s1 = { (float)tileLeft(c1), (float)tileTop(r1), (float)m_tileSize, (float)m_tileSize };
                    Rectangle s2 = { (float)tileLeft(c2), (float)tileTop(r2), (float)m_tileSize, (float)m_tileSize };
                    DrawRectangleRec(s1, fillCol);
                    DrawRectangleRec(s2, fillCol);
                    // Draw border outlines (rounded)
                    DrawRectangleLinesEx(s1, 2.0f, GREEN);
                    DrawRectangleLinesEx(s2, 2.0f, GREEN);
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

    // Ranks on leftmost screen file (top-left of the tile when not flipped, top-right when flipped)
    int leftLogicalCol = m_flip ? (Board::Tiles - 1) : 0;
    for (int r = 0; r < Board::Tiles; ++r)
    {
        // Top->bottom: if not flipped, ranks are 8..1; if flipped, ranks are 1..8
        int rankNum = (8 - r);
        std::string rankStr = std::to_string(rankNum);

        // Determine tile color at the leftmost screen file for this rank
        bool isLight = ((r + leftLogicalCol) % 2) == 0;
        const Color textColor = isLight ? darkColor : lightColor;

        int textW = (int)MeasureTextEx(m_mainFont, rankStr.c_str(), (float)SMALL_FONT, 0.0f).x;
        int x = tileLeft(leftLogicalCol) + PADDING;
        int y = tileTop(r) + PADDING;
        DrawTextEx(m_mainFont, rankStr.c_str(), { (float)x, (float)y }, (float)SMALL_FONT, 0.0f, textColor);
    }

    // Files on bottom-most rank '1' (bottom-right corner of the tile)
    int bottomLogicalRow = m_flip ? 0 : (Board::Tiles - 1);
    for (int c = 0; c < Board::Tiles; ++c)
    {
        // Files left->right: if not flipped, a..h; if flipped, h..a
        char fileChar = (char)('a' + c);
        char fileStr[2] = { fileChar, '\0' };

        bool isLight = ((bottomLogicalRow + c) % 2) == 0;
        const Color textColor = isLight ? darkColor : lightColor;

        int textW = (int)MeasureTextEx(m_mainFont, fileStr, (float)SMALL_FONT, 0.0f).x;
        int x = tileLeft(c) + m_tileSize - textW - PADDING;
        int y = tileTop(bottomLogicalRow) + m_tileSize - SMALL_FONT - PADDING / 2;
        DrawTextEx(m_mainFont, fileStr, { (float)x, (float)y }, (float)SMALL_FONT, 0.0f, textColor);
    }
}

void Renderer::drawPieces(const Board& board)
{
    // Draw static pieces, but skip the destination tile of an active animation so we can draw
    // the moving piece separately and avoid duplication.
    for (int r = 0; r < Board::Tiles; ++r)
    {
        for (int c = 0; c < Board::Tiles; ++c)
        {
            if (m_currentAnim.isActive && m_animatePieces)
            {
                // Destination(s) will be drawn as the moving sprite(s) to avoid duplication
                if (r == m_currentAnim.toY && c == m_currentAnim.toX) continue;
                if (m_currentAnim.hasSecondaryPiece && r == m_currentAnim.toY && c == m_currentAnim.secToX) continue;
            }

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
            float centerX = (float)(tileLeft(c) + m_tileSize / 2);
            float centerY = (float)(tileTop(r) + m_tileSize / 2);
            Rectangle dstRec = { centerX - drawW / 2.0f, centerY - drawH / 2.0f, drawW, drawH };

            // No rotation, white tint
            DrawTexturePro(*tex, srcRec, dstRec, {0,0}, 0.0f, WHITE);
        }
    }

    // Draw animated moving piece on top if active
    if (m_currentAnim.isActive && m_animatePieces)
    {
        // Centers for start/end squares
        Vector2 startPos = { (float)tileLeft(m_currentAnim.fromX) + m_tileSize * 0.5f,
                             (float)tileTop(m_currentAnim.fromY) + m_tileSize * 0.5f };
        Vector2 targetPos = { (float)tileLeft(m_currentAnim.toX) + m_tileSize * 0.5f,
                              (float)tileTop(m_currentAnim.toY) + m_tileSize * 0.5f };

        // Advance progress (speed multiplier 6.0f as requested)

        m_currentAnim.progress += GetFrameTime() * 6.0f;
        if (m_currentAnim.progress > 1.0f) m_currentAnim.progress = 1.0f;

        float curvedProgress = m_currentAnim.progress * m_currentAnim.progress * (3.0f - 2.0f * m_currentAnim.progress);

        Vector2 cur = Vector2Lerp(startPos, targetPos, curvedProgress);

        std::string key;
        key.push_back(m_currentAnim.pieceChar);
        key.push_back(m_currentAnim.pieceColor);
        const Texture2D* tex = textureForKey(key);
        if (tex)
        {
            float srcW = (float)tex->width;
            float srcH = (float)tex->height;
            float maxW = (float)m_tileSize * 0.85f; // same padding
            float maxH = (float)m_tileSize * 0.85f;
            float scale = std::min(maxW / srcW, maxH / srcH);
            float drawW = srcW * scale;
            float drawH = srcH * scale;
            Rectangle srcRec = { 0.0f, 0.0f, srcW, srcH };
            Rectangle dstRec = { cur.x - drawW / 2.0f, cur.y - drawH / 2.0f, drawW, drawH };
            DrawTexturePro(*tex, srcRec, dstRec, {0,0}, 0.0f, WHITE);
        }

        // If a secondary piece (rook) should move (castling), draw it using the same progress curve
        if (m_currentAnim.hasSecondaryPiece)
        {
            Vector2 rookStart = { (float)tileLeft(m_currentAnim.secFromX) + m_tileSize * 0.5f,
                                  (float)tileTop(m_currentAnim.fromY) + m_tileSize * 0.5f };
            Vector2 rookTarget = { (float)tileLeft(m_currentAnim.secToX) + m_tileSize * 0.5f,
                                   (float)tileTop(m_currentAnim.toY) + m_tileSize * 0.5f };
            Vector2 rookCur = Vector2Lerp(rookStart, rookTarget, curvedProgress);

            std::string key2;
            key2.push_back(m_currentAnim.secPieceChar);
            key2.push_back(m_currentAnim.pieceColor); // rook color matches primary piece
            const Texture2D* tex2 = textureForKey(key2);
            if (tex2)
            {
                float srcW = (float)tex2->width;
                float srcH = (float)tex2->height;
                float maxW = (float)m_tileSize * 0.85f;
                float maxH = (float)m_tileSize * 0.85f;
                float scale = std::min(maxW / srcW, maxH / srcH);
                float drawW = srcW * scale;
                float drawH = srcH * scale;
                Rectangle srcRec = { 0.0f, 0.0f, srcW, srcH };
                Rectangle dstRec = { rookCur.x - drawW / 2.0f, rookCur.y - drawH / 2.0f, drawW, drawH };
                DrawTexturePro(*tex2, srcRec, dstRec, {0,0}, 0.0f, WHITE);
            }
        }

        // Clean reset: when finished, disable animation so next frame the board draws normally
        if (m_currentAnim.progress >= 1.0f)
        {
            m_currentAnim.isActive = false;
            m_currentAnim.progress = 0.0f;
        }
    }
}
