#pragma once

#include <memory>
#include <vector>
#include <string>

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

    bool isSquareUnderAttack(int row, int col, PieceColor attackerColor) const;
    bool isInCheck(PieceColor color) const;

    // Determine if the given side has any legal moves
    bool hasLegalMoves(PieceColor color) const;

    // Game state
    GameState getGameState() const;

    // Last move error message for UI
    const std::string& getLastMoveError() const;

    // Check utilities
    bool isSquareUnderAttack(int row, int col, PieceColor attackerColor) const;
    bool isInCheck(PieceColor color) const;

    // Current turn (White starts)
    PieceColor getCurrentTurn() const;

private:
    // Own the pieces via unique_ptr, empty squares are nullptr
    std::unique_ptr<Piece> m_squares[Tiles][Tiles];
    // Whose turn it is to move
    PieceColor m_currentTurn;
    // Last move error message (filled when movePiece returns false)
    std::string m_lastMoveError;
};
