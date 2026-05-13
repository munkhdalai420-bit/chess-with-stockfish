#include "Board.h"

#include <cassert>
#include <utility>

Board::Board()
{
    for (int r = 0; r < Tiles; ++r)
        for (int c = 0; c < Tiles; ++c)
            m_squares[r][c].reset();
    m_currentTurn = PieceColor::White;
    m_lastMoveError.clear();
}

bool Board::hasLegalMoves(PieceColor color) const
{
    // Iterate all pieces of the given color and try every destination.
    for (int sr = 0; sr < Tiles; ++sr)
    {
        for (int sc = 0; sc < Tiles; ++sc)
        {
            const Piece* p = at(sr, sc);
            if (!p) continue;
            if (p->color() != color) continue;

            // Try every destination square
            for (int tr = 0; tr < Tiles; ++tr)
            {
                for (int tc = 0; tc < Tiles; ++tc)
                {
                    if (tr == sr && tc == sc) continue;

                    // First check movement validity
                    if (!p->isValidMove(tr, tc, *this)) continue;

                    // Simulate move
                    // Temporarily take ownership
                    std::unique_ptr<Piece> movingPiece = nullptr;
                    std::unique_ptr<Piece> capturedPiece = nullptr;
                    // We need to perform a mutable simulation on this const method. Cast away const safely
                    Board& self = const_cast<Board&>(*this);
                    movingPiece = std::move(self.m_squares[sr][sc]);
                    capturedPiece = std::move(self.m_squares[tr][tc]);
                    self.m_squares[tr][tc] = std::move(movingPiece);
                    if (self.m_squares[tr][tc]) self.m_squares[tr][tc]->setPosition(tr, tc);

                    bool inCheck = self.isInCheck(color);

                    // Revert
                    self.m_squares[sr][sc] = std::move(self.m_squares[tr][tc]);
                    if (self.m_squares[sr][sc]) self.m_squares[sr][sc]->setPosition(sr, sc);
                    self.m_squares[tr][tc] = std::move(capturedPiece);

                    if (!inCheck) return true;
                }
            }
        }
    }
    return false;
}

Board::GameState Board::getGameState() const
{
    if (hasLegalMoves(m_currentTurn)) return GameState::Active;
    if (isInCheck(m_currentTurn)) return GameState::Checkmate;
    return GameState::Stalemate;
}

const std::string& Board::getLastMoveError() const
{
    return m_lastMoveError;
}

bool Board::movePiece(int startRow, int startCol, int endRow, int endCol)
{
    // Basic bounds
    if (startRow < 0 || startRow >= Tiles || startCol < 0 || startCol >= Tiles)
    {
        m_lastMoveError = "Invalid source coordinates";
        return false;
    }
    if (endRow < 0 || endRow >= Tiles || endCol < 0 || endCol >= Tiles)
    {
        m_lastMoveError = "Invalid destination coordinates";
        return false;
    }

    Piece* p = at(startRow, startCol);
    if (!p)
    {
        m_lastMoveError = "No piece at source square";
        return false; // no piece to move
    }

    // Check turn
    if (p->color() != m_currentTurn)
    {
        m_lastMoveError = "Not your turn";
        return false;
    }

    // Ensure the piece's movement rules allow this move
    if (!p->isValidMove(endRow, endCol, *this))
    {
        m_lastMoveError = "Invalid move (movement rules or blocked)";
        return false;
    }

    const Piece* dest = at(endRow, endCol);
    // Disallow capturing own piece (isValidMove should already check, but double-check)
    if (dest && dest->color() == p->color())
    {
        m_lastMoveError = "Cannot capture your own piece";
        return false;
    }

    // Simulate the move transactionally: move the unique_ptrs, check for leaving king in check,
    // and revert if the move is illegal because it exposes own king.
    PieceColor movingColor = p->color();

    // Temporarily take ownership of the moving piece and any captured piece
    std::unique_ptr<Piece> movingPiece = std::move(m_squares[startRow][startCol]);
    std::unique_ptr<Piece> capturedPiece = std::move(m_squares[endRow][endCol]);

    // Place moving piece at destination
    m_squares[endRow][endCol] = std::move(movingPiece);
    if (m_squares[endRow][endCol])
    {
        m_squares[endRow][endCol]->setPosition(endRow, endCol);
    }

    // Now check whether this move leaves the mover in check
    if (isInCheck(movingColor))
    {
        // Undo move: move piece back to original square and restore captured piece
        m_squares[startRow][startCol] = std::move(m_squares[endRow][endCol]);
        if (m_squares[startRow][startCol])
        {
            m_squares[startRow][startCol]->setPosition(startRow, startCol);
        }
        m_squares[endRow][endCol] = std::move(capturedPiece);
        m_lastMoveError = "Move would leave king in check";
        return false;
    }

    // Move is legal: capturedPiece (if any) will be destroyed here when capturedPiece goes out of scope
    m_lastMoveError.clear();
    // Toggle turn
    m_currentTurn = (m_currentTurn == PieceColor::White) ? PieceColor::Black : PieceColor::White;

    return true;
}

