#include "Board.h"

#include <cassert>
#include <utility>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <sstream>

Board::Board()
{
    for (int r = 0; r < Tiles; ++r)
        for (int c = 0; c < Tiles; ++c)
            m_squares[r][c].reset();
    m_currentTurn = PieceColor::White;
    m_lastMoveError.clear();
    m_enPassantTarget = std::make_pair(-1, -1);
    // Default starting castling rights (both sides)
    m_castlingRights = (uint8_t)(Board::CR_WHITE_K | Board::CR_WHITE_Q | Board::CR_BLACK_K | Board::CR_BLACK_Q);
}

void Board::setLastMoveSAN(const std::string& san)
{
    if (!m_moveHistory.empty())
    {
        m_moveHistory.back().san = san;
        m_lastMove = m_moveHistory.back();
    }
}

std::string Board::getFullPGNText() const
{
    std::ostringstream out;
    int moveNumber = 1;
    for (size_t i = 0; i < m_moveHistory.size(); ++i)
    {
        const ChessMove &mv = m_moveHistory[i];
        if (mv.movedColor == PieceColor::White)
        {
            if (i != 0) out << ' ';
            out << moveNumber << ". ";
            if (!mv.san.empty()) out << mv.san;
            else out << "?";
        }
        else
        {
            out << ' ';
            if (!mv.san.empty()) out << mv.san;
            else out << "?";
            moveNumber++;
        }
    }
    return out.str();
}

static char pieceLetter(PieceType t)
{
    switch (t)
    {
    case PieceType::King: return 'K';
    case PieceType::Queen: return 'Q';
    case PieceType::Rook: return 'R';
    case PieceType::Bishop: return 'B';
    case PieceType::Knight: return 'N';
    default: return '\0';
    }
}

static std::string squareName(int r, int c)
{
    std::string s;
    s.push_back((char)('a' + c));
    s.push_back((char)('0' + (8 - r)));
    return s;
}

std::string Board::moveToSAN(const ChessMove& move)
{
    std::ostringstream out;

    // Castling
    if (move.isCastling)
    {
        // kingside if dst col > src col
        if (move.c2 > move.c1) return std::string("O-O");
        return std::string("O-O-O");
    }

    PieceType pt = move.movedType;
    char pLetter = pieceLetter(pt);

    bool isCapture = move.capturedType.has_value() || move.isEnPassant;

    if (pt == PieceType::Pawn)
    {
        if (isCapture)
        {
            out << (char)('a' + move.c1) << 'x' << squareName(move.r2, move.c2);
        }
        else
        {
            out << squareName(move.r2, move.c2);
        }

        // Promotion
        if (move.isPromotion)
        {
            char promo = 'Q';
            if (move.promotionChoice.has_value())
            {
                promo = pieceLetter(move.promotionChoice.value());
                if (promo == '\0') promo = 'Q';
            }
            out << '=' << promo;
        }
    }
    else
    {
        // Determine disambiguation
        bool needFile = false;
        bool needRank = false;

        // Search for other pieces of same type & color that could move to target
        int candidates = 0;
        for (int r = 0; r < Tiles; ++r)
        {
            for (int c = 0; c < Tiles; ++c)
            {
                if (r == move.r1 && c == move.c1) continue;
                const Piece* p = at(r, c);
                if (!p) continue;
                if (p->type() != pt) continue;
                if (p->color() != move.movedColor) continue;

                // Quick movement rule check
                if (!p->isValidMove(move.r2, move.c2, *this)) continue;

                // Use wouldMoveBeLegal to ensure legality (checks for checks)
                if (wouldMoveBeLegal(r, c, move.r2, move.c2))
                {
                    candidates++;
                    if (c != move.c1) needFile = true;
                    if (r != move.r1) needRank = true;
                }
            }
        }

        out << pLetter;

        if (candidates > 0)
        {
            if (needFile && !needRank) out << (char)('a' + move.c1);
            else if (!needFile && needRank) out << (char)('1' + (7 - move.r1));
            else if (needFile && needRank) out << (char)('a' + move.c1) << (char)('1' + (7 - move.r1));
            else out << (char)('a' + move.c1); // fallback
        }

        if (isCapture) out << 'x';
        out << squareName(move.r2, move.c2);

        // Promotion unlikely for non-pawn; ignore
    }

    // Determine check or checkmate after move: current turn is opponent because move already toggled
    Board::GameState gs = getGameState();
    if (gs == GameState::Checkmate)
    {
        out << '#';
    }
    else
    {
        if (isInCheck(getCurrentTurn())) out << '+';
    }

    return out.str();
}

