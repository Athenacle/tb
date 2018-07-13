

#ifndef FCHECKER_H
#define FCHECKER_H

#include "image.h"
#include "taobao.h"
#include "threads.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dirent.h>
#include <unistd.h>

#include <json/json.h>
#include <mysql.h>
#include <cstring>
#include <memory>
#include <queue>
#include <string>

using std::queue;
using std::string;


void* fcheckerHandler(void*);

using std::string;
using tb::utils::releaseMemory;
using tb::utils::requestMemory;

class SystemConfig
{
public:
    string path;
    string rawPath;
    string productPath;
    bool chRoot;
    bool useInotify;
    int uid, gid;
    bool forkToBackground;
    int threadCount;

    bool mysqlEnable;
    bool mysqlCompress;
    string mysqlAddress;
    int mysqlPort;
    string mysqlUserName;
    string mysqlPassword;
    string mysqlDB;

    static void buildDefaultSystemConfig();

    SystemConfig()
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

        std::string bcode;
        std::string fcode;
        int price;

    public:
        int processing();
        Item(const char*, const char*, const char*);
        ~Item()
        {
            releaseMemory(PIC_1);
            releaseMemory(PIC_2);
            releaseMemory(PIC_3);
        }
    };


    using tb::thread_ns::condition_variable;
    using tb::thread_ns::mutex;

    class ItemSchedular;
    class ItemProcessor;

    class ItemSchedular
    {
        std::queue<Item*> items;
        mutex queueMutex;
        bool work;
        condition_variable _cv;
        int processCount;
        std::vector<ItemProcessor*> processors;

        static ItemSchedular* instance;
        ItemSchedular();

    public:
        static ItemSchedular& getSchedular();
        void buildProcessor(int = 1);
        void stopSchedular();
        int addItem(Item*);
        Item* getItem();
    };

    class ItemProcessor : public tb::thread_ns::thread
    {
        ItemSchedular& sched;

    public:
        virtual ~ItemProcessor(){};
        ItemProcessor(ItemSchedular&);
        virtual void* start(void* = nullptr, void* = nullptr, void* = nullptr) override;
    };

    class FcHandler
    {
        friend void* fcheckerHandler(void*);

    private:
        static FcHandler* instance;
        const SystemConfig& config;
        mutex queueMutex;

        ItemSchedular& itemSched;

        FcHandler(const SystemConfig&);
        void setProductPath(const char*);

    public:
        static FcHandler& getHandler();
        void destroyHandler();
        void AddDirectory(const char*);
        void AddDirectory(DIR*, StringPointer&);
    };

    void Start(FcHandler&);

    class MySQLWorker : public thread
    {
        using csr = const string&;
        enum { CONNECTION_NOT_REAL_CONNECT = 0, CONNECTION_FAILED = 1, CONNECTION_SUCCESS = 2 };
        static MySQLWorker* instance;

        int status;

        MYSQL* _remote;
        int errNo;
        const char* errString;
        string addr;
        string user;
        string pass;
        string db;
        unsigned int port;
        bool compress;

        MySQLWorker(const MySQLWorker&) = delete;
        MySQLWorker(csr, csr, csr, csr, unsigned int, bool);

        void doConnect();

    public:
        virtual void* start(void*, void*, void*) override;

        virtual ~MySQLWorker();

        bool tryConnect(const char**);
        const char* getErrorString() const;
        MySQLWorker* getMySQLInstance();
        static MySQLWorker& initMySQLInstance(csr, csr, csr, csr, unsigned int, bool);
    };

}  // namespace fc


void fcExit();
void fcInit(const char*);
void parseArguments(int, char* []);
void version(const char*);
void StartLog(const Json::Value&, tb::thread_ns::barrier*);
void StartSystem(const Json::Value&);
void StartMySql();
void StartRemote(const Json::Value&);

#endif
