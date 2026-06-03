#include <windows.h>
#include "EngineManager.h"
#include <iostream>
#include <sstream>
#include <chrono>

/**
 * @brief Default constructor
 *
 * Basic default construction; heavy initialization (process creation and
 * worker thread start) is performed by `launch()`.
 */
EngineManager::EngineManager() = default;

/**
 * @brief Destructor ensures engine is shut down and resources released.
 */
EngineManager::~EngineManager()
{
    shutdown();
}

/** @brief Get the most recently parsed engine evaluation (normalized to White's perspective). */
float EngineManager::getEvaluation() const
{
    return m_currentEvaluation.load();
}

/** @brief Return true when the engine has reported a mate line. */
bool EngineManager::isMateDetected() const
{
    return m_isMateDetected.load();
}

/** @brief Return mate distance in plies as reported by the engine (signed). */
int EngineManager::getMateInMoves() const
{
    return m_mateInMoves.load();
}

/** @brief Reset mate detection state. */
void EngineManager::clearMateDetection()
{
    m_isMateDetected.store(false);
    m_mateInMoves.store(0);
}

/**
 * @brief Configure engine playing strength via UCI options (Elo).
 * @param elo Target Elo value applied to the engine using UCI_LimitStrength.
 */
void EngineManager::setDifficulty(int elo)
{
    if (!m_isRunning) return;

    // Enable limited strength and set Elo
    sendCommand("setoption name UCI_LimitStrength value true");
    sendCommand(std::string("setoption name UCI_Elo value ") + std::to_string(elo));
    // Ensure engine applied options
    sendCommand("isready");
    waitForResponse("readyok", 2000);
}

bool EngineManager::launch(const std::string& path)
{
    if (m_isRunning)
    {
        return false; // Already running
    }

    std::string enginePath = path.empty() ? "./engines/stockfish.exe" : path;

    // Create anonymous pipes for stdin and stdout
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create pipe for stdin (parent writes, child reads)
    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0))
    {
        std::cerr << "Failed to create stdin pipe" << std::endl;
        return false;
    }

    // Create pipe for stdout (parent reads, child writes)
    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0))
    {
        std::cerr << "Failed to create stdout pipe" << std::endl;
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        m_hStdinRead = m_hStdinWrite = nullptr;
        return false;
    }

    // Prepare process startup info
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = m_hStdinRead;
    si.hStdOutput = m_hStdoutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE; // Hide window

    PROCESS_INFORMATION pi = {};

    // Create the process
    if (!CreateProcessA(
        enginePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        TRUE,               // Inherit handles
        CREATE_NO_WINDOW,   // Create with no window
        nullptr,
        nullptr,
        &si,
        &pi))
    {
        std::cerr << "Failed to create Stockfish process: " << GetLastError() << std::endl;
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        m_hStdinRead = m_hStdinWrite = m_hStdoutRead = m_hStdoutWrite = nullptr;
        return false;
    }

    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread); // Close thread handle; we don't need it

    // Close the inherited handles in the parent process
    CloseHandle(m_hStdinRead);
    CloseHandle(m_hStdoutWrite);
    m_hStdinRead = nullptr;
    m_hStdoutWrite = nullptr;

    m_isRunning = true;
    m_shouldShutdown = false;

    // Start background worker thread
    // Start the background thread that continuously reads engine stdout
    // and parses info/bestmove lines. This thread is required to keep the
    // main render loop non-blocking.
    m_workerThread = std::thread(&EngineManager::backgroundWorker, this);

    // Perform UCI handshake: send "uci" and wait for "uciok"
    if (!sendCommand("uci"))
    {
        std::cerr << "Failed to send uci command" << std::endl;
        shutdown();
        return false;
    }

    if (!waitForResponse("uciok"))
    {
        std::cerr << "Engine did not respond with uciok" << std::endl;
        shutdown();
        return false;
    }

    // Send "isready" and wait for "readyok"
    if (!sendCommand("isready"))
    {
        std::cerr << "Failed to send isready command" << std::endl;
        shutdown();
        return false;
    }

    if (!waitForResponse("readyok"))
    {
        std::cerr << "Engine did not respond with readyok" << std::endl;
        shutdown();
        return false;
    }

    std::cout << "Stockfish engine launched successfully" << std::endl;
    return true;
}

