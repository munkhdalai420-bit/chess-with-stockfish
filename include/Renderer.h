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
    /** Visual board themes available to the renderer. */
    enum class BoardTheme { Wood, Grass, Ocean, Classic, Disco };

    /**
     * @brief Construct a Renderer
     * @param windowWidth Application window width in pixels
     * @param windowHeight Application window height in pixels
     * @param tileSize Size in pixels of a single chess tile
     */
    Renderer(int windowWidth, int windowHeight, int tileSize);

    /** @brief Destructor; releases GPU textures and font resources. */
    ~Renderer();

    /**
     * @brief Set the visual theme used for board coloring.
     * @param theme Theme enumerator
     */
    void setTheme(BoardTheme theme);

    /**
     * @brief Load piece textures from the resources directory.
     * @return true when all required textures were successfully loaded.
     */
    bool loadTextures();

    /**
     * @brief Render the full UI for the frame.
     *
     * The renderer reads game state from the provided const `Board` and selected
     * tile, draws board and pieces, and may invoke commands on `controller`
     * (e.g., start/stop/undo) in response to UI interactions.
     * @param board Const reference to the game board to draw.
     * @param selected Optional selected square (row,col) to highlight.
     * @param evaluation Displayed engine evaluation value for the current history index.
     * @param controller Non-const reference so the renderer may call UI commands.
     */
    void render(const Board& board, const std::optional<std::pair<int,int>>& selected, float evaluation, GameController& controller);

    /**
     * @brief Start an on-screen move animation from source to destination squares.
     * @param fromX Source column (0..7)
     * @param fromY Source row (0..7)
     * @param toX Destination column (0..7)
     * @param toY Destination row (0..7)
     * @param pieceChar Piece type character (e.g., 'k','q','r','b','n','p')
     * @param pieceColor Piece color code ('l' or 'd')
     * @param hasSecondary When true, animate a secondary piece (rook) for castling
     * @param secFromX Secondary piece source column
     * @param secToX Secondary piece destination column
     * @param secPieceChar Secondary piece type character (default 'r')
     */
    void triggerMoveAnimation(int fromX, int fromY, int toX, int toY, char pieceChar, char pieceColor,
                              bool hasSecondary = false, int secFromX = -1, int secToX = -1, char secPieceChar = 'r');

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
    BoardTheme m_theme = BoardTheme::Wood;
    int m_themeIndex = 2; // 0=Grass,1=Wood,2=Ocean,3=Classic (default Ocean)

    std::string textureFilenameForKey(const std::string& key) const;
    const Texture2D* textureForKey(const std::string& key) const;

    void drawBoard(const Board& board, const std::optional<std::pair<int,int>>& selected, const GameController& controller) const;
    void drawPieces(const Board& board);
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
        char pieceChar = 0;         // e.g., 'p','k','q'
        char pieceColor = 0;        // 'l' or 'd'
        float progress = 0.0f;      // 0.0 .. 1.0

        // Add optional secondary piece for castling
        bool hasSecondaryPiece = false;
        int secFromX = -1, secToX = -1; // only X columns needed; Y will match the king's
        char secPieceChar = 'R';
    };

    bool m_animatePieces = true;    // animations are permanently enabled
    PieceAnimation m_currentAnim;
    // Vertical scroll offset for the move history panel (pixels)
    float m_historyScrollOffset = 0.0f;
    // Double-click tracking for move history (timestamp and last clicked line index)
    double m_lastHistoryClickTime = 0.0;
    int m_lastHistoryClickedIndex = -1;
    // Whether the visual board is flipped (human chose to play Black).
    // This is updated each frame from Renderer::render(). Marked mutable so const draw helpers
    // can consult it without breaking const-correctness.
    mutable bool m_flip = false;
};
