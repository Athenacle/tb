

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

#include <cstdint>

#include <boost/filesystem.hpp>

using namespace boost::filesystem;
using std::queue;
using std::string;

void* fcheckerHandler(void*);

using std::string;
using tb::utils::releaseMemory;
using tb::utils::requestMemory;

class SystemConfig
{
private:
    mutable tb::thread_ns::mutex _m;

public:
    path rootPath;
    path rawPath;
    path productPath;
    bool chRoot;
    bool useInotify;
    bool deleteRaw;
    int uid, gid;
    bool forkToBackground;
    int threadCount;
    int productPrefixLength;

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

    std::map<string, uint64_t> dirs;

    static void buildDefaultSystemConfig();

    uint64_t getDirectoryID(const string&);


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

        path destPIC[3];

        bool ok;
        Image front;
        Image back;
        Image board;

        OcrResult ocr;

        std::string bcode;
        std::string fcode;
        int price;
        int ocrfailed;

        int roi[4];

    public:
        bool getOK() const
        {
            return ok;
        }
        void ocrFailed()
        {
            this->ocrfailed++;
        }
        int getFailed() const
        {
            return this->ocrfailed;
        }

        int processingAccurateOCR(int&, bool = false);
        int processingOCR(int&);

        int getBarCode(string& c)
        {
            auto ret = board.getBarCode(c, roi);
            this->bcode = c;
            return ret;
        }

        void getCode(string&, string&, int&);

        void getOcrJson(string&);

        const OcrResult& getOcrResult() const
        {
            return ocr;
        }

        const char* getBoardName() const
        {
            return PIC_3;
        }

        void getDestName(path& p1, path& p2, path& p3)
        {
            p1 = destPIC[0];
            p2 = destPIC[1];
            p3 = destPIC[2];
        }

        void getName(const char*& p1, const char*& p2, const char*& p3)
        {
            p1 = PIC_1;
            p2 = PIC_2;
            p3 = PIC_3;
        }
        int processing();
        void SaveFile(const path&, bool = false, const string& code = "");

        Item(const char*, const char*, const char*);

        void setDestPath(path&, path&, path&);

        const int* getRoI() const
        {
            return roi;
        }
        ~Item();
    };

    using queueItemNext = std::function<void(std::shared_ptr<Item>)>;

    class ItemRemoteTimer : public thread
    {
        using fn = std::function<bool(Item*)>;

        using qsi = std::queue<std::shared_ptr<Item>>;

        virtual void* start(void*, void*, void*) override
        {
            static qsi qu;
            do {
                sleep(_int);
                _m.lock();
                std::swap(qu, _q);
                while (_q.size() > 0) {
                    _q.pop();
                }
                _m.unlock();
                auto r = processing(qu);
                if (r && _q.size() == 0) {
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
            : ItemRemoteTimer(10, "mysql"), instance(tb::remote::MySQLWorker::getMySQLInstance())
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

    class ItemSchedular;
    class OcrHandlerQueue;
    class ItemProcessor;

    class ItemSchedular
    {
        std::queue<Item*> items;
        mutex queueMutex;
        bool work;
        condition_variable _cv;
        int processCount;
        std::vector<ItemProcessor*> processors;
        OcrHandlerQueue* ocr;

        MySQLTimer sql;
#ifdef BUILD_WITH_LIBSSH
        SFTP sftp;
#endif

        static ItemSchedular* instance;
        ItemSchedular();

    public:
        ~ItemSchedular();
        static ItemSchedular& getSchedular();
        static void destroyItemSchedular();
        void buildProcessor(int = 1);
        void stopSchedular();
        int addItem(Item*);
        Item* getItem();
    };


    class OcrHandlerQueue : public tb::thread_ns::thread
    {
        ItemSchedular& sched;
        queueItemNext mysql;
        queueItemNext sftp;
        std::queue<Item*> _q;
        mutex _m;
        condition_variable _cv;

    public:
        OcrHandlerQueue(ItemSchedular&, queueItemNext, queueItemNext);
        virtual ~OcrHandlerQueue();
        virtual void* start(void* = nullptr, void* = nullptr, void* = nullptr) override;
        void addItem(Item*);
    };

    class ItemProcessor : public tb::thread_ns::thread
    {
        ItemSchedular& sched;
        queueItemNext mysqlNext;
        OcrHandlerQueue& ocr;

    public:
        virtual ~ItemProcessor(){};
        ItemProcessor(ItemSchedular&, OcrHandlerQueue&);
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
