#pragma once

#include <string>

#include "raylib.h"

enum class PieceType { King, Queen, Rook, Bishop, Knight, Pawn };
enum class PieceColor { White, Black };

class Piece {
public:
    Piece(PieceType type, PieceColor color, int row, int col);

    PieceType type() const;
    PieceColor color() const;
    int row() const;
    int col() const;
    void setPosition(int row, int col);

    // Returns an asset key used by the Renderer (e.g. "k_l" or "q_d")
    std::string assetKey() const;

    // Validate a prospective move for this piece on the given board.
    // Returns true if the piece is allowed to move to targetRow/targetCol
    // according to (basic) piece movement rules. Board state is provided
    // to allow checking for blocking pieces and captures.
    virtual bool isValidMove(int targetRow, int targetCol, const class Board& board) const;

    // Movement state
    bool hasMoved() const;
    void setMoved(bool moved);

    // Change piece type (used for promotion)
    void setType(PieceType type);

private:
    PieceType m_type;
    PieceColor m_color;
    int m_row;
    int m_col;
    bool m_hasMoved = false;
};
