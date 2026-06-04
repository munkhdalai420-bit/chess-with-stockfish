# <img src="./assets/icons/chess-icon.png" width="24" height="24" />Advanced Multi-Threaded Chess Application

An interactive, high-performance desktop Chess application built from scratch in C++ using the **Raylib** immediate-mode GUI framework and featuring asynchronous **Stockfish AI** integration. 

This project goes beyond traditional turn-based chess games by executing engine evaluation and depth searches in an isolated native background worker thread. This ensures the immediate-mode rendering loop remains completely fluid, responsive, and stutter-free.

## Key Features

* 🤖 **Asynchronous Stockfish Engine:** Integrated Stockfish UCI engine operating on a separate background thread via non-blocking OS pipe streams.
* 🎚️ **Dynamic ELO Selection:** Real-time difficulty adjustment ranging through distinct rating profiles (500 -> 1000 -> 1500 -> 2000).
* 🔄 **Bi-directional State Traversal:** Full match timeline history management allowing infinite Undo/Redo cycles and continuous SAN (Standard Algebraic Notation) logging.
* 💾 **Disk-Backed State Persistence:** Robust custom serialization tracking complete board parameters and timeline arrays to a local file (`latest_match.txt`).
* 🎨 **Immediate-Mode Custom UI:** Fully dynamic layout scaling incorporating real-time engine evaluation bars, color theme shifting, piece move animations, and responsive side control panels.

---

## Component Architecture Overview

```text
       ┌─────────────────────────────────────────────────────────┐
       │                     GameController                      │
       │  (State Machine, Core Coordination & Rules Mediation)   │
       └───────────┬─────────────────────────┬───────────────────┘
                   │                         │
                   │ Composition             │ Composition
                   ▼                         ▼
       ┌───────────────────────┐ ┌───────────────────────────────┐
       │         Board         │ │         EngineManager         │
       │ (Pure Data Model/FEN) │ │  (Asynchronous Stockfish IPC) │
       └───────────────────────┘ └───────────────────────────────┘
                   ▲
                   │ Dependency 
                   │ (Reads State via Const Reference)
                   │
       ┌───────────┴───────────┐
       │       Renderer        │
       │ (Immediate-Mode UI)   │
       └───────────────────────┘
```

---

## Class Interfaces & Member Specifications

### 1. Class: `GameController`
Manages the application state machine, game loop coordination, and serves as the logic mediator between the user interface and backend services.

#### Public Member Functions
* `GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth, Renderer* renderer)`
    * Constructs the controller and initializes game, audio, engine, and UI layout states.
* `~GameController()`
    * Destructor; coordinates an orderly shutdown of the Stockfish engine process and terminates the audio context.
* `int getTargetElo() const`
    * Returns the currently selected target Elo constraint level.
* `void cycleTargetElo()`
    * Cycles to the next available Elo setting (500 -> 1000 -> 1500 -> 2000) and updates engine configurations.
* `bool isMatchStarted() const`
    * Returns whether a chess match is currently active.
* `void startMatch()`
    * Resets the board model, initializes a new match state, and engages the AI thread if the human selects Black.
* `void endMatch()`
    * Terminates the current match state and resets the controller/UI context back to the main lobby menu.
* `void undo()`
    * Undoes one ply (half-move) from the timeline and places the system into history review mode.
* `void redo()`
    * Redoes one ply forward from the timeline history stack and updates the review mode state.
* `void goToHistoryIndex(size_t index)`
    * Performs arbitrary random-access navigation to a specific frame pointer index in the match history vector.
* `float getDisplayedEvaluation() const`
    * Returns the precise positional evaluation score corresponding to the currently viewed history index frame.
* `bool isMateDetected() const`
    * Queries whether the background engine parsing thread has flagged an impending checkmate condition.
* `int getMateInMoves() const`
    * Retrieves the numerical distance (in plies) to the detected checkmate reported by the engine.
* `bool canUndo() const`
    * Condition check tracking whether an undo command is valid (engine must be idle and `historyIndex > 0`).
* `bool canRedo() const`
    * Condition check tracking whether a redo command is valid (engine must be idle and forward timeline steps exist).
* `void update()`
    * Processes structural keyboard/mouse inputs and advances core game variables; invoked once per frame tick.
* `void requestHint()`
    * Dispatches a localized, sub-second engine search request to generate a visual hint move.
* `void saveLatestGame()`
    * Serializes the current match matrix into `latest_match.txt` (records player alignment and the raw UCI move sequence).
* `void loadLatestGame()`
    * Parses and replays a stored match configuration directly from the local disk-backed text file.