bool EngineManager::sendCommand(const std::string& cmd)
{
    if (!m_isRunning || !m_hStdinWrite)
    {
        return false;
    }

    std::string fullCmd = cmd;
    if (fullCmd.back() != '\n')
    {
        fullCmd += '\n';
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(m_hStdinWrite, fullCmd.c_str(), (DWORD)fullCmd.size(), &bytesWritten, nullptr))
    {
        std::cerr << "Failed to write command to engine: " << GetLastError() << std::endl;
        return false;
    }

    if (!FlushFileBuffers(m_hStdinWrite))
    {
        std::cerr << "Failed to flush stdin: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

bool EngineManager::startSearch(const std::string& fen, int timeMs)
{
    if (!m_isRunning)
    {
        return false;
    }

    // Build the command sequence
    std::string posCmd = "position fen " + fen;
    // Record which side is to move in this position so we can normalize evaluation
    // FEN format: <piece-placements> <active-color> ...
    {
        std::istringstream iss(fen);
        std::string part;
        // skip piece placement
        if (iss >> part)
        {
            if (iss >> part)
            {
                bool whiteToMove = (!part.empty() && part[0] == 'w');
                m_positionSideIsWhite.store(whiteToMove);
            }
        }
    }
    // Use movetime for a fixed thinking time and avoid using wtime/btime which are
    // intended to communicate remaining clock times.
    std::string goCmd = "go movetime " + std::to_string(timeMs);

    if (!sendCommand(posCmd))
    {
        return false;
    }

    // Clear any previous best move
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bestMove.clear();
        m_bestMoveReady = false;
    }

    if (!sendCommand(goCmd))
    {
        return false;
    }

    return true;
}

void EngineManager::stopSearch()
{
    if (!m_isRunning)
    {
        return;
    }

    // Mark that we're aborting this search so we can ignore its bestmove response
    m_searchAborted = true;

    // Send "stop" command to force Stockfish to halt and return current best move.
    // The worker thread will then parse the bestmove and set m_bestMoveReady.
    sendCommand("stop");
}

bool EngineManager::checkBestMove(std::string& outMove)
{
    if (!m_bestMoveReady)
    {
        return false;
    }

    // If the search was aborted, ignore this bestmove
    if (m_searchAborted.load())
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bestMove.clear();
        m_bestMoveReady = false;
        m_searchAborted = false;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_bestMove.empty())
        {
            return false;
        }
        outMove = m_bestMove;
        m_bestMove.clear();
    }

    m_bestMoveReady = false;
    return true;
}

bool EngineManager::isRunning() const
{
    return m_isRunning;
}

void EngineManager::shutdown()
{
    if (!m_isRunning)
    {
        return;
    }

    // Signal shutdown to the worker thread so it can stop reading.
    m_shouldShutdown = true;

    // Try a graceful shutdown: send "quit" to the engine so it can exit cleanly.
    if (m_hStdinWrite)
    {
        // sendCommand requires m_isRunning to be true; keep it true until we've asked the engine to quit
        if (!sendCommand("quit"))
        {
            // If sending failed, still attempt to close the pipe to signal EOF
        }

        // Close our write handle to signal EOF to the child process
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = nullptr;
    }

    // Wait for the engine process to exit gracefully for up to 2 seconds
    const DWORD WAIT_TIMEOUT_MS = 2000;
    if (m_hProcess)
    {
        DWORD waitRes = WaitForSingleObject(m_hProcess, WAIT_TIMEOUT_MS);
        if (waitRes == WAIT_TIMEOUT)
        {
            // Still running after timeout - force termination
            TerminateProcess(m_hProcess, 0);
            WaitForSingleObject(m_hProcess, 1000);
        }

        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }

    // Ensure the background worker thread exits and join it
    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }

    // Close remaining handles used for communication
    if (m_hStdinRead)
    {
        CloseHandle(m_hStdinRead);
        m_hStdinRead = nullptr;
    }
    if (m_hStdoutRead)
    {
        CloseHandle(m_hStdoutRead);
        m_hStdoutRead = nullptr;
    }

    // Mark not running
    m_isRunning = false;

    std::cout << "Stockfish engine shut down" << std::endl;
}

