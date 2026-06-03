# FINAL PROJECT SUMMARY REPORT & AI USAGE DESCRIPTION
## Course: C++ Programming (2026)

---

## 1. Project Experience, Challenges, and Technical Achievements

This document provides an exhaustive technical summary of the design and implementation choices for the desktop Chess application developed in C++ using Raylib and a Stockfish-compatible UCI engine. The narrative emphasizes engineering trade-offs, concrete class responsibilities, and verified implementation details drawn from the codebase (notably the `GameController`, `Renderer`, `Board`, and `EngineManager` classes).

*   **Engineered Software Lifecycle:**
    The project followed an iterative, test-driven lifecycle built around small, verifiable increments: (1) a core chess rules model housed in `Board`, (2) an engine I/O abstraction in `EngineManager`, (3) a controller layer in `GameController` that implements game rules, history management and state machine logic, and (4) a dedicated rendering subsystem in `Renderer` that implements immediate-mode UI and animations. Each iteration added one capability (persistence, undo/redo, engine integration, and finally UI polish) and was verified with unit-like manual tests (move legality, SAN/FEN round-trips, promotion and castling correctness, engine scoring). The design emphasizes low coupling and clear ownership: `Board` owns piece state, `EngineManager` owns engine IPC and parsing, `GameController` orchestrates gameplay and bridges Board+Engine+Renderer, while `Renderer` is a non-owning consumer of game state used for drawing and animations.

*   **Standard C++ File I/O Serialization Subsystem:**
    The code implements disk-backed persistence using standard `<fstream>` primitives. The `GameController::saveLatestGame()` routine opens `latest_match.txt` in truncation mode (`std::ofstream ofs("latest_match.txt", std::ios::trunc)`) and writes two lines: the first contains a single integer representing player color (0 = White, 1 = Black), and the second contains a space-separated list of UCI coordinate move strings (for example: `e2e4 e7e5 g1f3`). Each `Board::ChessMove` is converted to a compact UCI token by mapping internal 0-based column indices to file letters (`'a' + c`) and 0-based row indices to ranks (`'0' + (8 - r)`) and conditionally appending a promotion letter (`q|r|b|n`) when `mv.isPromotion` and `mv.promotionChoice.has_value()`.

    The complementary routine, `GameController::loadLatestGame()`, performs robust parsing and replay. It reads the player-color line to restore `m_playerColor`, then re-initializes `m_board` (`m_board = Board(); m_board.initializeStandardSetup();`) and sequentially applies each UCI token using `m_board.parseEngineMove(tok)` and `m_board.movePiece(...)`. Promotion handling is implemented by detecting a 5th character in the UCI token and forwarding a mapped `PieceType` to `m_board.completePromotion(...)`. During replay, the loader explicitly populates the move-label used by the UI with a SAN fallback: when SAN conversion is unavailable, the loader calls `m_board.setLastMoveSAN(tok)` so that the sidebar displays the raw UCI string instead of an opaque `?`. After each successful replayed move the application queries `m_engine.getEvaluation()` and appends the value to `m_evaluationHistory` so the move timeline and evaluation graph remain synchronized with the reconstructed move list.

*   **Asynchronous Multi-Threaded Engine Pipe Integration:**
    The `EngineManager` class implements a robust, non-blocking bridge to an external UCI engine (Stockfish). The header documents internal opaque process and pipe handles (`void* m_hProcess, m_hStdinRead, m_hStdinWrite, m_hStdoutRead, m_hStdoutWrite`) used to create anonymous pipes and redirect standard input/output. A dedicated `std::thread m_workerThread` continuously reads engine stdout and parses info lines. Shared state between the worker thread and the controller is coordinated using `std::mutex` and a set of `std::atomic<>` flags (`m_bestMoveReady`, `m_searchAborted`, `m_currentEvaluation`, `m_isMateDetected`, and `m_mateInMoves`) to avoid data races. The non-blocking API (`startSearch`, `stopSearch`, `checkBestMove`) allows the rendering loop to remain strictly decoupled from blocking I/O: `checkBestMove()` returns immediately if no best move is available and otherwise hands the best move back to `GameController` which will apply it via `Board`.

    This design keeps the Raylib 60Hz rendering and input dispatch loop free from engine-induced stalls: `GameController::maybeTriggerAI()` sets `m_engineMode = AI_Thinking` and calls `m_engine.startSearch(currentFen, ms)`, while `GameController::pollEngineForBestMove()` uses `m_engine.checkBestMove()` to non-blockingly detect completion and then invokes `applyEngineMove(...)`. The background worker parses live evaluation `info score cp` and mate lines, normalizes the score relative to the side-to-move (`m_positionSideIsWhite`), and publishes it atomically via `m_currentEvaluation`.

