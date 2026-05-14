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

    Board();

    const Piece* at(int row, int col) const;
    Piece* at(int row, int col);

    void initializeStandardSetup();
    // Attempt to move a piece from start to end. Returns true if the move
    // was legal according to piece-specific isValidMove and the board state,
    // and updates the board state (including captures).
    bool movePiece(int startRow, int startCol, int endRow, int endCol);

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
    };

    void undoMove();
    void redoMove();
    // Check whether moving the piece at (r1,c1) to (r2,c2) would be legal
    // (including checks that would leave the king in check). This simulates
    // the move internally and restores board state before returning.
    bool wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol);
    // Promotion handling
    bool isAwaitingPromotion() const;
    std::pair<int,int> getPendingPromotionSquare() const;
    void completePromotion(PieceType chosenType);

    // Current turn (White starts)
    PieceColor getCurrentTurn() const;

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
};