* `void clearActiveHint()`
    * Cancels any active hint search sequence and flushes the visual hint coordinate buffers.
* `bool canRequestHint() const`
    * Validates whether a hint request is legal (engine must be idle, match started, and it must be the human's turn).
* `std::string getHintMove() const`
    * Returns the last computed hint move in standard LAN/UCI string format, or an empty string if unallocated.
* `Board& getBoard()`
    * Exposes a mutable reference to the underlying structural data board matrix.
* `std::optional<std::pair<int,int>> getSelected() const`
    * Returns the array coordinates `[row, col]` of the currently selected square, or `std::nullopt` if unselected.
* `std::string getMoveMessage() const`
    * Retrieves transient UI notification text defining movement error feedbacks.
* `float getMessageTimer() const`
    * Returns the frame duration remaining for displaying the transient error text overlay.
* `GameController::PlayerColor getPlayerColor() const`
    * Accesses the alignment state setting assigned to the human participant.
* `void setPlayerColor(GameController::PlayerColor c)`
    * Force-assigns the human participant's alignment state.
* `void togglePlayerColor()`
    * Inverts the player side variable between White and Black alignments.
* `bool isEngineIdle() const`
    * Evaluates true if the background calculation worker thread is in an unengaged, non-blocking standby state.
* `bool isMatchEnded() const`
    * Evaluates true if a terminal state (Checkmate/Draw) has frozen the match clock and shifted the system to post-match review.

#### Private Data Members
* `Board m_board` — In-memory object representation of the active chess grid and its legal validator routines.
* `EngineManager m_engine` — Multi-threaded system subprocess controller handling Stockfish execution contexts.
* `EngineMode m_engineMode` — Tracked enumerated status flag (`Idle`, `AI_Thinking`, `Hint_Calculating`).
* `std::string m_hintMove` — Cached coordinate markers defining the current calculated hint target.
* `bool m_isReviewingHistory` — Gate variable disabling live piece manipulation overlays during history scrubs.
* `std::optional<std::pair<int,int>> m_selected` — Coordinate tracker capturing selected piece indices.
* `std::vector<float> m_evaluationHistory` — Vector array saving sequential numerical evaluation values per ply.
* `size_t m_historyIndex` — Step pointer capturing the current frame being visually displayed.
* `int m_targetElo` — Target engine strength configuration limit variable.
* `PlayerColor m_playerColor` — Perspective color assignment setting assigned to the user.
* `int m_timePerMoveMs` — Maximum execution time allowed for engine searching cycles before cut-off triggers.
* `int m_eloIndex` — Array offset variable mapping choice positions within static ELO data options.
* `bool m_gameStarted` — Flag state recording whether a match is running inside the core loop bounds.
* `bool m_matchEnded` — Execution state marker shifting input paths into decoupled review behaviors.
* `std::string m_moveMessage` — String storage holding current on-screen error notifications.
* `float m_messageTimer` — Execution lifetime variable tracking the decay path of temporary text blocks.
* `int m_windowWidth` / `m_windowHeight` / `m_tileSize` / `m_sidebarWidth` — Metric canvas layout dimensions.
* `Renderer* m_renderer` — Non-owning raw pointer pointing toward the primary graphical asset worker instance.
* `Sound m_sndCapture` / `m_sndCastle` / `m_sndMoveCheck` / `m_sndMoveSelf` / `m_sndPromote` — Raylib audio context structures.
* `bool m_audioEnabled` — System validation gate recording audio device initialization success.

---

### 2. Class: `Renderer`
Responsible exclusively for layout matrix transforms, drawing assets, color scaling, and localized UI side panel geometry.

#### Public Member Functions
* `Renderer(int windowWidth, int windowHeight, int tileSize)`
    * Constructs layout metrics and establishes pixel origin bounds based on initialization settings.
* `~Renderer()`
    * Destructor; iterates through device memories to cleanly drop fonts, buffers, and texturing sheets.
* `void setTheme(Renderer::BoardTheme theme)`
    * Reconfigures hex color arrays used across canvas loops based on active enum definitions.
* `bool loadTextures()`
    * Binds image asset files directly to map variables; returns true upon asset allocation successes.
* `void render(const Board& board, const std::optional<std::pair<int,int>>& selected, float evaluation, GameController& controller)`
    * Core visual pass function. Maps current class data structures to graphical primitives every frame.
* `void triggerMoveAnimation(int fromX, int fromY, int toX, int toY, char pieceChar, char pieceColor, bool hasSecondary = false, int secFromX = -1, int secToX = -1, char secPieceChar = 'r')`
    * Instantiates frame vector delta variables to smoothly interpolate piece movements across frames.

#### Private Data Members
* `int m_windowWidth` / `m_windowHeight` / `m_tileSize` — Base resolution parameters.
* `int m_boardPixelSize` / `m_boardOriginX` / `m_boardOriginY` — Canvas coordinate mapping markers.
* `int m_sidebarWidth` — Fixed horizontal screen padding allocation reserved for text lists and logs.
* `std::unordered_map<std::string, Texture2D> m_textures` — Unordered memory index mapping textures to asset string keys.
* `Font m_mainFont` — Raylib structure containing loaded TrueType Font assets.
* `BoardTheme m_theme` — Struct parameter defining color schemes across tiles.
* `int m_themeIndex` — Integer index used to cycle selected color arrays.
* `bool m_animatePieces` — State gate controlling asset position interpolations.
* `PieceAnimation m_currentAnim` — Structural wrapper holding positional updates for current asset interpolations.
* `float m_historyScrollOffset` — Delta value tracking vertical mouse scroll offsets inside the sidebar panel.
* `double m_lastHistoryClickTime` — Time parameter recording double click bounds within history listings.
* `int m_lastHistoryClickedIndex` — Index tracking matching historical text bounds to clear click drift.
* `mutable bool m_flip` — Drawing configuration tracker controlling board inversion.

---

### 3. Class: `Board`
Acts as the algorithmic data model and core rules engine. Contains zero visualization components, handling matrix variables, algebraic formatting, and simulation rollbacks.

#### Public Member Functions
* `Board()`
    * Generates a blank layout and sets reference vectors to standard initialization scopes.
* `const Piece* at(int row, int col) const` / `Piece* at(int row, int col)`
    * Matrix address accessor returning references to items at targeted `[row, col]` slots.
* `void initializeStandardSetup()`
    * Configures pointer arrays to clear matching baseline initial setups.
* `MoveResult movePiece(int startRow, int startCol, int endRow, int endCol)`
    * Primary execution command. Adjusts piece matrices, checks rule boundaries, and handles multi-piece anomalies.
* `bool isSquareUnderAttack(int row, int col, PieceColor attackerColor) const`
    * Iterates outward vectors relative to targeted positions to evaluate if threat parameters intersect.
* `bool isInCheck(PieceColor color) const`
    * Identifies active King coordinates to test if threat paths report target intersections.
* `bool hasLegalMoves(PieceColor color) const` / `hasLegalMoves(PieceColor color)`
    * Iterates through available move vectors to check for structural checkmate boundaries.
* `GameState getGameState() const`
    * Returns categorized status trackers identifying terminal game parameters (`Active`, `Checkmate`, `Stalemate`).
* `const std::string& getLastMoveError() const`
    * Exposes precise internal failure tracking strings to support interface logic processing.
* `std::pair<int,int> getEnPassantTarget() const`
    * Returns vector coordinates indicating legal en-passant captures, or `{-1,-1}` if empty.
* `void undoMove()` / `void redoMove()`
    * Reverts recent adjustments or re-applies structural modifications using historical delta parameters.
* `bool wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol) const`
    * Simulates changes across local datasets, testing state validity before triggering automated tracking rollbacks.
* `bool isAwaitingPromotion() const` / `std::pair<int,int> getPendingPromotionSquare() const`
    * Queries active pawn replacement overlays and identifies corresponding coordinate targets.
* `MoveResult completePromotion(PieceType chosenType)`
    * Finalizes pawn replacement procedures by swapping target references with chosen object definitions.
* `std::optional<ChessMove> getLastMove() const`
    * Returns structural tracking data containing the most recent valid move to drive visual interfaces.
* `std::string getFEN() const` / `bool loadFromFEN(const std::string& fen)`
    * Generates or parses standard Forsyth-Edwards Notation (FEN) data strings.
* `std::string moveToSAN(const ChessMove& move)` / `void setLastMoveSAN(const std::string& san)`
    * Maps internal coordinate logs into Standard Algebraic Notation (SAN) character arrays and caches them.
* `std::string getFullPGNText() const`
    * Iterates across state logging history items to compile structured Portable Game Notation (PGN) blocks.
* `std::vector<ChessMove> getMoveHistory() const`
    * Copies historical vector sets out of local boundaries to support parallel visualization checks.
* `ChessMove parseEngineMove(const std::string& moveStr)`
    * Processes standard LAN/UCI strings to initialize native application objects.
* `PieceColor getCurrentTurn() const`
    * Identifies targeted color parameters matching active turn identifiers.
* `uint8_t getCastlingRights() const`
    * Returns a 4-bit bitmask sequence detailing active castling capabilities.

#### Private Data Members
* `std::unique_ptr<Piece> m_squares[Tiles][Tiles]` — Primary matrix data container holding exclusive ownership references of piece assets.
* `PieceColor m_currentTurn` — State parameter mapping current turn parameters.
* `std::string m_lastMoveError` — Internal character cache containing tracking errors.
* `std::pair<int,int> m_enPassantTarget` — Coordinate indices mapping legal en-passant tracking positions.
* `std::vector<ChessMove> m_moveHistory` — Active core vector history mapping committed operational records.
* `std::vector<ChessMove> m_redoStack` — Forward collection storage supporting inverse tracking rollbacks.
* `bool m_isAwaitingPromotion` — Context controller blocking input paths during promotion option selectors.
* `std::pair<int,int> m_pendingPromotionSquare` — Positional coordinates pinning promotion target locations.
* `std::optional<ChessMove> m_lastMove` — Structural reference copy tracing chronological items to update highlighting.
* `int m_halfmoveClock` — Step counter tracking rules required by fifty-move draw logic.
* `int m_fullmoveNumber` — Core engine step tracker appended within FEN state records.
* `uint8_t m_castlingRights` — Functional bitfield configuration holding active castling capability flags.

---

### 4. Class: `EngineManager`
An isolated multi-threaded platform layer that spawns Stockfish inside an independent process, managing concurrent inter-process communication via Win32 pipes.

#### Public Member Functions
* `EngineManager()` / `~EngineManager()`
    * Configures handle tracking references and ensures strict teardown patterns are completed to eliminate stray threads.
* `bool launch(const std::string& path = "")`
    * Instantiates executable subprocess loops and routes asynchronous standard output data streams via operating system anonymous pipe endpoints.
* `bool sendCommand(const std::string& cmd)`
    * Forwards command characters onto active input pipes in a safe, non-blocking configuration.
* `bool startSearch(const std::string& fen, int timeMs)`
    * Transmits layout FEN positions and triggers localized engine search configurations.
* `void stopSearch()`
    * Sends an immediate interrupt override command onto background processing contexts to drop computation calculations.
* `bool checkBestMove(std::string& outMove)`
    * Non-blocking atomic query function. Copies the calculation result string to the output parameter upon background completion.
* `float getEvaluation() const`
    * Returns the most recent scoring valuation, normalized relative to White's positional alignment perspective.
* `void setDifficulty(int elo)`
    * Forces limits across engine search limits matching user choices via standardized UCI interface rules.
* `void clearMateDetection()` / `bool isMateDetected() const` / `int getMateInMoves() const`
    * Accesses or flushes cached evaluations that hold tracking indicators for checkmate vectors and distances.
* `bool isRunning() const`
    * Verification check confirming background threads and process variables report alive states.
* `void shutdown()`
    * Dispatches process exit requests, tears down system pipes, and rejoins multi-threaded execution loops.

#### Private Data Members
* `void* m_hProcess` — Opaque operating system instance tracking references pointing to the background task engine.
* `void* m_hStdinRead` / `m_hStdinWrite` / `m_hStdoutRead` / `m_hStdoutWrite` — Pipe handles tracking internal engine operational input and output pathways.
* `std::thread m_workerThread` — Concurrent processing thread handling continuous blocking line parsing streams.
* `std::atomic<bool> m_isRunning` / `m_shouldShutdown` — Atomic verification flags tracking runtime loops and shutdown procedures.
* `std::mutex m_mutex` — Synchronization lock protecting multi-threaded asset modification actions.
* `std::string m_lastLine` / `m_bestMove` — Text trackers capturing output chunks and strategic pathways.
* `std::atomic<bool> m_bestMoveReady` — Atomic visibility variable checking calculated items availability to drive polling routines.
* `std::atomic<bool> m_searchAborted` — Atomic flag tracking user-driven process interruptions.
* `std::atomic<float> m_currentEvaluation` — Synchronized atomic parameter storing calculation scoring metrics.
* `std::atomic<bool> m_isMateDetected` / `m_mateInMoves` — Atomic flags mapping checkmate status and distances.
* `std::atomic<bool> m_positionSideIsWhite` — Tracks active turns inside internal parsing steps to normalize evaluation scores.
* `std::atomic<bool> m_responseReceived` / `std::string m_expectedResponse` — Handshake variables validating targeting lines across startup sequences.