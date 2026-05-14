#include "Piece.h"
#include "Board.h"

#include <string>
#include <cstdlib>

Piece::Piece(PieceType type, PieceColor color, int row, int col)
    : m_type(type), m_color(color), m_row(row), m_col(col)
{
}

bool Piece::hasMoved() const { return m_hasMoved; }
void Piece::setMoved(bool moved) { m_hasMoved = moved; }

void Piece::setType(PieceType type) { m_type = type; }

namespace {
    // Helper: check straight-line path (rook-like) is clear between start and end
    bool isStraightPathClear(const Board& board, int sr, int sc, int er, int ec)
    {
        if (sr == er)
        {
            int step = (ec > sc) ? 1 : -1;
            for (int c = sc + step; c != ec; c += step)
            {
                if (board.at(sr, c) != nullptr) return false;
            }
            return true;
        }
        else if (sc == ec)
        {
            int step = (er > sr) ? 1 : -1;
            for (int r = sr + step; r != er; r += step)
            {
                if (board.at(r, sc) != nullptr) return false;
            }
            return true;
        }
        return false;
    }

    // Helper: check diagonal path (bishop-like) is clear between start and end
    bool isDiagonalPathClear(const Board& board, int sr, int sc, int er, int ec)
    {
        int dr = er - sr;
        int dc = ec - sc;
        if (std::abs(dr) != std::abs(dc)) return false;
        int stepR = (dr > 0) ? 1 : -1;
        int stepC = (dc > 0) ? 1 : -1;
        int r = sr + stepR;
        int c = sc + stepC;
        while (r != er && c != ec)
        {
            if (board.at(r, c) != nullptr) return false;
            r += stepR;
            c += stepC;
        }
        return true;
    }
}

bool Piece::isValidMove(int targetRow, int targetCol, const Board& board) const
{
    // Basic bounds check
    if (targetRow < 0 || targetRow >= Board::Tiles || targetCol < 0 || targetCol >= Board::Tiles)
        return false;

    int sr = m_row;
    int sc = m_col;

    // Destination piece (if any)
    const Piece* dest = board.at(targetRow, targetCol);
    // Disallow capturing own pieces
    if (dest && dest->color() == m_color) return false;

    switch (m_type)
    {
    case PieceType::Rook:
    {
        if (sr == targetRow || sc == targetCol)
        {
            return isStraightPathClear(board, sr, sc, targetRow, targetCol);
        }
        return false;
    }
    case PieceType::Knight:
    {
        int dr = std::abs(targetRow - sr);
        int dc = std::abs(targetCol - sc);
        // L-shape (2,1) or (1,2) and can jump over pieces
        return (dr == 2 && dc == 1) || (dr == 1 && dc == 2);
    }
    case PieceType::Pawn:
    {
        // Pawns move differently based on color. White pawns move "up" (decreasing row)
        int dir = (m_color == PieceColor::White) ? -1 : 1;

        // Single forward
        if (targetCol == sc && targetRow == sr + dir)
        {
            return (dest == nullptr);
        }

        // Double forward from starting rank
        int startRow = (m_color == PieceColor::White) ? 6 : 1;
        if (sr == startRow && targetCol == sc && targetRow == sr + 2 * dir)
        {
            // Must be clear in between and destination empty
            if (board.at(sr + dir, sc) != nullptr) return false;
            return (dest == nullptr);
        }

        // Diagonal capture
        if ((targetCol == sc + 1 || targetCol == sc - 1) && targetRow == sr + dir)
        {
            // Normal capture
            if (dest != nullptr && dest->color() != m_color) return true;

            // En passant capture: destination empty but matches board's en passant target
            auto ep = board.getEnPassantTarget();
            if (ep.first == targetRow && ep.second == targetCol)
            {
                // The captured pawn is located on the starting row, in the target column
                const Piece* cap = board.at(sr, targetCol);
                if (cap && cap->type() == PieceType::Pawn && cap->color() != m_color)
                    return true;
            }
            return false;
        }

        return false;
    }
    case PieceType::Bishop:
    {
        if (std::abs(targetRow - sr) == std::abs(targetCol - sc))
        {
            return isDiagonalPathClear(board, sr, sc, targetRow, targetCol);
        }
        return false;
    }
    case PieceType::Queen:
    {
        // Queen combines rook and bishop
        if (sr == targetRow || sc == targetCol)
        {
            return isStraightPathClear(board, sr, sc, targetRow, targetCol);
        }
        if (std::abs(targetRow - sr) == std::abs(targetCol - sc))
        {
            return isDiagonalPathClear(board, sr, sc, targetRow, targetCol);
        }
        return false;
    }
    case PieceType::King:
    {
        int dr = std::abs(targetRow - sr);
        int dc = std::abs(targetCol - sc);
        // One square any direction
        if ((dr <= 1) && (dc <= 1) && !(dr == 0 && dc == 0))
        {
            return true; // capture handled by top-of-function color check
        }

        // Castling: two-square horizontal move
        if (dr == 0 && dc == 2)
        {
            // Check board-level castling rights rather than piece moved flags
            int rookCol = (targetCol > sc) ? 7 : 0;
            const Piece* rook = board.at(sr, rookCol);
            if (!rook) return false;
            if (rook->type() != PieceType::Rook) return false;
            if (rook->color() != m_color) return false;

            // Determine required castling right
            uint8_t rights = board.getCastlingRights();
            bool allowed = false;
            if (m_color == PieceColor::White)
            {
                if (targetCol > sc) allowed = (rights & Board::CR_WHITE_K);
                else allowed = (rights & Board::CR_WHITE_Q);
            }
            else
            {
                if (targetCol > sc) allowed = (rights & Board::CR_BLACK_K);
                else allowed = (rights & Board::CR_BLACK_Q);
            }
            if (!allowed) return false;

            int step = (targetCol > sc) ? 1 : -1;
            // Squares between king and rook must be empty
            for (int c = sc + step; c != rookCol; c += step)
            {
                if (board.at(sr, c) != nullptr) return false;
            }

            // King must not be in check, and cannot pass through or land on attacked square
            PieceColor opponent = (m_color == PieceColor::White) ? PieceColor::Black : PieceColor::White;
            if (board.isInCheck(m_color)) return false;
            if (board.isSquareUnderAttack(sr, sc + step, opponent)) return false;
            if (board.isSquareUnderAttack(sr, targetCol, opponent)) return false;

            return true;
        }

        return false;
    }
    default:
        // Other pieces not implemented yet
        return false;
    }
}

PieceType Piece::type() const { return m_type; }
PieceColor Piece::color() const { return m_color; }
int Piece::row() const { return m_row; }
int Piece::col() const { return m_col; }

void Piece::setPosition(int row, int col)
{
    m_row = row;
    m_col = col;
}

std::string Piece::assetKey() const
{
    // Map types to single-letter keys matching asset filenames used in the repository
    // k = king, q = queen, r = rook, b = bishop, n = knight, p = pawn
    char t = 'p';
    switch (m_type)
    {
    case PieceType::King: t = 'k'; break;
    case PieceType::Queen: t = 'q'; break;
    case PieceType::Rook: t = 'r'; break;
    case PieceType::Bishop: t = 'b'; break;
    case PieceType::Knight: t = 'n'; break;
    case PieceType::Pawn: t = 'p'; break;
    }

    char c = (m_color == PieceColor::White) ? 'l' : 'd';
    std::string s;
    s.push_back(t);
    s.push_back(c);
    return s; // e.g. "kl" or "qd"
}