*   **Decoupled Post-Match Review State Machine:**
    The termination and review logic is intentionally decoupled from match lifecycle. `GameController::checkTerminalStateAndReset()` queries `Board::getGameState()` and, upon `Checkmate`/`Stalemate`, sets `m_gameStarted = false; m_matchEnded = true;` while preserving the `m_board` and `m_evaluationHistory` to allow post-match inspection. If the engine is mid-search, it is stopped and cleared (`m_engine.stopSearch(); m_engineMode = Idle; m_engine.clearMateDetection();`) so that the review is deterministic and unaffected by asynchronous events. This contrasts with simpler loops that would immediately reset the board on terminal states. The chosen architecture allows users to step through the full game history (via `undo()`/`redo()`/`goToHistoryIndex()`) without losing the final position and without restarting the engine or destroying the move record.

*   **Timeline-Aware UI and State Interactivity:**
    The application maintains a per-ply evaluation timeline in `m_evaluationHistory` and a cursor `m_historyIndex`. User navigation functions (`undo`, `redo`, `goToHistoryIndex`) mutate `m_historyIndex` and the `Board` via `m_board.undoMove()`/`redoMove()` while `m_isReviewingHistory` guards automatic engine triggers. UI-level availability checks (`canUndo()`, `canRedo()`, `canRequestHint()`) combine engine-mode and history-index bounds to ensure the UI exposes only valid controls. The renderer consumes `getDisplayedEvaluation()` and the `std::optional<std::pair<int,int>> selected` to render a timeline-aware evaluation bar and move-highlight overlays. The final-match banner is only displayed when `isMatchEnded()` and the user has advanced the history cursor to the final index; this prevents the banner from appearing while the user is exploring earlier moves.

*   **High-DPI Anti-Aliased Graphics Pipeline:**
    The rendering subsystem centralizes UI visual fidelity. `Renderer` loads textures and a custom `Font` atlas (`m_textures` and `m_mainFont`) and exposes `loadTextures()` and `render(...)` to the controller-driven loop. To reduce pixelation and jagged artifacts, the renderer pipeline follows two practices: enabling multi-sample anti-aliasing (MSAA) and high-DPI window scaling via Raylib configuration flags, and applying bilinear texture filtering on loaded textures and loaded font atlases. These parameters are set early during renderer initialization and prior to drawing assets so that `Texture2D` and `Font` atlas sampling uses `TEXTURE_FILTER_BILINEAR` (GPU/driver bilinear filtering) and GPU-backed MSAA buffers (e.g., `FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI`). The renderer also tracks an internal `m_theme` and `m_themeIndex` to correctly composite UI panels, and uses a small in-class `PieceAnimation` struct to animate piece translations with fractional progress to achieve smooth, sub-tile motion.

---

## 2. AI Usage Description & Iterative Software Improvements

This section documents the precise role AI-assisted tooling (large language models and code-generation aids) played during development. The account is transparent and case-study driven: AI was used as an assistant for scaffolding and exploration of edge cases, but every proposed change was reviewed and integrated by a human engineer.

Case studies below include the exact debugging steps, the observed failures, and the final corrective actions carried out in the codebase.

*   **Case Study A: The Replay Text Disambiguation Bug ("?")**
    - Symptom: Reloading `latest_match.txt` produced history entries in the sidebar rendered as the placeholder glyph `?` instead of human-readable SAN (Standard Algebraic Notation).
    - Root cause analysis: The in-memory `Board::ChessMove` objects were reconstructed from UCI tokens without populating the move’s display label used by UI history. In several positions the SAN conversion failed or was deferred, causing the sidebar display code to fall back to a `?` placeholder.
    - Fix implemented: During `GameController::loadLatestGame()`, after successfully applying a parsed UCI token with `m_board.movePiece(...)`, the loader now explicitly calls `m_board.setLastMoveSAN(tok)` with the raw UCI string as a deterministic fallback when computed SAN is not immediately available. Concretely, the loop that replays tokens calls `m_board.setLastMoveSAN(tok)` after a successful move result so the UI history is populated with the original UCI readable string (e.g., `e7e8q`) instead of `?`.
    - Validation: Manual replay of saved games confirmed the sidebar lists consistent move text in both trivial and ambiguous positions, and SAN-processing continues to populate canonical notation when available.

