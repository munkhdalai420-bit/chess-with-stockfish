```markdown

\# Architectural Component Specification



This document provides an exhaustive reference of the primary class architectures, access modifiers, field types, and member responsibilities driving the core execution lifecycle of the application.



\---



\## Component Architecture Overview





```



┌─────────────────────────────────────────────────────────┐

│                     GameController                      │

│  (State Machine, Core Coordination \& Rules Mediation)   │

└───────────┬─────────────────────────┬───────────────────┘

│ Composition             │ Composition

▼                         ▼

┌───────────────────────┐ ┌───────────────────────────────┐

│         Board         │ │         EngineManager         │

│ (Pure Data Model/FEN) │ │  (Asynchronous Stockfish IPC) │

└───────────────────────┘ └───────────────────────────────┘

▲

│ Dependency (Reads State via Const Reference)

┌───────────┴───────────┐

│       Renderer        │

│ (Immediate-Mode UI)   │

└───────────────────────┘



```



\---



\## Detailed Class Reference



\### 1. Class: `GameController`

Centralizes the application's business logic, managing state transitions between the lobby and active play, while orchestrating human interactions, audio effects, and engine background updates.



\#### Public Member Functions

\* `GameController(int windowWidth, int windowHeight, int tileSize, int sidebarWidth, Renderer\* renderer)`

&#x20;   \* Constructs the controller and initializes game variables, audio asset contexts, the engine subprocess, and immediate-mode layout boundaries.

\* `\~GameController()`

&#x20;   \* Destructor; coordinates the explicit shutdown sequence for the background engine thread and frees active audio devices.

\* `int getTargetElo() const` / `void cycleTargetElo()`

&#x20;   \* Accesses and cycles through the targeted engine Elo difficulty constraints ($500 \\to 1000 \\to 1500 \\to 2000$).

\* `bool isMatchStarted() const` / `bool isMatchEnded() const`

&#x20;   \* Queries active lifecycle flag states defining current input interaction scopes (Lobby vs. Active Match vs. Post-Match Review).

\* `void startMatch()` / `void endMatch()`

&#x20;   \* Initializes or tears down active match parameters, handling background AI thread triggers upon instantiation.

\* `void undo()` / `void redo()`

&#x20;   \* Traverses individual match steps sequentially while transitioning the system context to History Review mode.

\* `void goToHistoryIndex(size\_t index)`

&#x20;   \* Performs arbitrary random-access traversal across the timeline history array.

\* `float getDisplayedEvaluation() const`

&#x20;   \* Retrieves the stored float evaluation score relative to the currently scrubbed history position index.

\* `bool isMateDetected() const` / `int getMateInMoves() const`

&#x20;   \* Queries checkmate evaluation status and distance metrics passed back from the engine worker thread.

\* `bool canUndo() const` / `bool canRedo() const`

&#x20;   \* Validates boundary constraints to guard against out-of-bounds history array lookups.

\* `void update()`

&#x20;   \* Processes frame-by-frame user inputs and advances application state; executed exactly once per frame tick.

\* `void requestHint()` / `void clearActiveHint()` / `bool canRequestHint() const`

&#x20;   \* Dispatches or aborts sub-second asynchronous calculations targeted for generating on-screen destination visual helpers.

\* `std::string getHintMove() const`

&#x20;   \* Returns the cached best move string derived from the hint sequence in standard Universal Chess Interface (UCI) notation.

\* `Board\& getBoard()`

&#x20;   \* Exposes a mutable reference to the underlying structural data matrix for direct modification.

\* `std::optional<std::pair<int,int>> getSelected() const`

&#x20;   \* Exposes coordinates for the currently selected tile index, returning `std::nullopt` if no piece is active.

\* `std::string getMoveMessage() const` / `float getMessageTimer() const`

&#x20;   \* Tracks and updates temporal text data flags deployed to notify users of illegal move inputs.

\* `GameController::PlayerColor getPlayerColor() const` / `void setPlayerColor(...)` / `void togglePlayerColor()`

&#x20;   \* Accesses, defines, or inverts the human participant's perspective and piece alignment.

\* `bool isEngineIdle() const`

&#x20;   \* Returns true if the engine thread state is un-entangled and sitting in a blocking idle pause loop.



\#### Private Data Members

\* `Board m\_board` — In-memory representation of the active match grid state and rule validator engine.

\* `EngineManager m\_engine` — Multi-threaded operating system pipeline wrapper isolating Stockfish logic.

\* `EngineMode m\_engineMode` — Enumerated parameter tracking worker status (`Idle`, `AI\_Thinking`, `Hint\_Calculating`).

\* `std::string m\_hintMove` — Cached coordinates storing the localized hint text asset.

\* `bool m\_isReviewingHistory` — Controls user interaction gates when viewing historical steps.

\* `std::optional<std::pair<int,int>> m\_selected` — Zero-drift structure holding array indexes for targeted pieces.

\* `std::vector<float> m\_evaluationHistory` — Dynamic vector collection preserving continuous engine scoring.

\* `size\_t m\_historyIndex` — Timeline frame pointer index mapping history viewing locations.

\* `int m\_targetElo` — Scalar value containing the literal chosen difficulty floor.

\* `PlayerColor m\_playerColor` — Tracks active alignment definitions assigned to the user.

\* `int m\_timePerMoveMs` — Maximum time allocated for engine processing sequences before enforcing execution cut-off.

\* `int m\_eloIndex` — Internal structural offset position tracking option shifts.

\* `bool m\_gameStarted` — Tracks execution of an alive game loop instance.

\* `bool m\_matchEnded` — State parameter tracking transitions into decoupled review loops.

\* `std::string m\_moveMessage` — String container storing error strings.

\* `float m\_messageTimer` — Tracks screen duration remaining for temporary text notices.

\* `int m\_windowWidth` / `m\_windowHeight` / `m\_tileSize` / `m\_sidebarWidth` — Metric dimensions.

\* `Renderer\* m\_renderer` — Non-owning pointer targeting global drawing structures.

\* `Sound m\_sndCapture` / `m\_sndCastle` / `m\_sndMoveCheck` / `m\_sndMoveSelf` / `m\_sndPromote` — Sound contexts.

\* `bool m\_audioEnabled` — Verification flag reporting hardware driver initialization success.



\---



\### 2. Class: `Renderer`

Responsible exclusively for pixel output, coordinate transformations, asset allocations, and immediate-mode UI interface scaling.



\#### Public Member Functions

\* `Renderer(int windowWidth, int windowHeight, int tileSize)`

&#x20;   \* Initializes display canvas variables and pre-computes operational offset metrics.

\* `\~Renderer()`

&#x20;   \* Iterates through allocations to cleanly drop textures, memory buffers, and font atlases.

\* `void setTheme(Renderer::BoardTheme theme)`

&#x20;   \* Modifies color parameters across rendering operations based on global enum selections.

\* `bool loadTextures()`

&#x20;   \* Allocates PNG asset streams into memory maps; returns true if files successfully bound.

\* `void render(const Board\& board, const std::optional<std::pair<int,int>>\& selected, float evaluation, GameController\& controller)`

&#x20;   \* The primary layout function. Transforms class datasets into graphical arrays, outputs pieces, and updates interaction regions.

\* `void triggerMoveAnimation(int fromX, int fromY, int toX, int toY, char pieceChar, char pieceColor, bool hasSecondary = false, int secFromX = -1, int secToX = -1, char secPieceChar = 'r')`

&#x20;   \* Initializes structural delta paths used to interpolate piece transitions smoothly across frames.



\#### Private Data Members

\* `int m\_windowWidth` / `m\_windowHeight` / `m\_tileSize` — Base configuration dimensions.

\* `int m\_boardPixelSize` / `m\_boardOriginX` / `m\_boardOriginY` — Dynamic rendering layout scales.

\* `int m\_sidebarWidth` — Fixed horizontal dimension allocation reserved for side info panels.

\* `std::unordered\_map<std::string, Texture2D> m\_textures` — Memory map holding loaded GPU texture assets.

\* `Font m\_mainFont` — Handle structure containing loaded TrueType Font assets.

\* `BoardTheme m\_theme` — Enumerated parameter configuring active palette styling.

\* `int m\_themeIndex` — Counter parameter tracking palette configuration toggles.

\* `bool m\_animatePieces` — Master flag controlling interpolation animation paths.

\* `PieceAnimation m\_currentAnim` — Structural wrapper maintaining active frame positional deltas.

\* `float m\_historyScrollOffset` — Vertical tracker capturing structural scrolling positions in long game histories.

\* `double m\_lastHistoryClickTime` — Floating timestamp capturing input delta intervals to filter double clicks.

\* `int m\_lastHistoryClickedIndex` — Tracked history row value utilized to verify target uniformity.

\* `mutable bool m\_flip` — Flag defining board rendering orientation (marked `mutable` to permit state shifts during `const` drawing loops).



\---



\### 3. Class: `Board`

Acts as the central rule engine and pure state model of the application, managing move processing, verification loops, validation simulations, and notation strings.



\#### Public Member Functions

\* `Board()`

&#x20;   \* Sets class parameters to blank conditions and resets state vectors.

\* `const Piece\* at(int row, int col) const` / `Piece\* at(int row, int col)`

&#x20;   \* Overloaded methods providing row/col checks on piece assets; returns `nullptr` for empty squares.

\* `void initializeStandardSetup()`

&#x20;   \* Clears memory structures and constructs standard positions matching baseline rules.

\* `MoveResult movePiece(int startRow, int startCol, int endRow, int endCol)`

&#x20;   \* Commits execution adjustments to memory models, verifying legality and evaluating side effects (e.g., Castling, Check detection, Promotion triggers).

\* `bool isSquareUnderAttack(int row, int col, PieceColor attackerColor) const`

&#x20;   \* Scans paths relative to target tiles to verify intersections from opposing pieces.

\* `bool isInCheck(PieceColor color) const`

&#x20;   \* Locates target King pieces to check for structural threats.

\* `bool hasLegalMoves(PieceColor color) const` / `hasLegalMoves(PieceColor color)`

&#x20;   \* Iterates across viable paths to check for legal alternatives, serving as the basis for checkmate evaluations.

\* `GameState getGameState() const`

&#x20;   \* Returns categorized state variables detailing terminal match statuses (`Active`, `Checkmate`, `Stalemate`).

\* `const std::string\& getLastMoveError() const`

&#x20;   \* Exposes internal text identifiers outlining input failures.

\* `std::pair<int,int> getEnPassantTarget() const`

&#x20;   \* Returns coordinates for active en-passant validation targets, returning `{-1,-1}` if empty.

\* `void undoMove()` / `void redoMove()`

&#x20;   \* Restores or re-applies historical move steps using cached delta parameters.

\* `bool wouldMoveBeLegal(int startRow, int startCol, int endRow, int endCol) const`

&#x20;   \* Simulates the target move via structural state changes, checking for legality before rolling back modifications.

\* `bool isAwaitingPromotion() const` / `std::pair<int,int> getPendingPromotionSquare() const`

&#x20;   \* Queries active pawn replacement overlays and identifies corresponding coordinate targets.

\* `MoveResult completePromotion(PieceType chosenType)`

&#x20;   \* Finalizes active promotion sequences by replacing target pointers with chosen piece references.

\* `std::optional<ChessMove> getLastMove() const`

&#x20;   \* Retrieves data matching the most recently committed move step to drive interface highlight behaviors.

\* `std::string getFEN() const` / `bool loadFromFEN(const std::string\& fen)`

&#x20;   \* Generates or parses standard Forsyth-Edwards Notation (FEN) text streams.

\* `std::string moveToSAN(const ChessMove\& move)` / `void setLastMoveSAN(const std::string\& san)`

&#x20;   \* Generates or caches standard algebraic notation text blocks matching structural move variables.

\* `std::string getFullPGNText() const`

&#x20;   \* Compiles complete Portable Game Notation (PGN) documentation blocks utilizing move history vectors.

\* `std::vector<ChessMove> getMoveHistory() const`

&#x20;   \* Provides deep copies of active history stacks to decouple visualization passes.

\* `ChessMove parseEngineMove(const std::string\& moveStr)`

&#x20;   \* Transforms raw engine strings (e.g., `e2e4`, `e7e8q`) into formatted application structs.

\* `PieceColor getCurrentTurn() const`

&#x20;   \* Reports active player flags defining turn parameters.

\* `uint8\_t getCastlingRights() const`

&#x20;   \* Exposes a 4-bit bitmask tracking active castling eligibility.



\#### Private Data Members

\* `std::unique\_ptr<Piece> m\_squares\[Tiles]\[Tiles]` — Primary matrix holding ownership references for pieces.

\* `PieceColor m\_currentTurn` — Enum flag containing current turn parameters.

\* `std::string m\_lastMoveError` — Internal character storage containing structural errors.

\* `std::pair<int,int> m\_enPassantTarget` — Targeted vector recording valid en-passant paths.

\* `std::vector<ChessMove> m\_moveHistory` — History log tracking committed steps to support serialization routines.

\* `std::vector<ChessMove> m\_redoStack` — Secondary history collection tracking available forward steps.

\* `bool m\_isAwaitingPromotion` — Operational gate tracking active on-screen replacement menus.

\* `std::pair<int,int> m\_pendingPromotionSquare` — Structural row/col bounds tracing promotion positions.

\* `std::optional<ChessMove> m\_lastMove` — Struct copy mapping recent steps to drive visual elements.

\* `int m\_halfmoveClock` — Step tracker verifying requirements for the 50-move draw rule.

\* `int m\_fullmoveNumber` — Core loop step counter outputted within FEN streams.

\* `uint8\_t m\_castlingRights` — Operational bitfield containing binary indicators for castling flags.



\---



\### 4. Class: `EngineManager`

An isolated multi-threaded system component running Stockfish within a separate background process, communicating asynchronously via low-level standard pipes.



\#### Public Member Functions

\* `EngineManager()`

&#x20;   \* Clears handles and registers atomic status primitives to zero states.

\* `\~EngineManager()`

&#x20;   \* Guarantees standard termination sequences are cleanly completed to avoid orphaned system processes.

\* `bool launch(const std::string\& path = "")`

&#x20;   \* Spawns engine subprocesses and connects asynchronous standard output streams via Windows API anonymous pipes.

\* `bool sendCommand(const std::string\& cmd)`

&#x20;   \* Dispatches text commands to standard engine input streams in a non-blocking configuration.

\* `bool startSearch(const std::string\& fen, int timeMs)`

&#x20;   \* Dispatches positional layout state markers and initiates localized engine analysis rounds.

\* `void stopSearch()`

&#x20;   \* Interrupts deep calculation searches to force instant outputs from background processes.

\* `bool checkBestMove(std::string\& outMove)`

&#x20;   \* Non-blocking thread scanner checking engine queues; copies data to the output variable upon calculation completion.

\* `float getEvaluation() const`

&#x20;   \* Exposes current evaluation values normalized from White's perspective.

\* `void setDifficulty(int elo)`

&#x20;   \* Limits calculation depth configurations across engine runs using standard UCI interface limits.

\* `void clearMateDetection()` / `bool isMateDetected() const` / `int getMateInMoves() const`

&#x20;   \* Accesses or resets calculations capturing structural checkmate distances.

\* `bool isRunning() const`

&#x20;   \* Returns true if background threads and active system process handles are valid.

\* `void shutdown()`

&#x20;   \* Dispatches termination indicators, terminates system pipes, and joins the worker thread.



\#### Private Data Members

\* `void\* m\_hProcess` — Internal operating system reference tracking the Stockfish background task.

\* `void\* m\_hStdinRead` / `m\_hStdinWrite` — Pipe handles tracking internal engine instruction feeds.

\* `void\* m\_hStdoutRead` / `m\_hStdoutWrite` — Pipe endpoints capturing engine terminal data outputs.

\* `std::thread m\_workerThread` — Concurrent processing thread handling blocked line-by-line streaming passes.

\* `std::atomic<bool> m\_isRunning` — Atomic state flag confirming process health parameters across runtime iterations.

\* `std::atomic<bool> m\_shouldShutdown` — Core atomic thread control parameter used to drop reading loops.

\* `std::mutex m\_mutex` — Synchronization lock protecting multi-threaded operations.

\* `std::string m\_lastLine` — String tracker capturing output chunks for validation parsing.

\* `std::string m\_bestMove` — Processed output variable identifying chosen strategic pathways.

\* `std::atomic<bool> m\_bestMoveReady` — Atomic visibility flag confirming data presence for main thread polling loops.

\* `std::atomic<bool> m\_searchAborted` — Atomic flag tracking user-driven process interruptions.

\* `std::atomic<float> m\_currentEvaluation` — Synchronized score parameter holding evaluation outputs.

\* `std::atomic<bool> m\_isMateDetected` — Atomic flag mapping checkmate status indicators.

\* `std::atomic<int> m\_mateInMoves` — Atomic counter identifying move gaps before checkmate constraints.

\* `std::atomic<bool> m\_positionSideIsWhite` — Tracks active turns inside internal parsing steps to normalize evaluation scores.

\* `std::atomic<bool> m\_responseReceived` — Synchronization flag managing step sequences during engine setup handshakes.

\* `std::string m\_expectedResponse` — Validation parameter defining expected engine handshake responses.



```



\---