//void Board::runFENTests()
//{
//    const std::vector<std::string> tests = {
//        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
//        "r1bqkbnr/pppp1ppp/2n5/3Pp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 3",
//        "8/4P3/8/8/8/8/k6K/8 w - - 0 50",
//        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"
//    };
//
//    for (const auto &fen : tests)
//    {
//        bool ok = loadFromFEN(fen);
//        std::string out = getFEN();
//        if (!ok)
//        {
//            std::cout << "FEN TEST FAIL (parse): " << fen << std::endl;
//        }
//        else if (out == fen)
//        {
//            std::cout << "FEN TEST PASS: " << fen << std::endl;
//        }
//        else
//        {
//            std::cout << "FEN TEST FAIL (round-trip)\n  expected: " << fen << "\n  got:      " << out << std::endl;
//        }
//    }
//}

std::string Board::getFEN() const
{
    // Piece placement
    std::string fen;
    for (int r = 0; r < Tiles; ++r)
    {
        int empty = 0;
        for (int c = 0; c < Tiles; ++c)
        {
            const Piece* p = at(r, c);
            if (!p)
            {
                empty++;
            }
            else
            {
                if (empty > 0) { fen.push_back('0' + empty); empty = 0; }
                char ch = 'p';
                switch (p->type())
                {
                case PieceType::King: ch = 'k'; break;
                case PieceType::Queen: ch = 'q'; break;
                case PieceType::Rook: ch = 'r'; break;
                case PieceType::Bishop: ch = 'b'; break;
                case PieceType::Knight: ch = 'n'; break;
                case PieceType::Pawn: ch = 'p'; break;
                }
                if (p->color() == PieceColor::White) ch = (char)toupper(ch);
                fen.push_back(ch);
            }
        }
        if (empty > 0) { fen.push_back('0' + empty); }
        if (r != Tiles - 1) fen.push_back('/');
    }

    // Active color
    fen.push_back(' ');
    fen.push_back((m_currentTurn == PieceColor::White) ? 'w' : 'b');

    // Castling availability from board-level castling rights
    fen.push_back(' ');
    std::string castling;
    if (m_castlingRights & Board::CR_WHITE_K) castling.push_back('K');
    if (m_castlingRights & Board::CR_WHITE_Q) castling.push_back('Q');
    if (m_castlingRights & Board::CR_BLACK_K) castling.push_back('k');
    if (m_castlingRights & Board::CR_BLACK_Q) castling.push_back('q');
    if (castling.empty()) fen.push_back('-'); else fen += castling;

    // En passant target
    fen.push_back(' ');
    if (m_enPassantTarget.first < 0)
    {
        fen.push_back('-');
    }
    else
    {
        int er = m_enPassantTarget.first;
        int ec = m_enPassantTarget.second;
        char file = 'a' + ec;
        char rank = '0' + (8 - er);
        fen.push_back(file);
        fen.push_back(rank);
    }

    // Halfmove and fullmove
    fen.push_back(' ');
    fen += std::to_string(m_halfmoveClock);
    fen.push_back(' ');
    fen += std::to_string(m_fullmoveNumber);

    return fen;
}

