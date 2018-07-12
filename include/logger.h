


#ifndef LOG_H
#define LOG_H
#include "taobao.h"
#include "threads.h"

#include <sys/types.h>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

using std::queue;
using std::string;
using std::tuple;
using std::vector;


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

    class Logger : public tb::thread_ns::thread
    {
        struct LogMessageObject {
            static boost::pool<>* charPool;

            char* msg;
            size_t length;
            LogSeverity severity;
            pthread_t tid;

            LogMessageObject(const LogMessageObject&) = delete;
            LogMessageObject() {}
            void Setup(const char*, LogSeverity, const char*, const char*);
            ~LogMessageObject();
        };

    private:
        virtual void* start(void*, void*, void*) override;

        static mutex objPoolMutex;
        static boost::object_pool<Logger::LogMessageObject>* objPool;
        static Logger* instance;

        vector<FileLogObject> fileBackends;

        queue<LogMessageObject*> msgQueue;

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

        void StartThread(barrier*);

    public:
        Logger& AddFileBackend(const char*, const LogSeverity = INFO, bool = false);
        Logger& AddConsoleBackend(const LogSeverity = INFO);
        void LogProcessor(const char*, LogSeverity, const char*);
        static Logger& getLogger(barrier* = nullptr);
        static void DestoryLogger();
    };
}  // namespace tb


#define BUILD_FUNCTION(severity)                                                                 \
    inline void log_##severity(const char* msg)                                                  \
    {                                                                                            \
        tb::Logger::getLogger().LogProcessor(tb::thread_ns::GetThreadName(), tb::severity, msg); \
    }


BUILD_FUNCTION(TRACE);
BUILD_FUNCTION(DEBUG);
BUILD_FUNCTION(INFO);
BUILD_FUNCTION(WARNING);
BUILD_FUNCTION(ERROR);
BUILD_FUNCTION(FATAL);

#endif
