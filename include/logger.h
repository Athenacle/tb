


#ifndef LOG_H
#include "taobao.h"

#include <sys/types.h>
#include <queue>
#include <string>
#include <tuple>
#include <vector>


using std::queue;
using std::string;
using std::tuple;
using std::vector;


void* StartLog(void*);

namespace tb
{
    enum LogSeverity {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
        FATAL = 5,
        NONE = 6
    };

    const int FD = 0;
    const int SEVERITY = 1;
    const int FILENAME = 2;

    using FileLogObject = tuple<int, LogSeverity, const char*>;


    const int PRESERVED_STDOUT_FILENO = 999;

    using namespace tb::thread_ns;

    class Logger
    {
        friend void* ::StartLog(void*);

        struct LogMessageObject {
            char* msg;
            size_t length;
            LogSeverity severity;
            pthread_t tid;

            LogMessageObject(pthread_t, LogSeverity, const char*, const char*);
            ~LogMessageObject();
        };

    private:
        static Logger* instance;

        vector<FileLogObject> fileBackends;

        queue<LogMessageObject*> msgQueue;

        tid TID;
        thread logger;
        condition_variable msgQueueCond;
        mutex msgQueueMutex;
        mutex backendsMutex;
        mutex acceptNewLog;

        bool newLog;
        bool stopping;


        Logger();
        ~Logger();
        void SetAccept()
        {
            newLog = true;
        }

        void InternalLogWriter();

        void StartThread();

    public:
        Logger& AddFileBackend(const char*, const LogSeverity = INFO);
        Logger& AddConsoleBackend(const LogSeverity = INFO);
        void LogProcessor(pthread_t, LogSeverity, const char*);
        static Logger& getLogger();
        static void DestoryLogger();
    };
}  // namespace tb

#define BASE_MACRO(severity, tid, msg)                                \
    do {                                                              \
        tb::Logger::getLogger().LogProcessor(tid, tb::severity, msg); \
    } while (false);

#define BUILD_FUNCTION(severity)                  \
    inline void log_##severity(const char* msg)   \
    {                                             \
        BASE_MACRO(severity, pthread_self(), msg) \
    }

BUILD_FUNCTION(TRACE);
BUILD_FUNCTION(DEBUG);
BUILD_FUNCTION(INFO);
BUILD_FUNCTION(WARNING);
BUILD_FUNCTION(ERROR);
BUILD_FUNCTION(FATAL);

#endif
