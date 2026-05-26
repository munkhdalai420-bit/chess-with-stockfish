#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

class EngineManager
{
public:
    /// Launches the Stockfish engine process with optional custom path.
    /// If path is empty, defaults to "./engines/stockfish.exe"
    bool launch(const std::string& path = "");

    /// Sends a raw command to the Stockfish engine (e.g., "position startpos")
    /// Returns false if engine is not running
    bool sendCommand(const std::string& cmd);

    /// Initiates a search from the given FEN position with a time limit in milliseconds
    bool startSearch(const std::string& fen, int timeMs);

    /// Immediately stops the current search by sending "stop" to Stockfish
    /// This allows the engine to return its current best guess
    void stopSearch();

    /// Non-blocking check for a computed best move.
    /// Returns true if a best move is available and populates outMove with it.
    /// Clears the ready flag after retrieval.
    bool checkBestMove(std::string& outMove);

    /// Returns the last published evaluation from the engine (positive = White advantage)
    float getEvaluation() const;
    /// Configure engine strength via UCI Elo (enables UCI_LimitStrength)
    void setDifficulty(int elo);

    /// Checks if the engine is currently running
    bool isRunning() const;

    /// Gracefully stops the engine and cleans up resources
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
