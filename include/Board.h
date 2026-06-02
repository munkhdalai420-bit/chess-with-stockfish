#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>

#include "Piece.h"

class Board
{
public:
    static constexpr int Tiles = 8;

    enum class GameState { Active, Checkmate, Stalemate };
    enum class MoveResult { Invalid = 0, Normal, Capture, Castle, Promotion, Check };

    // Castling rights bitmask (bit0 = White kingside, bit1 = White queenside,
    // bit2 = Black kingside, bit3 = Black queenside)
    enum CastlingMask : uint8_t { CR_WHITE_K = 1 << 0, CR_WHITE_Q = 1 << 1, CR_BLACK_K = 1 << 2, CR_BLACK_Q = 1 << 3 };

    Board();

    const Piece* at(int row, int col) const;
    Piece* at(int row, int col);

    void initializeStandardSetup();
    // Attempt to move a piece from start to end. Returns a MoveResult
    // indicating the outcome (Invalid if move failed). Updates board state
    // (including captures) when the move succeeds.
    MoveResult movePiece(int startRow, int startCol, int endRow, int endCol);

    // Check utilities
    // Check utilities
    bool isSquareUnderAttack(int row, int col, PieceColor attackerColor) const;
    bool isInCheck(PieceColor color) const;

    // Determine if the given side has any legal moves
    bool hasLegalMoves(PieceColor color) const;
    bool hasLegalMoves(PieceColor color);

    // Game state
    GameState getGameState() const;

    // Last move error message for UI
    const std::string& getLastMoveError() const;

    // En passant target square (row,col) or {-1,-1} if none
    std::pair<int,int> getEnPassantTarget() const;

    // Undo/Redo support
    struct ChessMove
    {
        int r1, c1, r2, c2;
        PieceType movedType;
        PieceColor movedColor;
        std::optional<PieceType> capturedType;
        std::optional<PieceColor> capturedColor;
        bool isCastling = false;
        bool isEnPassant = false;
        bool isPromotion = false;
        std::pair<int,int> enPassantBefore = {-1,-1};
        std::optional<PieceType> promotionChoice;
        bool movedPieceHadMoved = false;
        bool rookHadMoved = false;
        int rookSrcCol = -1;
        int rookDstCol = -1;
        // Record castling rights before and after the move to allow undo/redo
        uint8_t castlingBefore = 0;
        uint8_t castlingAfter = 0;
        // Save clocks prior to the move so undo can restore them
        int halfmoveClockBefore = 0;
        int fullmoveNumberBefore = 1;
        // SAN string for this move (filled externally after move is executed)
        std::string san;
    };

    void undoMove();
    void redoMove();
    // Check whether moving the piece at (r1,c1) to (r2,c2) would be legal
    // (including checks that would leave the king in check). This simulates
    // the move internally and restores board state before returning.
    bool wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol) const;
    // Promotion handling
    bool isAwaitingPromotion() const;
    std::pair<int,int> getPendingPromotionSquare() const;
    MoveResult completePromotion(PieceType chosenType);

    // Last committed move (if any)
    std::optional<ChessMove> getLastMove() const;

    // FEN support
    std::string getFEN() const;
    bool loadFromFEN(const std::string& fen);

    // PGN / SAN utilities
    std::string moveToSAN(const ChessMove& move);
    void setLastMoveSAN(const std::string& san);
    std::string getFullPGNText() const;

    // Parse Stockfish's Long Algebraic Notation (LAN) string into a ChessMove object
    // Input format: 4-5 characters (e.g., 'e2e4' or 'e7e8q')
    // For promotion moves (5 chars), the 5th character is not processed here
    ChessMove parseEngineMove(const std::string& moveStr);

    // Self-test utilities
    //void runFENTests();

    // Current turn (White starts)
    PieceColor getCurrentTurn() const;

    // Castling rights access
    uint8_t getCastlingRights() const;

private:
    // Own the pieces via unique_ptr, empty squares are nullptr
    std::unique_ptr<Piece> m_squares[Tiles][Tiles];
    // Whose turn it is to move
    PieceColor m_currentTurn;
    // Last move error message (filled when movePiece returns false)
    std::string m_lastMoveError;
    // En passant target square (-1,-1 when not available)
    std::pair<int,int> m_enPassantTarget = { -1, -1 };
    // Move history for undo/redo
    std::vector<ChessMove> m_moveHistory;
    std::vector<ChessMove> m_redoStack;
    // Promotion awaiting state
    bool m_isAwaitingPromotion = false;
    std::pair<int,int> m_pendingPromotionSquare = {-1,-1};
    // Last committed move (for UI highlighting)
    std::optional<ChessMove> m_lastMove;
    // FEN-related clocks
    int m_halfmoveClock = 0;
    int m_fullmoveNumber = 1;
    // Board-level castling rights
    uint8_t m_castlingRights = CR_WHITE_K | CR_WHITE_Q | CR_BLACK_K | CR_BLACK_Q;
    // Internal helper (removed: clocks are now updated incrementally)
};
