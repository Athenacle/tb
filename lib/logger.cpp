
#include "logger.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#ifdef UNIX_HAVE_SYS_PRCTL
#include <sys/prctl.h>
#endif

namespace
{
    const size_t timeBuffSize = 128;
    const char timeFormat[] = "%F %T";

    const char* getTimeStamp()
    {
        static char buf[timeBuffSize];
        static struct timespec spec;
        static time_t tv_sec = 0;
        static char* lastEndOfTime = nullptr;
        clock_gettime(CLOCK_REALTIME, &spec);
        if (spec.tv_sec > tv_sec) {
            struct tm* gmt = gmtime(&spec.tv_sec);
            lastEndOfTime = &buf[strftime(buf, timeBuffSize, timeFormat, gmt)];
        }
        snprintf(lastEndOfTime, timeBuffSize - (lastEndOfTime - &buf[0]), ":%-9ld", spec.tv_nsec);
        return buf;
    }
}  // namespace


namespace tb
{
    Logger* Logger::instance = nullptr;
    boost::object_pool<Logger::LogMessageObject>* Logger::objPool = nullptr;
    boost::pool<>* Logger::LogMessageObject::charPool;
    mutex Logger::objPoolMutex;

    void* Logger::start(void* bar, void*, void*)
    {
        if (bar != nullptr) {
            reinterpret_cast<tb::thread_ns::barrier*>(bar)->wait();
        }
        instance->InternalLogWriter();
        return nullptr;
    }

    void Logger::LogMessageObject::Setup(pthread_t _tid,
                                         LogSeverity _severity,
                                         const char* _msg,
                                         const char* _timestamp)
    {
        static const char logSeverityString[][8] = {
            "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"};

        severity = _severity;
        tid = _tid;
        size_t size = strlen(_msg) + strlen(_timestamp) + 64;
        msg = static_cast<char*>(Logger::LogMessageObject::charPool->ordered_malloc(size));
        length = snprintf(msg,
                          size,
                          "%s - [%s] - %lx - %s \n",
                          _timestamp,
                          logSeverityString[_severity],
                          tid,
                          _msg);
    }

    Logger::LogMessageObject::~LogMessageObject()
    {
        Logger::LogMessageObject::charPool->free(msg);
    }

    void Logger::StartThread(barrier* b)
    {
        begin(b);
    }

    Logger::~Logger()
    {
        for (auto& bd : fileBackends) {
            close(std::get<FD>(bd));
            delete[] std::get<FILENAME>(bd);
        }
        close(PRESERVED_STDOUT_FILENO);
    }

    Logger::Logger() : thread("logger")
    {
        dup2(STDOUT_FILENO, PRESERVED_STDOUT_FILENO);
        stopping = false;
        newLog = true;
    }

    void Logger::LogProcessor(pthread_t tid, LogSeverity severity, const char* msg)
    {
        static auto& m = Logger::objPoolMutex;
        bool log = false;
        acceptNewLog.lock();
        log = Logger::instance->newLog;
        acceptNewLog.unlock();
        if (log) {
            // only print log when accept

            m.lock();
            std::cout << pthread_self() << std::endl;
            LogMessageObject* msgObj = Logger::objPool->construct();
            msgObj->Setup(tid, severity, msg, getTimeStamp());
            m.unlock();

            msgQueueMutex.lock();
            this->msgQueue.push(msgObj);
            msgQueueCond.notify_all();
            msgQueueMutex.unlock();
        }
    }

    void Logger::InternalLogWriter()
    {
        static auto& m = Logger::objPoolMutex;
        while (true) {
            msgQueueCond.wait(msgQueueMutex,
                              [this] { return stopping || this->msgQueue.size() > 0; });
            if (stopping) {
#ifdef USE_POSIX_THREAD
                msgQueueMutex.unlock();
#endif
                return;
            }
            LogMessageObject* obj = msgQueue.front();
#ifdef USE_CXX_THREAD
            msgQueueMutex.lock();
#endif
            msgQueue.pop();
            msgQueueMutex.unlock();

            if (fileBackends.size() > 0) {
                backendsMutex.lock();
                for (auto& bd : fileBackends) {
                    if (std::get<SEVERITY>(bd) > obj->severity) {
                        continue;
                    } else {
                        int fd = std::get<FD>(bd);
                        write(fd, obj->msg, obj->length);
                    }
                }
                backendsMutex.unlock();
            } else {
                write(STDERR_FILENO, obj->msg, obj->length);
            }
            m.lock();
            Logger::objPool->destroy(obj);
            m.unlock();
        }
    }

    Logger& Logger::AddConsoleBackend(const LogSeverity defaultSeverity)
    {
        backendsMutex.lock();
        int fd = dup(PRESERVED_STDOUT_FILENO);
        fileBackends.push_back(std::make_tuple(fd, defaultSeverity, nullptr));
        backendsMutex.unlock();
        return *this;
    }

    Logger& Logger::AddFileBackend(const char* filename,
                                   const LogSeverity defaultSeverity,
                                   bool append)
    {
        const unsigned int openFlag = O_CREAT | O_WRONLY;
        int fd = -1;
        if (append) {
            fd = open(filename, openFlag | O_APPEND);
        } else {
            fd = open(filename, openFlag);
        }
        if (fd == -1) {
            size_t size = strlen(filename) + 64;
            char* buf = new char[size];
            snprintf(buf, size, "Open File %s for logging Failed: %s", filename, strerror(errno));
            log_FATAL(buf);
            delete[] buf;
        } else {
            if (!append) {
                ftruncate(fd, 0);
            }
            fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            char* path = new char[strlen(filename) + 1];
            strcpy(path, filename);
            backendsMutex.lock();
            fileBackends.push_back(std::make_tuple(fd, defaultSeverity, path));
            backendsMutex.unlock();
        }
        return *this;
    }

    void Logger::DestoryLogger()
    {
        auto& accept = Logger::instance->acceptNewLog;
        auto& msgMtx = Logger::instance->msgQueueMutex;
        auto& cond = Logger::instance->msgQueueCond;
        accept.lock();
        Logger::instance->newLog = false;
        accept.unlock();
        do {
            msgMtx.lock();
            auto value = Logger::instance->msgQueue.size();
            msgMtx.unlock();
            if (value > 0) {
                sleep(1);
            } else {
                Logger::instance->stopping = true;
                msgMtx.lock();
                cond.notify_all();
                msgMtx.unlock();
                instance->join();
                delete Logger::instance;
                delete Logger::objPool;
                Logger::LogMessageObject::charPool->~pool<>();
                free(Logger::LogMessageObject::charPool);
                Logger::objPool = nullptr;
                Logger::instance = nullptr;
                break;
            }
        } while (true);
    }

    Logger& Logger::getLogger(barrier* b)
    {
        if (Logger::instance == nullptr) {
            Logger::objPool = new boost::object_pool<Logger::LogMessageObject>;
            Logger::instance = new Logger();
            Logger::instance->StartThread(b);
            auto pool =
                static_cast<boost::pool<>*>(malloc(sizeof *Logger::LogMessageObject::charPool));
            Logger::LogMessageObject::charPool = new (pool) boost::pool<>(sizeof(char));
        }
        return *Logger::instance;
    }
}  // namespace tb
