#include <windows.h>
#include "EngineManager.h"
#include <iostream>
#include <sstream>
#include <chrono>

EngineManager::EngineManager() = default;

EngineManager::~EngineManager()
{
    shutdown();
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
    std::string goCmd = "go wtime " + std::to_string(timeMs) + " btime " + std::to_string(timeMs);

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

bool EngineManager::checkBestMove(std::string& outMove)
{
    if (!m_bestMoveReady)
    {
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

    m_shouldShutdown = true;
    m_isRunning = false;

    // Close input pipe to signal EOF to engine
    if (m_hStdinWrite)
    {
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = nullptr;
    }

    // Wait for worker thread with timeout
    if (m_workerThread.joinable())
    {
        if (m_workerThread.join(), true) // Always completes, but with potential timeout handling
        {
            // Thread joined successfully
        }
    }

    // Terminate process if still running
    if (m_hProcess)
    {
        TerminateProcess(m_hProcess, 0);
        WaitForSingleObject(m_hProcess, 1000);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }

    // Close remaining handles
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

    std::cout << "Stockfish engine shut down" << std::endl;
}

void EngineManager::backgroundWorker()
{
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