bool Board::loadFromFEN(const std::string& fen)
{
    // Parse into temporaries first. If any parse error occurs, abort and
    // leave the current board untouched.
    std::vector<std::string> parts;
    {
        std::string tmp;
        for (char ch : fen)
        {
            if (ch == ' ')
            {
                if (!tmp.empty()) { parts.push_back(tmp); tmp.clear(); }
            }
            else tmp.push_back(ch);
        }
        if (!tmp.empty()) parts.push_back(tmp);
    }
    if (parts.size() != 6) return false;

    // Temporary storage for squares
    std::unique_ptr<Piece> newSquares[Tiles][Tiles];
    // Initialize to null
    for (int rr = 0; rr < Tiles; ++rr) for (int cc = 0; cc < Tiles; ++cc) newSquares[rr][cc].reset();

    // Piece placement
    const std::string &placement = parts[0];
    std::vector<std::string> ranks;
    {
        std::string cur;
        for (char ch : placement)
        {
            if (ch == '/') { ranks.push_back(cur); cur.clear(); }
            else cur.push_back(ch);
        }
        if (!cur.empty()) ranks.push_back(cur);
    }
    if (ranks.size() != 8) return false;

    for (int rr = 0; rr < 8; ++rr)
    {
        const std::string &rowStr = ranks[rr];
        int c = 0;
        for (char ch : rowStr)
        {
            if (std::isdigit((unsigned char)ch))
            {
                int skip = ch - '0';
                if (skip <= 0 || skip > 8) return false;
                c += skip;
            }
            else
            {
                if (c < 0 || c >= Tiles) return false;
                if (!std::isalpha((unsigned char)ch)) return false;
                PieceColor color = (std::isupper((unsigned char)ch)) ? PieceColor::White : PieceColor::Black;
                char lower = (char)std::tolower((unsigned char)ch);
                PieceType type = PieceType::Pawn;
                switch (lower)
                {
                case 'k': type = PieceType::King; break;
                case 'q': type = PieceType::Queen; break;
                case 'r': type = PieceType::Rook; break;
                case 'b': type = PieceType::Bishop; break;
                case 'n': type = PieceType::Knight; break;
                case 'p': type = PieceType::Pawn; break;
                default: return false;
                }
                int rowIndex = rr; // rr=0 is top rank (8)
                newSquares[rowIndex][c] = std::make_unique<Piece>(type, color, rowIndex, c);
                c++;
            }
        }
        if (c != Tiles) return false;
    }

    // Active color
    PieceColor newTurn = (parts[1].size() > 0 && parts[1][0] == 'b') ? PieceColor::Black : PieceColor::White;

    // Castling availability
    std::string castling = parts[2];
    uint8_t newCastling = 0;
    if (castling == "-") { /* none */ }
    else
    {
        for (char ch : castling)
        {
            if (ch == 'K') newCastling |= CR_WHITE_K;
            else if (ch == 'Q') newCastling |= CR_WHITE_Q;
            else if (ch == 'k') newCastling |= CR_BLACK_K;
            else if (ch == 'q') newCastling |= CR_BLACK_Q;
            else return false; // invalid char
        }
    }

    // En passant target
    std::pair<int,int> newEP = std::make_pair(-1, -1);
    const std::string &ep = parts[3];
    if (ep == "-") { /* none */ }
    else if (ep.size() == 2)
    {
        char file = ep[0];
        char rank = ep[1];
        if (file < 'a' || file > 'h') return false;
        if (rank < '1' || rank > '8') return false;
        int ec = file - 'a';
        int er = 8 - (rank - '0');
        if (er < 0 || er >= 8) return false;
        newEP = std::make_pair(er, ec);
    }
    else return false;

    // Halfmove and fullmove
    int newHalf = 0;
    int newFull = 1;
    try { newHalf = std::stoi(parts[4]); } catch (...) { return false; }
    try { newFull = std::stoi(parts[5]); } catch (...) { return false; }

    // All parsing succeeded; commit to board atomically
    // Clear existing board
    for (int rr = 0; rr < Tiles; ++rr) for (int cc = 0; cc < Tiles; ++cc) m_squares[rr][cc].reset();
    // Move new pieces into place
    for (int rr = 0; rr < Tiles; ++rr)
    {
        for (int cc = 0; cc < Tiles; ++cc)
        {
            if (newSquares[rr][cc])
            {
                m_squares[rr][cc] = std::move(newSquares[rr][cc]);
            }
            else
            {
                m_squares[rr][cc].reset();
            }
        }
    }

    // Commit other state
    m_moveHistory.clear();
    m_redoStack.clear();
    m_lastMove.reset();
    m_isAwaitingPromotion = false;
    m_currentTurn = newTurn;
    m_castlingRights = newCastling;
    m_enPassantTarget = newEP;
    m_halfmoveClock = newHalf;
    m_fullmoveNumber = newFull;

    return true;
}

