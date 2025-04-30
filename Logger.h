#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>

class Logger
{
    std::ofstream logFile_;
    std::mutex mutex_;
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    std::string getCurrentTimestamp() const;
    std::string generateLogFilename() const;

public:
    static Logger& getInstance();
    void log(std::string const& msg);
    ~Logger();
};

#endif // LOGGER_H