void EngineManager::backgroundWorker()
{
    /**
     * @brief Background reader thread for engine stdout.
     *
     * Continuously reads from the engine stdout pipe, accumulates text, splits
     * lines on '\n', parses info lines (evaluation/mate) and bestmove, and
     * signals waiting callers via atomics and mutex-protected state.
     */
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    std::string accumulator;

    while (!m_shouldShutdown && m_hStdoutRead)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(m_hStdoutRead, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr))
        {
            DWORD err = GetLastError();
            if (err != ERROR_BROKEN_PIPE)
            {
                std::cerr << "Error reading from engine: " << err << std::endl;
            }
            break;
        }

        if (bytesRead == 0)
        {
            // EOF
            break;
        }

        buffer[bytesRead] = '\0';
        accumulator += buffer;

        // Process complete lines
        size_t pos = 0;
        while ((pos = accumulator.find('\n')) != std::string::npos)
        {
            std::string line = accumulator.substr(0, pos);

            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            // Debug output
            std::cout << "[Engine] " << line << std::endl;
            // Parse live evaluation info lines (e.g., "info ... score cp <v>" or "info ... score mate <v>")
            if (line.rfind("info", 0) == 0)
            {
                std::istringstream iss(line);
                std::string token;
                while (iss >> token)
                {
                    if (token == "score")
                    {
                        std::string scoreType;
                        if (!(iss >> scoreType)) break;

                        if (scoreType == "cp")
                        {
                            int cpValue = 0;
                            if (iss >> cpValue)
                            {
                                float eval = (float)cpValue / 100.0f;
                                // Normalize so positive = White advantage
                                if (!m_positionSideIsWhite.load()) eval = -eval;
                                m_currentEvaluation.store(eval);
                                m_isMateDetected.store(false);
                            }
                        }
                        else if (scoreType == "mate")
                        {
                            int mateVal = 0;
                            if (iss >> mateVal)
                            {
                                // Normalize sign so positive = White is winning (mate for White)
                                if (!m_positionSideIsWhite.load()) mateVal = -mateVal;
                                m_mateInMoves.store(mateVal);
                                m_isMateDetected.store(true);
                            }
                        }
                    }
                }
            }
            //std::printf("%f\n", m_currentEvaluation.load());

            // Check for bestmove
            if (line.substr(0, 8) == "bestmove")
            {
                std::string move = parseBestMove(line);
                if (!move.empty())
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bestMove = move;
                    m_bestMoveReady = true;
                }
            }

            // Check for UCI responses
            if (line == "uciok" || line == "readyok")
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_lastLine = line;
                }
                m_responseReceived = true;
            }

            accumulator = accumulator.substr(pos + 1);
        }
    }

    std::cout << "Engine worker thread exiting" << std::endl;
}

bool EngineManager::waitForResponse(const std::string& expectedLine, int timeoutMs)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    while (!m_shouldShutdown)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_lastLine == expectedLine)
            {
                m_responseReceived = false;
                return true;
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed > timeoutMs)
        {
            std::cerr << "Timeout waiting for response: " << expectedLine << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

std::string EngineManager::parseBestMove(const std::string& line)
{
    // Format: "bestmove e2e4" or "bestmove e7e8q" (with promotion)
    // Extract the move token after "bestmove "

    if (line.size() < 9) // "bestmove " is 9 characters
    {
        return "";
    }

    std::istringstream iss(line);
    std::string token;
    iss >> token; // Read "bestmove"

    if (token != "bestmove")
    {
        return "";
    }

    iss >> token; // Read the move
    if (token.empty() || token == "null")
    {
        return "";
    }

    // Validate move format: should be 4 or 5 characters (e.g., e2e4 or e7e8q)
    if (token.size() < 4 || token.size() > 5)
    {
        return "";
    }

    return token;
}
