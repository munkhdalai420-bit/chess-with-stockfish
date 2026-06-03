#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

class EngineManager
{
public:
    /**
     * @brief Launch the Stockfish engine subprocess and set up IPC pipes.
     *
     * Creates anonymous pipes for stdin/stdout redirection and starts a
     * background worker thread to read engine output. If `path` is empty the
     * implementation will attempt to locate the default engine binary.
     * @param path Optional path to the engine executable.
     * @return true on successful launch and UCI handshake, false on failure.
     */
    bool launch(const std::string& path = "");

    /**
     * @brief Send a raw UCI command to the engine.
     * @param cmd The command string (without guaranteed trailing newline).
     * @return true on success; false if engine is not running or write fails.
     */
    bool sendCommand(const std::string& cmd);

    /**
     * @brief Initiate an engine search from the provided FEN with a time limit.
     *
     * The engine will receive a "position fen <fen>" command followed by
     * "go movetime <timeMs>". The background worker will parse live info
     * lines and publish evaluations and eventual bestmove.
     * @param fen FEN string representing the search position.
     * @param timeMs Milliseconds of thinking time to allocate.
     * @return true if the command was dispatched, false otherwise.
     */
    bool startSearch(const std::string& fen, int timeMs);

    /**
     * @brief Stop an ongoing search by sending the UCI "stop" command.
     *
     * Marks the current search as aborted and signals the engine to return
     * its current best move. The aborted search result will be ignored by
     * `checkBestMove`.
     */
    void stopSearch();

    /**
     * @brief Non-blocking retrieval of a computed bestmove.
     * @param outMove Receives the UCI move string when available (e.g., "e2e4").
     * @return true if a best move was available and written to outMove.
     */
    bool checkBestMove(std::string& outMove);

    /** @brief Return the most recent evaluation published by the engine (positive = White advantage). */
    float getEvaluation() const;
    /** @brief Configure engine playing strength (UCI Elo). */
    void setDifficulty(int elo);
    /** @brief Clear any cached mate detection state. */
    void clearMateDetection();
    /** @brief True if the engine has reported a forced mate in its info lines. */
    bool isMateDetected() const;
    /** @brief If mate detected, returns mate distance in plies (signed by side). */
    int getMateInMoves() const;

    /** @brief Return true if the engine process and worker thread are active. */
    bool isRunning() const;

    /** @brief Gracefully stop the engine and release OS resources. */
    void shutdown();

    EngineManager();
    ~EngineManager();

    // Prevent copying
    EngineManager(const EngineManager&) = delete;
    EngineManager& operator=(const EngineManager&) = delete;

private:
    /// Background thread worker function
    void backgroundWorker();

    /// Blocks until a specific response is received from the engine
    /// Returns false on timeout or error
    bool waitForResponse(const std::string& expectedLine, int timeoutMs = 5000);

    /// Parses a bestmove line and extracts the move
    static std::string parseBestMove(const std::string& line);

    // Process handles and pipes (opaque pointers to avoid exposing Windows.h)
    void* m_hProcess = nullptr;
    void* m_hStdinRead = nullptr;
    void* m_hStdinWrite = nullptr;
    void* m_hStdoutRead = nullptr;
    void* m_hStdoutWrite = nullptr;

    // Worker thread
    std::thread m_workerThread;
    std::atomic<bool> m_isRunning = false;
    std::atomic<bool> m_shouldShutdown = false;

    // Shared state protection
    std::mutex m_mutex;

    // Output parsing state
    std::string m_lastLine;                  // Latest line read from engine
    std::string m_bestMove;                  // Extracted best move when ready
    std::atomic<bool> m_bestMoveReady = false;

    // Flag to indicate that a search was aborted; set when stopSearch() is called
    // Used to ignore any bestmove that arrives after a stop was requested
    std::atomic<bool> m_searchAborted = false;

    // Live evaluation published by the engine (positive = White advantage)
    std::atomic<float> m_currentEvaluation{0.0f};
    // Mate detection
    std::atomic<bool> m_isMateDetected{false};
    std::atomic<int> m_mateInMoves{0};

    // Indicates whether the last position sent to the engine had White to move
    // Used to normalize engine score (Stockfish outputs score from side-to-move's perspective)
    std::atomic<bool> m_positionSideIsWhite{true};

    // Response synchronization (for UCI handshake)
    std::atomic<bool> m_responseReceived = false;
    std::string m_expectedResponse;
};
