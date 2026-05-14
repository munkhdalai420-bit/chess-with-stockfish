#include "Board.h"

#include <cassert>
#include <utility>
#include <cstdlib>

Board::Board()
{
    for (int r = 0; r < Tiles; ++r)
        for (int c = 0; c < Tiles; ++c)
            m_squares[r][c].reset();
    m_currentTurn = PieceColor::White;
    m_lastMoveError.clear();
    m_enPassantTarget = std::make_pair(-1, -1);
}

bool Board::hasLegalMoves(PieceColor color) const
{
    // NOTE: this function previously simulated moves by mutating the board; with undo/redo
    // machinery available, we can attempt real moves and undo them. However this member
    // needs to mutate the board during simulation, so it cannot be const. For safety,
    // callers should invoke the non-const version. This const wrapper simply casts away
    // const and delegates to the non-const implementation.
    Board& self = const_cast<Board&>(*this);
    return self.hasLegalMoves(color);
}

bool Board::hasLegalMoves(PieceColor color)
{
    // Save state to restore after simulation
    auto savedHistory = m_moveHistory;
    auto savedRedo = m_redoStack;
    auto savedEnPassant = m_enPassantTarget;
    auto savedTurn = m_currentTurn;
    auto savedLastError = m_lastMoveError;
    // Also save promotion-awaiting state to avoid leaving UI in a pending state
    bool savedAwaiting = m_isAwaitingPromotion;
    auto savedPending = m_pendingPromotionSquare;

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

                    // Quick pre-check using movement rules to avoid expensive calls
                    if (!p->isValidMove(tr, tc, *this)) continue;

                    // Ensure turn matches the mover
                    m_currentTurn = color;

                    bool moved = movePiece(sr, sc, tr, tc);
                    if (moved)
                    {
                        // Undo to restore state
                        undoMove();

                        // Restore saved state to avoid polluting history/redo
                        m_moveHistory = savedHistory;
                        m_redoStack = savedRedo;
                        m_enPassantTarget = savedEnPassant;
                        m_currentTurn = savedTurn;
                        m_lastMoveError = savedLastError;
                        m_isAwaitingPromotion = savedAwaiting;
                        m_pendingPromotionSquare = savedPending;

                        return true;
                    }

                    // Restore any state changes from failed move
                    m_moveHistory = savedHistory;
                    m_redoStack = savedRedo;
                    m_enPassantTarget = savedEnPassant;
                    m_currentTurn = savedTurn;
                    m_lastMoveError = savedLastError;
                    m_isAwaitingPromotion = savedAwaiting;
                    m_pendingPromotionSquare = savedPending;
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

std::pair<int,int> Board::getEnPassantTarget() const
{
    return m_enPassantTarget;
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

    // Get piece and moving type/color early
    Piece* piecePtr = m_squares[startRow][startCol].get();
    if (!piecePtr)
    {
        m_lastMoveError = "No piece at source square";
        return false;
    }
    PieceType movingType = piecePtr->type();
    PieceColor movingColor = piecePtr->color();

    // Prepare move record for history
    ChessMove mv;
    mv.r1 = startRow; mv.c1 = startCol; mv.r2 = endRow; mv.c2 = endCol;
    mv.movedType = movingType;
    mv.movedColor = movingColor;
    mv.enPassantBefore = m_enPassantTarget;
    mv.movedPieceHadMoved = piecePtr->hasMoved();

    // Determine captured piece info (normal capture or en passant)
    if (dest)
    {
        mv.capturedType = dest->type();
        mv.capturedColor = dest->color();
    }
    // local aliases
    // PieceType movingType and PieceColor movingColor already declared above
    int dir = (movingColor == PieceColor::White) ? -1 : 1;

    bool isPawn = (movingType == PieceType::Pawn);
    bool isTwoStep = isPawn && (std::abs(endRow - startRow) == 2);
    // En passant capture detection: diagonal move into empty square which matches en passant target
    bool isEnPassantCapture = false;
    if (isPawn && std::abs(endCol - startCol) == 1 && endRow == startRow + dir && dest == nullptr)
    {
        auto ep = m_enPassantTarget;
        if (ep.first == endRow && ep.second == endCol)
            isEnPassantCapture = true;
    }

    if (isEnPassantCapture)
    {
        // captured pawn is at startRow,endCol
        const Piece* cap = at(startRow, endCol);
        if (cap)
        {
            mv.capturedType = cap->type();
            mv.capturedColor = cap->color();
            mv.isEnPassant = true;
        }
    }

    bool isCastling = (movingType == PieceType::King && startRow == endRow && std::abs(endCol - startCol) == 2);
    int rookSrcCol = -1, rookDstCol = -1;
    if (isCastling)
    {
        rookSrcCol = (endCol > startCol) ? 7 : 0;
        rookDstCol = (endCol > startCol) ? endCol - 1 : endCol + 1;
    }

    if (isCastling)
    {
        const Piece* rook = at(startRow, rookSrcCol);
        if (rook)
        {
            mv.rookSrcCol = rookSrcCol;
            mv.rookDstCol = rookDstCol;
            mv.rookHadMoved = rook->hasMoved();
            mv.isCastling = true;
        }
    }

    // Promotion detection
    if (movingType == PieceType::Pawn)
    {
        int promotionRow = (movingColor == PieceColor::White) ? 0 : 7;
        if (endRow == promotionRow) mv.isPromotion = true;
    }

    // Save pointers for restoration
    std::unique_ptr<Piece> movingPiece = std::move(m_squares[startRow][startCol]);
    std::unique_ptr<Piece> capturedPiece = std::move(m_squares[endRow][endCol]);
    std::unique_ptr<Piece> capturedEP = nullptr;
    std::unique_ptr<Piece> rookPiece = nullptr;
    std::unique_ptr<Piece> rookCaptured = nullptr;

    // Handle en passant captured pawn (located at startRow, endCol)
    if (isEnPassantCapture)
    {
        capturedEP = std::move(m_squares[startRow][endCol]);
    }

    // Handle castling rook movement
    if (isCastling)
    {
        rookPiece = std::move(m_squares[startRow][rookSrcCol]);
        rookCaptured = std::move(m_squares[startRow][rookDstCol]);
    }

    // Place moving piece at destination
    m_squares[endRow][endCol] = std::move(movingPiece);
    if (m_squares[endRow][endCol]) m_squares[endRow][endCol]->setPosition(endRow, endCol);

    // If castling, place rook at its destination
    if (isCastling)
    {
        m_squares[startRow][rookDstCol] = std::move(rookPiece);
        if (m_squares[startRow][rookDstCol]) m_squares[startRow][rookDstCol]->setPosition(startRow, rookDstCol);
    }

    // If en passant capture, remove the captured pawn from board (already moved into capturedEP)
    if (isEnPassantCapture)
    {
        m_squares[startRow][endCol] = nullptr;
    }

    // Now check whether this move leaves the mover in check
    if (isInCheck(movingColor))
    {
        // Undo move: move piece back to original square and restore captures
        m_squares[startRow][startCol] = std::move(m_squares[endRow][endCol]);
        if (m_squares[startRow][startCol]) m_squares[startRow][startCol]->setPosition(startRow, startCol);

        // Restore captured piece on destination
        m_squares[endRow][endCol] = std::move(capturedPiece);

        // Restore en passant captured pawn
        if (isEnPassantCapture)
        {
            m_squares[startRow][endCol] = std::move(capturedEP);
        }

        // Restore rook if castling
        if (isCastling)
        {
            m_squares[startRow][rookSrcCol] = std::move(m_squares[startRow][rookDstCol]);
            if (m_squares[startRow][rookSrcCol]) m_squares[startRow][rookSrcCol]->setPosition(startRow, rookSrcCol);
            m_squares[startRow][rookDstCol] = std::move(rookCaptured);
        }

        m_lastMoveError = "Move would leave king in check";
        return false;
    }

    // Move succeeded logically. Handle special rules on commit.
    // Clear en passant target by default
    m_enPassantTarget = std::make_pair(-1, -1);

    // If pawn moved two squares, set en passant target to the skipped square
    if (isTwoStep)
    {
        m_enPassantTarget = std::make_pair(startRow + dir, startCol);
    }

    // Handle pawn promotion: if pawn reaches last rank
    // NOTE: do not auto-promote here. If mv.isPromotion is true we will
    // defer to the UI (completePromotion) to apply the player's choice.

    // Mark moved flags
    if (m_squares[endRow][endCol]) m_squares[endRow][endCol]->setMoved(true);
    if (isCastling)
    {
        if (m_squares[startRow][rookDstCol]) m_squares[startRow][rookDstCol]->setMoved(true);
    }

    // capturedPiece and capturedEP (if any) will be destroyed when leaving scope
    m_lastMoveError.clear();

    // Mark moved flags on moving piece
    if (m_squares[endRow][endCol]) m_squares[endRow][endCol]->setMoved(true);
    if (isCastling)
    {
        if (m_squares[startRow][rookDstCol]) m_squares[startRow][rookDstCol]->setMoved(true);
    }

    // Clear redo stack because new move invalidates future
    m_redoStack.clear();

    // Push move onto history
    m_moveHistory.push_back(mv);

    // If this move is a promotion, wait for player choice before finalizing turn
    if (mv.isPromotion)
    {
        m_isAwaitingPromotion = true;
        m_pendingPromotionSquare = { endRow, endCol };
        // Do not toggle turn yet; completion will toggle
    }
    else
    {
        // Toggle turn
        m_currentTurn = (m_currentTurn == PieceColor::White) ? PieceColor::Black : PieceColor::White;
    }

    return true;
}

bool Board::wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol)
{
    // Save internal mutable state
    auto savedHistory = m_moveHistory;
    auto savedRedo = m_redoStack;
    auto savedEnPassant = m_enPassantTarget;
    auto savedTurn = m_currentTurn;
    auto savedLastError = m_lastMoveError;
    bool savedAwaiting = m_isAwaitingPromotion;
    auto savedPending = m_pendingPromotionSquare;

    const Piece* p = at(startRow, startCol);
    if (!p) return false;

    // Ensure movePiece sees the correct turn for the mover
    m_currentTurn = p->color();

    bool moved = movePiece(startRow, startCol, endRow, endCol);
    if (moved)
    {
        // Undo the simulated move
        undoMove();
    }

    // Restore saved state to avoid any side-effects
    m_moveHistory = savedHistory;
    m_redoStack = savedRedo;
    m_enPassantTarget = savedEnPassant;
    m_currentTurn = savedTurn;
    m_lastMoveError = savedLastError;
    m_isAwaitingPromotion = savedAwaiting;
    m_pendingPromotionSquare = savedPending;

    return moved;
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

bool Board::isAwaitingPromotion() const
{
    return m_isAwaitingPromotion;
}

std::pair<int,int> Board::getPendingPromotionSquare() const
{
    return m_pendingPromotionSquare;
}

void Board::completePromotion(PieceType chosenType)
{
    if (!m_isAwaitingPromotion || m_moveHistory.empty()) return;

    ChessMove &mv = m_moveHistory.back();
    if (!mv.isPromotion) return;

    // Apply chosen promotion type to the piece on the board
    int r = mv.r2, c = mv.c2;
    if (m_squares[r][c])
    {
        m_squares[r][c]->setType(chosenType);
    }

    // Record chosen promotion in history
    mv.promotionChoice = chosenType;

    // Clear awaiting state and finalize move: toggle turn
    m_isAwaitingPromotion = false;
    m_pendingPromotionSquare = {-1,-1};

    // Mark moved flag for the piece
    if (m_squares[r][c]) m_squares[r][c]->setMoved(true);

    // Toggle turn now
    m_currentTurn = (m_currentTurn == PieceColor::White) ? PieceColor::Black : PieceColor::White;

    m_lastMoveError.clear();
}

void Board::undoMove()
{
    if (m_moveHistory.empty()) return;
    ChessMove mv = m_moveHistory.back();
    m_moveHistory.pop_back();

    // Move piece back from r2,c2 to r1,c1
    std::unique_ptr<Piece> moving = std::move(m_squares[mv.r2][mv.c2]);
    if (moving)
    {
        m_squares[mv.r1][mv.c1] = std::move(moving);
        if (m_squares[mv.r1][mv.c1]) m_squares[mv.r1][mv.c1]->setPosition(mv.r1, mv.c1);
        // If promotion occurred, revert type back to pawn
        if (mv.isPromotion)
        {
            m_squares[mv.r1][mv.c1]->setType(PieceType::Pawn);
        }
        // restore moved flag
        m_squares[mv.r1][mv.c1]->setMoved(mv.movedPieceHadMoved);
    }

    // Restore captured piece if any
    if (mv.capturedType.has_value())
    {
        int cr = mv.isEnPassant ? mv.r1 : mv.r2;
        int cc = mv.isEnPassant ? mv.c2 : mv.c2;
        m_squares[cr][cc] = std::make_unique<Piece>(mv.capturedType.value(), mv.capturedColor.value(), cr, cc);
    }

    // If castling, move rook back
    if (mv.isCastling)
    {
        // rook currently at rookDstCol on mv.r1
        m_squares[mv.r1][mv.rookSrcCol] = std::move(m_squares[mv.r1][mv.rookDstCol]);
        if (m_squares[mv.r1][mv.rookSrcCol]) m_squares[mv.r1][mv.rookSrcCol]->setPosition(mv.r1, mv.rookSrcCol);
        if (m_squares[mv.r1][mv.rookSrcCol]) m_squares[mv.r1][mv.rookSrcCol]->setMoved(mv.rookHadMoved);
    }

    // Restore en passant target
    m_enPassantTarget = mv.enPassantBefore;

    // Set turn back to moving player
    m_currentTurn = mv.movedColor;

    // Push onto redo stack
    m_redoStack.push_back(mv);
}

void Board::redoMove()
{
    if (m_redoStack.empty()) return;
    ChessMove mv = m_redoStack.back();
    m_redoStack.pop_back();

    // Remove any captured piece at destination (normal capture)
    if (mv.capturedType.has_value() && !mv.isEnPassant)
    {
        m_squares[mv.r2][mv.c2] = nullptr;
    }

    // Handle en passant capture removal
    if (mv.isEnPassant)
    {
        m_squares[mv.r1][mv.c2] = nullptr; // captured pawn
    }

    // Move piece from r1,c1 to r2,c2
    m_squares[mv.r2][mv.c2] = std::move(m_squares[mv.r1][mv.c1]);
    if (m_squares[mv.r2][mv.c2]) m_squares[mv.r2][mv.c2]->setPosition(mv.r2, mv.c2);

    // Handle castling rook move
    if (mv.isCastling)
    {
        m_squares[mv.r1][mv.rookDstCol] = std::move(m_squares[mv.r1][mv.rookSrcCol]);
        if (m_squares[mv.r1][mv.rookDstCol]) m_squares[mv.r1][mv.rookDstCol]->setPosition(mv.r1, mv.rookDstCol);
    }

    // Handle promotion
    if (mv.isPromotion)
    {
        if (m_squares[mv.r2][mv.c2])
        {
            PieceType chosen = mv.promotionChoice.has_value() ? mv.promotionChoice.value() : PieceType::Queen;
            m_squares[mv.r2][mv.c2]->setType(chosen);
        }
    }

    // Set moved flags
    if (m_squares[mv.r2][mv.c2]) m_squares[mv.r2][mv.c2]->setMoved(true);
    if (mv.isCastling && m_squares[mv.r1][mv.rookDstCol]) m_squares[mv.r1][mv.rookDstCol]->setMoved(true);

    // Set en passant target after redo (if pawn two-step)
    if (mv.movedType == PieceType::Pawn && std::abs(mv.r2 - mv.r1) == 2)
    {
        m_enPassantTarget = std::make_pair((mv.r1 + mv.r2) / 2, mv.c1);
    }
    else
    {
        m_enPassantTarget = std::make_pair(-1, -1);
    }

    // Toggle turn to opponent
    m_currentTurn = (mv.movedColor == PieceColor::White) ? PieceColor::Black : PieceColor::White;

    // Push move back onto history
    m_moveHistory.push_back(mv);
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
