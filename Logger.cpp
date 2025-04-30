#include "Logger.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <sstream>

Logger::Logger()
{
    try
    {
        std::filesystem::path BASE_FOLDER(".");
        std::string fileName = generateLogFilename();
        std::filesystem::path logPath = BASE_FOLDER / fileName;
        logFile_.open(logPath, std::ios::app);
    }
    catch (std::runtime_error const &e)
    {
        std::cerr << "Failed to create an Logger object. The server wiil shut down immediately." << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

std::string Logger::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000; // Get milliseconds

    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str() + ']';
}

std::string Logger::generateLogFilename() const
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y_%m_%d_%H%M%S") << ".log";
    return oss.str();
}

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}


void Logger::log(std::string const& msg)
{
    std::lock_guard<std::mutex> lock(mutex_); // no two threads try to log to the logfile simultaneously
    std::string const timestamp = getCurrentTimestamp();
    logFile_ << timestamp << " - " << "[" << msg << "]" << std::endl;
    std::cout << timestamp << " - " << "[" << msg << "]" << std::endl;
}

Logger::~Logger()
{
    if (logFile_.is_open())
    {
        logFile_.flush();
        logFile_.close();
    }
}
