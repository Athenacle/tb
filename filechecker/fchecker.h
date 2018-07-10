

#ifndef FCHECKER_H
#define FCHECKER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dirent.h>
#include <unistd.h>

#include <memory>
#include <queue>
#include <string>
#include <cstring> 
#include <json/json.h>
#include "taobao.h"

#include "boost/pool/pool.hpp"

using boost::pool;
using std::queue;
using std::string;


void* fcheckerHandler(void*);

using std::string;

class SystemConfig
{
public:
    string path;
    pool<> _pool;
    string rawPath;
    string productPath;
    bool chRoot;
    bool useInotify;
    int uid, gid;
    bool forkToBackground;


    SystemConfig() : _pool(sizeof(char), 1 << 12)  // allocate 4096KB from system
    {
#ifdef USE_INOTIFY
        inotifyFD = 0;
#endif
    }

    ~SystemConfig()
    {
#ifdef USE_INOTIFY
        close(inotifyFD);
#endif
    }


#ifdef USE_INOTIFY
    int inotifyFD;
#endif
};

// global ====
extern SystemConfig globalConfig;

inline char* requestMemory(unsigned long _size)
{
    return reinterpret_cast<char*>(globalConfig._pool.ordered_malloc(_size));
}

inline void releaseMemory(const void* ptr)
{
    if (ptr == nullptr)
        return;
    globalConfig._pool.ordered_free(const_cast<void*>(ptr));
}

inline char* stringDUP(const char* s)
{
    if (s == nullptr)
        return nullptr;
    char* ret = requestMemory(strlen(s) + 10);
    strcpy(ret, s);
    return ret;
}

class CharPointer
{
public:
    const char* pointer;

    operator const char*()
    {
        return pointer;
    }

    CharPointer(const char* _pointer)
    {
        pointer = stringDUP(_pointer);
    }

    CharPointer(CharPointer& p) = delete;  // {this->pointer = p;};
    CharPointer() = delete;

    ~CharPointer()
    {
        releaseMemory(const_cast<char*>(pointer));
    }
};

using StringPointer = std::shared_ptr<CharPointer>;

// global ===


namespace fc
{
    using tb::thread_ns::condition_variable;
    using tb::thread_ns::mutex;
    using tb::thread_ns::thread;

    class Item
    {
        const char* PIC_1;
        const char* PIC_2;
        const char* PIC_3;

    public:
        Item(const char*, const char*, const char*);
        ~Item()
        {
            releaseMemory(PIC_1);
            releaseMemory(PIC_2);
            releaseMemory(PIC_3);
        }
    };

    class FcHandler
    {
        friend void* fcheckerHandler(void*);

    private:
        static FcHandler* instance;
        const SystemConfig& config;
        mutex queueMutex;

        FcHandler(const SystemConfig&);
        void setProductPath(const char*);

    public:
        static FcHandler& getHandler();
        void destroyHandler();
        void AddDirectory(const char*);
        void AddDirectory(DIR*, StringPointer&);
    };

    void Start(FcHandler&);
}  // namespace fc


void fcExit();
void fcInit(const char*);
void parseArguments(int, char* []);
void version(const char*);
void StartLog(const Json::Value&, tb::thread_ns::barrier*);
void StartSystem(const Json::Value&);

#endif