bool Board::isSquareUnderAttack(int row, int col, PieceColor attackerColor) const
{
    // Iterate all pieces of attackerColor and see if any can move to (row,col)
    for (int r = 0; r < Tiles; ++r)
    {
        for (int c = 0; c < Tiles; ++c)
        {
            const Piece* p = at(r, c);
            if (!p) continue;
            if (p->color() != attackerColor) continue;
            if (p->isValidMove(row, col, *this)) return true;
        }
    }
    return false;
}

bool Board::isInCheck(PieceColor color) const
{
    // Find king of 'color'
    int kr = -1, kc = -1;
    for (int r = 0; r < Tiles; ++r)
    {
        for (int c = 0; c < Tiles; ++c)
        {
            const Piece* p = at(r, c);
            if (!p) continue;
            if (p->type() == PieceType::King && p->color() == color)
            {
                kr = r; kc = c; break;
            }
        }
        if (kr != -1) break;
    }

    if (kr == -1) return false; // no king found (shouldn't happen)

    PieceColor opponent = (color == PieceColor::White) ? PieceColor::Black : PieceColor::White;
    return isSquareUnderAttack(kr, kc, opponent);
}

PieceColor Board::getCurrentTurn() const
{
    return m_currentTurn;
}

const Piece* Board::at(int row, int col) const
{
    assert(row >= 0 && row < Tiles && col >= 0 && col < Tiles);
    return m_squares[row][col].get();
}

Piece* Board::at(int row, int col)
{
    assert(row >= 0 && row < Tiles && col >= 0 && col < Tiles);
    return m_squares[row][col].get();
}

void Board::initializeStandardSetup()
{
    // Clear board first
    for (int r = 0; r < Tiles; ++r)
        for (int c = 0; c < Tiles; ++c)
            m_squares[r][c].reset();

    // Helper to place piece
    auto place = [&](PieceType type, PieceColor color, int row, int col)
    {
        m_squares[row][col] = std::make_unique<Piece>(type, color, row, col);
    };

    // Pawns
    for (int c = 0; c < Tiles; ++c)
    {
        place(PieceType::Pawn, PieceColor::White, 6, c);
        place(PieceType::Pawn, PieceColor::Black, 1, c);
    }

    // Rooks
    place(PieceType::Rook, PieceColor::White, 7, 0);
    place(PieceType::Rook, PieceColor::White, 7, 7);
    place(PieceType::Rook, PieceColor::Black, 0, 0);
    place(PieceType::Rook, PieceColor::Black, 0, 7);

    // Knights
    place(PieceType::Knight, PieceColor::White, 7, 1);
    place(PieceType::Knight, PieceColor::White, 7, 6);
    place(PieceType::Knight, PieceColor::Black, 0, 1);
    place(PieceType::Knight, PieceColor::Black, 0, 6);

    // Bishops
    place(PieceType::Bishop, PieceColor::White, 7, 2);
    place(PieceType::Bishop, PieceColor::White, 7, 5);
    place(PieceType::Bishop, PieceColor::Black, 0, 2);
    place(PieceType::Bishop, PieceColor::Black, 0, 5);

    // Queens
    place(PieceType::Queen, PieceColor::White, 7, 3);
    place(PieceType::Queen, PieceColor::Black, 0, 3);

    // Kings
    place(PieceType::King, PieceColor::White, 7, 4);
    place(PieceType::King, PieceColor::Black, 0, 4);
}