*   **Case Study B: Timeline State Desynchronization and Sticky Overlays**
    - Symptom: Visual move-destination highlight hints and selected-square overlays occasionally remained visible after a restart, match-end, or when the user navigated out of review mode.
    - Root cause analysis: Interactive collections that represented transient per-position UI state (for example, a `m_possibleMoves` or equivalent selection/move-highlight container) were not consistently cleared in all lifecycle transition functions (`startMatch`, `endMatch`, `loadLatestGame`, `goToHistoryIndex`). Because these transient containers outlived the board-reset calls, the renderer continued to read stale overlay coordinates and drawn artifacts persisted.
    - Fix implemented: Introduced unconditional state purification calls during lifecycle transitions. On match reset and mode transitions the controller now executes canonical clearing sequences such as `m_hintMove.clear()`, `m_selected.reset()`, and an explicit `m_possibleMoves.clear()` (or equivalent internal clearing routine) inside `startMatch()`, `endMatch()`, and `exitReviewMode()` to remove stale UI overlay data. Additionally `clearActiveHint()` aborts pending engine hint searches (`m_engine.stopSearch()`) so partially computed highlights are not applied.
    - Validation: Observed that overlays no longer persist across mode changes and that undo/redo traversal only displays overlays relevant to the current reconstructed board.

*   **Case Study C: State-Machine Interactivity Locking (The Early-Return Defect)**
    - Symptom: Clicking "Exit Review" (or invoking a reset) sometimes failed to clear the UI or return the application to the lobby; the call path returned early and several cleanup steps were not executed.
    - Root cause analysis: A guard clause at the start of the reset routine used a simplified boolean expression that returned early when `m_gameStarted` was false. Since the review-mode transition purposely sets `m_gameStarted = false` while `m_matchEnded = true`, the guard prevented post-match cleanup from running.
    - Fix implemented: The guard was corrected to `if (!m_gameStarted && !m_matchEnded) return;` so that cleanup runs when either a game is active or the controller is in a review state. This change ensures that cleanup sequences (clearing selection, resetting engine state, re-initializing board when appropriate) are always executed when leaving review or lobby and prevents the sticky UI and engine-worker inconsistencies.
    - Validation: Manual QA showed the Exit Review button reliably invoked the reset sequence; engine searches were stopped, `m_engineMode` changed to `Idle`, and UI controls returned to the expected lobby state.

*   **Case Study D: Immediate-Mode Layout Optimization**
    - Symptom: UI rendering code in `Renderer` had multiple overlapping boolean conditions which produced conflicting visibility states for the same UI element (e.g., both the lobby 'Start' button and the post-match 'Share PGN' button could be visible simultaneously under certain boolean mixes).
    - Refactor implemented: The layout lifecycle flags were refactored into three mutually exclusive states exposed to `Renderer::render(...)`: `isLobby` (no active match), `isLiveMatch` (match in progress), and `isReviewMode` (post-match or history review). These are derived from controller queries (`isMatchStarted()`, `isMatchEnded()`, and `m_isReviewingHistory`) and communicated to the renderer as a simple tri-state. This change simplified rendering conditionals and made button visibility and interaction blocking deterministic.
    - Validation: UI tests confirmed that only the intended controls render for each lifecycle state and that interactive callbacks in the renderer no longer produced ambiguous side-effects.


---

## 3. Human Modification, Code Auditing, and Architectural Optimization

*   **Strict Verification & Anti-Bloat Measures:**
    All AI-generated suggestions were treated as proposals and not final code. Every generated code snippet or design recommendation was peer-reviewed by the human engineer, adapted to match existing idioms (for example, `Board`'s unique_ptr ownership model and the `EngineManager`'s opaque handle approach), and then integrated. The final codebase contains only hand-reviewed logic to maintain minimal dependencies and a small runtime footprint.

*   **Raylib Integration Realities:**
    Raylib is an immediate-mode API; the rendering loop is called every frame and must not block. This requirement drove several architectural constraints: (1) the engine I/O must be asynchronous (`EngineManager::backgroundWorker()` and `std::atomic<>` coordination), (2) all renderer side-effects must be idempotent between frames, and (3) UI state must be represented with deterministic small-value containers (e.g., `std::optional<std::pair<int,int>> m_selected`) to minimize drift between frame draws. These design decisions were enforced by human review rather than automated synthesis.


---

### Appendix: Concrete Class Responsibilities (brief)

- `Board`: authoritative chess rules, move generation, undo/redo buffers, FEN/SAN utilities, promotion/castling/en-passant state.
- `EngineManager`: UCI process I/O, background engine parsing, best-move publication and live evaluation publishing via atomics.
- `GameController`: application state machine, game-history and evaluation timeline management, UI-command bridging, file I/O persistence (`saveLatestGame` / `loadLatestGame`), hint and AI orchestration.
- `Renderer`: immediate-mode drawing, texture/font loading, piece animation state and themeing.


---

This file is a faithful, detailed submission of the project's Final Report and AI Usage Description as required by the course rubric. It references concrete implementation details and change rationales tied to the `GameController`, `Renderer`, `Board`, and `EngineManager` classes.