bool Board::hasLegalMoves(PieceColor color) const
{
    // Create a temporary Board initialized from the current FEN and perform
    // the non-const simulation on that copy. This avoids mutating the const
    // object while remaining reasonably fast (FEN round-trip is lightweight).
    Board tmp;
    // If FEN round-trip fails for any reason, conservatively report no legal moves.
    if (!tmp.loadFromFEN(this->getFEN())) return false;
    return tmp.hasLegalMoves(color);
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
    // Save castling rights
    uint8_t savedCastling = m_castlingRights;

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

                    MoveResult movedRes = movePiece(sr, sc, tr, tc);
                    if (movedRes != MoveResult::Invalid)
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
                        m_castlingRights = savedCastling;

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
                    m_castlingRights = savedCastling;
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

Board::MoveResult Board::movePiece(int startRow, int startCol, int endRow, int endCol)
{
    // Basic bounds
    if (startRow < 0 || startRow >= Tiles || startCol < 0 || startCol >= Tiles)
    {
        m_lastMoveError = "Invalid source coordinates";
        return MoveResult::Invalid;
    }
    if (endRow < 0 || endRow >= Tiles || endCol < 0 || endCol >= Tiles)
    {
        m_lastMoveError = "Invalid destination coordinates";
        return MoveResult::Invalid;
    }

    Piece* p = at(startRow, startCol);
    if (!p)
    {
        m_lastMoveError = "No piece at source square";
        return MoveResult::Invalid; // no piece to move
    }

    // Check turn
    if (p->color() != m_currentTurn)
    {
        m_lastMoveError = "Not your turn";
        return MoveResult::Invalid;
    }

    // Ensure the piece's movement rules allow this move
    if (!p->isValidMove(endRow, endCol, *this))
    {
        m_lastMoveError = "Invalid move (movement rules or blocked)";
        return MoveResult::Invalid;
    }

    const Piece* dest = at(endRow, endCol);
    // Disallow capturing own piece (isValidMove should already check, but double-check)
    if (dest && dest->color() == p->color())
    {
        m_lastMoveError = "Cannot capture your own piece";
        return MoveResult::Invalid;
    }

    // Get piece and moving type/color early
    Piece* piecePtr = m_squares[startRow][startCol].get();
    if (!piecePtr)
    {
        m_lastMoveError = "No piece at source square";
        return MoveResult::Invalid;
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
    // Record castling rights before the move
    mv.castlingBefore = m_castlingRights;
    // Record clocks before the move
    mv.halfmoveClockBefore = m_halfmoveClock;
    mv.fullmoveNumberBefore = m_fullmoveNumber;

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

    // Update board-level castling rights based on this move/capture
    uint8_t newRights = mv.castlingBefore;
    // If a king moved, clear both rights for that color
    if (movingType == PieceType::King)
    {
        if (movingColor == PieceColor::White) newRights &= ~(Board::CR_WHITE_K | Board::CR_WHITE_Q);
        else newRights &= ~(Board::CR_BLACK_K | Board::CR_BLACK_Q);
    }
    // If a rook moved from a corner, clear the corresponding right
    if (movingType == PieceType::Rook)
    {
        if (startRow == 7 && startCol == 7) newRights &= ~Board::CR_WHITE_K;
        if (startRow == 7 && startCol == 0) newRights &= ~Board::CR_WHITE_Q;
        if (startRow == 0 && startCol == 7) newRights &= ~Board::CR_BLACK_K;
        if (startRow == 0 && startCol == 0) newRights &= ~Board::CR_BLACK_Q;
    }
    // If a rook was captured on a corner, clear the corresponding right
    if (mv.capturedType.has_value() && mv.capturedType.value() == PieceType::Rook)
    {
        int cr = endRow;
        int cc = endCol;
        // Note: en-passant cannot capture a rook
        if (cr == 7 && cc == 7) newRights &= ~Board::CR_WHITE_K;
        if (cr == 7 && cc == 0) newRights &= ~Board::CR_WHITE_Q;
        if (cr == 0 && cc == 7) newRights &= ~Board::CR_BLACK_K;
        if (cr == 0 && cc == 0) newRights &= ~Board::CR_BLACK_Q;
    }
    mv.castlingAfter = newRights;
    m_castlingRights = newRights;

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
        return MoveResult::Invalid;
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
    // Record last committed move
    m_lastMove = mv;

    // Update clocks incrementally
    if (isPawn || mv.capturedType.has_value())
    {
        m_halfmoveClock = 0;
    }
    else
    {
        m_halfmoveClock = mv.halfmoveClockBefore + 1;
    }

    // Increment fullmove number only after Black completes a move (and only if move fully completed — not awaiting promotion)
    if (!mv.isPromotion)
    {
        if (movingColor == PieceColor::Black)
            m_fullmoveNumber = mv.fullmoveNumberBefore + 1;
        else
            m_fullmoveNumber = mv.fullmoveNumberBefore;
    }
    else
    {
        // For a pending promotion, do not increment fullmoveNumber yet
        m_fullmoveNumber = mv.fullmoveNumberBefore;
    }

    // If this move is a promotion, wait for player choice before finalizing turn
    if (mv.isPromotion)
    {
        m_isAwaitingPromotion = true;
        m_pendingPromotionSquare = { endRow, endCol };
        // Do not toggle turn yet; completion will toggle
        return MoveResult::Promotion;
    }

    // Toggle turn
    m_currentTurn = (m_currentTurn == PieceColor::White) ? PieceColor::Black : PieceColor::White;

    // If this move results in the opponent being in check, prioritize that result
    if (isInCheck(m_currentTurn))
    {
        return MoveResult::Check;
    }

    if (mv.isCastling)
    {
        return MoveResult::Castle;
    }

    if (mv.capturedType.has_value())
    {
        return MoveResult::Capture;
    }

    return MoveResult::Normal;
}

bool Board::wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol) const
{
    // Save internal mutable state
    // Implementation cannot modify member state because this method is const.
    // Use a temporary board reconstructed from FEN to simulate the move.
    Board tmp;
    if (!tmp.loadFromFEN(this->getFEN())) return false;
    const Piece* p = tmp.at(startRow, startCol);
    if (!p) return false;
    tmp.setLastMoveSAN(""); // no-op but keeps parity with non-const flow
    MoveResult res = tmp.movePiece(startRow, startCol, endRow, endCol);
    return (res != MoveResult::Invalid);
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

std::optional<Board::ChessMove> Board::getLastMove() const
{
    return m_lastMove;
}

// (Removed updateClocksFromHistory — clocks are now updated incrementally per-move)

Board::MoveResult Board::completePromotion(PieceType chosenType)
{
    if (!m_isAwaitingPromotion || m_moveHistory.empty()) return MoveResult::Invalid;

    ChessMove &mv = m_moveHistory.back();
    if (!mv.isPromotion) return MoveResult::Invalid;

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

    // Update last move record now that promotion choice is known
    if (!m_moveHistory.empty()) m_lastMove = m_moveHistory.back();

    m_lastMoveError.clear();

    return MoveResult::Promotion;
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

    // Restore castling rights to before this move
    m_castlingRights = mv.castlingBefore;

    // Restore clocks
    m_halfmoveClock = mv.halfmoveClockBefore;
    m_fullmoveNumber = mv.fullmoveNumberBefore;

    // Update last move to most recent in history (or clear if none)
    if (!m_moveHistory.empty()) m_lastMove = m_moveHistory.back(); else m_lastMove.reset();
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

    // Restore castling rights to after this move
    m_castlingRights = mv.castlingAfter;

    // Reapply clocks for this move
    if (mv.movedType == PieceType::Pawn || mv.capturedType.has_value())
    {
        m_halfmoveClock = 0;
    }
    else
    {
        m_halfmoveClock = mv.halfmoveClockBefore + 1;
    }

    if (mv.isPromotion)
    {
        // promotion keeps fullmove number unchanged until completion
        m_fullmoveNumber = mv.fullmoveNumberBefore;
    }
    else
    {
        m_fullmoveNumber = (mv.movedColor == PieceColor::Black) ? (mv.fullmoveNumberBefore + 1) : mv.fullmoveNumberBefore;
    }

    // Update last move to this redo'd move
    m_lastMove = mv;
}

PieceColor Board::getCurrentTurn() const
{
    return m_currentTurn;
}

uint8_t Board::getCastlingRights() const
{
    return m_castlingRights;
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
    // Use transactional FEN loader to initialize standard starting position.
    // This ensures the board state, clocks, castling rights and en-passant
    // target are all set consistently and atomically.
    const std::string startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    bool ok = loadFromFEN(startFEN);

    // Ensure any remaining runtime state is reset for a fresh game.
    if (ok)
    {
        m_lastMoveError.clear();
        m_isAwaitingPromotion = false;
        m_pendingPromotionSquare = {-1, -1};
        // move history/redo cleared by loadFromFEN; ensure lastMove cleared
        m_lastMove.reset();
    }
    else
    {
        // Fallback: clear board in case loading failed
        for (int r = 0; r < Tiles; ++r)
            for (int c = 0; c < Tiles; ++c)
                m_squares[r][c].reset();
        m_currentTurn = PieceColor::White;
        m_castlingRights = (uint8_t)(CR_WHITE_K | CR_WHITE_Q | CR_BLACK_K | CR_BLACK_Q);
        m_enPassantTarget = std::make_pair(-1, -1);
        m_halfmoveClock = 0;
        m_fullmoveNumber = 1;
        m_moveHistory.clear();
        m_redoStack.clear();
        m_lastMove.reset();
        m_lastMoveError.clear();
        m_isAwaitingPromotion = false;
        m_pendingPromotionSquare = {-1, -1};
    }
}

Board::ChessMove Board::parseEngineMove(const std::string& moveStr)
{
    ChessMove move = {};

    // Validate input length: must be 4 or 5 characters
    // 4 chars: standard move (e.g., "e2e4")
    // 5 chars: promotion move (e.g., "e7e8q") - 5th character ignored here
    if (moveStr.length() < 4 || moveStr.length() > 5)
    {
        return move; // Return empty/default move on invalid input
    }

    // Parse source square (first 2 characters)
    char srcFile = moveStr[0];
    char srcRank = moveStr[1];

    // Validate file and rank are within valid range
    if (srcFile < 'a' || srcFile > 'h' || srcRank < '1' || srcRank > '8')
    {
        return move;
    }

    // Convert file ('a'-'h') to column (0-7)
    int c1 = srcFile - 'a';
    // Convert rank ('1'-'8') to row (7-0)
    // Since rank '1' is at the bottom and our grid has row 0 at top (rank 8),
    // we use: row = 8 - (rank - '0') = 8 - rank + '0' = '8' - rank
    int r1 = 8 - (srcRank - '0');

    // Parse destination square (characters at index 2 and 3)
    char dstFile = moveStr[2];
    char dstRank = moveStr[3];

    // Validate destination file and rank
    if (dstFile < 'a' || dstFile > 'h' || dstRank < '1' || dstRank > '8')
    {
        return move;
    }

    // Convert destination file and rank using same logic
    int c2 = dstFile - 'a';
    int r2 = 8 - (dstRank - '0');

    // Populate the move object with coordinates
    move.r1 = r1;
    move.c1 = c1;
    move.r2 = r2;
    move.c2 = c2;

    // Note: The 5th character (if present) indicating promotion choice
    // (e.g., 'q', 'r', 'b', 'n') is not processed here.
    // The caller should handle promotion choice separately via completePromotion().

    return move;
}
