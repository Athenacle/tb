

#ifndef FCHECKER_H
#define FCHECKER_H

#include "image.h"
#include "remote.h"
#include "taobao.h"
#include "threads.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dirent.h>
#include <unistd.h>

#include <json/json.h>
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

#ifdef BUILD_WITH_LIBSSH
    string sftpAddress;
    string sftpUsername;
    string sftpPassword;
    string sftpPassphrase;
    string sftpIndentifyPath;
    string sftpRemotePath;
    bool sftpEnable;
    int sftpPort;
#endif

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
        void getName(const char*& p1, const char*& p2, const char*& p3)
        {
            p1 = PIC_1;
            p2 = PIC_2;
            p3 = PIC_3;
        }
        int processing();
        Item(const char*, const char*, const char*);
        ~Item()
        {
            releaseMemory(PIC_1);
            releaseMemory(PIC_2);
            releaseMemory(PIC_3);
            log_INFO("~Item");
        }
    };

    using queueItemNext = std::function<void(std::shared_ptr<Item>)>;

    class ItemRemoteTimer : public thread
    {
        using fn = std::function<bool(Item*)>;
        virtual void* start(void*, void*, void*) override
        {
            do {
                sleep(_int);
                _m.lock();
                auto r = processing(_q);
                _m.unlock();
                if (r) {
                    break;
                }
            } while (true);
            return nullptr;
        }


        unsigned int _int;

    protected:
        std::queue<std::shared_ptr<Item>> _q;
        mutex _m;

        using queueType = decltype(_q);
        virtual bool processing(queueType&) = 0;
        ItemRemoteTimer(int _i, const char* n) : thread(n), _int(_i) {}

    public:
        void addItem(std::shared_ptr<Item> _i)
        {
            _m.lock();
            _q.emplace(_i);
            _m.unlock();
        }
    };

    class MySQLTimer : public ItemRemoteTimer
    {
        using sql = tb::remote::MySQLWorker;

        sql& instance;
        virtual bool processing(queueType&) override;

    public:
        MySQLTimer()
            : ItemRemoteTimer(5, "mysql"), instance(tb::remote::MySQLWorker::getMySQLInstance())
        {
        }
    };


    using tb::thread_ns::condition_variable;
    using tb::thread_ns::mutex;

    class ItemSchedular;
    class ItemProcessor;
#ifdef BUILD_WITH_LIBSSH
    class SFTP : public thread
    {
        std::queue<std::shared_ptr<Item>> _q;
        using queueType = decltype(_q);

        mutex _m;
        condition_variable _cv;

        tb::remote::SFTPWorker& sftp;
        virtual void* start(void*, void*, void*);

    public:
        SFTP() : thread("sftp"), sftp(tb::remote::SFTPWorker::getSFTPInstance()) {}

        void addItem(std::shared_ptr<Item> _i)
        {
            _m.lock();
            _q.emplace(_i);
            _cv.notify_all();
            _m.unlock();
        }
    };
#endif

    class ItemSchedular
    {
        std::queue<Item*> items;
        mutex queueMutex;
        bool work;
        condition_variable _cv;
        int processCount;
        std::vector<ItemProcessor*> processors;

        MySQLTimer sql;
#ifdef BUILD_WITH_LIBSSH
        SFTP sftp;
#endif

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
        queueItemNext mysqlNext;
        queueItemNext sftpNext;

    public:
        virtual ~ItemProcessor(){};
        ItemProcessor(ItemSchedular&, queueItemNext, queueItemNext);
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
}  // namespace fc


void fcExit();
void fcInit(const char*);
void parseArguments(int, char* []);
void version(const char*);
void StartLog(const Json::Value&, tb::thread_ns::barrier*);
void StartSystem(const Json::Value&);
void StartMYSQL();
void StartRemote(const Json::Value&);
void StartSFTP();
#endif
