
#include "fchecker.h"
#include "logger.h"
#include "remote.h"

#include <sys/stat.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

namespace
{
    int getNumber(const char* s)
    {
        int ret = 0;
        bool matchNumber = false;
        while (*s) {
            if (*s == '.') {
                break;
            }
            if (isdigit(*s)) {
                matchNumber = true;
                ret = ret * 10 + *s - '0';
            } else if ((*s == '-' || *s == '_' || isalpha(*s)) && matchNumber) {
                char* buf = requestMemory(strlen(s) + 32);
                sprintf(buf, "Parse FileNumber of %s Failed", s);
                log_ERROR(buf);
                releaseMemory(buf);
                return -1;
            }
            s++;
        }
        return ret;
    }

    const unsigned long nameMax = 255;

}  // namespace

SystemConfig globalConfig;


namespace fc
{
    FcHandler* FcHandler::instance = nullptr;
    ItemSchedular* ItemSchedular::instance = nullptr;

    // FcHandler ==================
    FcHandler::FcHandler(const SystemConfig& _config)
        : config(_config), itemSched(ItemSchedular::getSchedular())
    {
    }

    void FcHandler::AddDirectory(const char* p)
    {
        char* buffer = requestMemory(strlen(p) + 128);
        DIR* dir = opendir(p);
        if (dir == nullptr) {
            sprintf(buffer, "Open Directory %s Failed: %s", p, strerror(errno));
            log_ERROR(buffer);
        } else {
            sprintf(buffer, "Open Directory %s Success", p);
            log_TRACE(buffer);
            auto parent = std::make_shared<CharPointer>((p));
            AddDirectory(dir, parent);
        }
        releaseMemory(buffer);
    }


    void FcHandler::AddDirectory(DIR* dir, StringPointer& parent)
    {
        const int FNAME = 0;
        const int NUMBER = 1;

        using imgType = std::tuple<const char*, int>;
        vector<imgType> IMGs;

        auto parentPTR = parent.get()->pointer;
        auto length = strlen(*parent.get());
        char* name = requestMemory(length + nameMax + 10);
        strcpy(name, parentPTR);
        char* end = name + length;
        do {
            dirent* entry = readdir(dir);
            if (entry == nullptr) {
                break;
            }
            const char* fname = entry->d_name;
            if (*fname == '.') {
                continue;
                // skip file name started with .
            } else {
                *end = '/';
                *(end + 1) = 0;
                strcpy(end + 1, fname);
                struct stat st;
                int statRet = stat(name, &st);
                if (statRet == -1) {
                    char* buffer = requestMemory(length + nameMax + 128);
                    sprintf(buffer,
                            "System Call \"stat\" failed with file %s: %s",
                            name,
                            strerror(errno));
                    log_ERROR(buffer);
                    releaseMemory(buffer);
                    continue;
                }
                if (S_ISREG(st.st_mode)) {
                    //regular file.
                    IMGs.push_back(std::make_tuple<>(stringDUP(name), getNumber(name)));
                } else if (S_ISDIR(st.st_mode)) {
                    // directory
                    AddDirectory(name);
                } else {
                    char* buffer = requestMemory(length + nameMax + 128);
                    sprintf(buffer, "Unknown file type %s", name);
                    log_ERROR(buffer);
                    releaseMemory(buffer);
                }
            }
        } while (true);
        releaseMemory(name);
        closedir(dir);
        if (IMGs.size() % 3 != 0) {
            char* buffer = requestMemory(length + 128);
            sprintf(buffer, "Image Number in directory %s cannot be divided by 3 well.", parentPTR);
            log_ERROR(buffer);
            releaseMemory(buffer);
        } else {
            std::sort(IMGs.begin(), IMGs.end(), [&](auto& a, auto& b) {
                return std::get<NUMBER>(a) - std::get<NUMBER>(b);
            });
            for (vector<imgType>::size_type i = 0; i < IMGs.size();) {
                auto p1 = IMGs[i];
                i++;
                auto p2 = IMGs[i];
                i++;
                auto p3 = IMGs[i];
                i++;
                auto it = new Item(std::get<FNAME>(p1), std::get<FNAME>(p2), std::get<FNAME>(p3));
                ItemSchedular::getSchedular().addItem(it);
            }
        }
        for (auto& ref : IMGs) {
            releaseMemory(std::get<FNAME>(ref));
        }
    }

    FcHandler& FcHandler::getHandler()
    {
        if (FcHandler::instance == nullptr) {
            instance = new FcHandler(globalConfig);
        }
        return *FcHandler::instance;
    }

    void FcHandler::destroyHandler()
    {
        delete FcHandler::instance;
        FcHandler::instance = nullptr;
    }

    // FcHandler END ====================

    // ItemSchedular

    void ItemSchedular::destroyItemSchedular()
    {
        delete instance;
    }

    ItemSchedular::ItemSchedular() {}

    ItemSchedular::~ItemSchedular()
    {
        delete ocr;
    }

    int ItemSchedular::addItem(Item* i)
    {
        queueMutex.lock();
        items.emplace(i);
        int size = items.size();
        _cv.notify_all();
        queueMutex.unlock();
        return size;
    }

    void ItemSchedular::buildProcessor(int count)
    {
        if (count <= 0) {
            count = 1;
        }
        processCount = count;
        queueItemNext mNext = std::bind(&MySQLTimer::addItem, &sql, std::placeholders::_1);
        queueItemNext sNext =
#ifdef BUILD_WITH_LIBSSH
            std::bind(&SFTP::addItem, &sftp, std::placeholders::_1);
#else
            [](std::shared_ptr<Item>) {};
#endif

        ocr = new OcrHandlerQueue(*this, mNext, sNext);
        for (int i = 0; i < count; i++) {
            processors.emplace_back(new ItemProcessor(*this, *ocr));
        }
        for (auto& p : processors) {
            p->begin();
        }
        ocr->begin();
        sql.begin();
#ifdef BUILD_WITH_LIBSSH
        sftp.begin();
#endif
    }

    Item* ItemSchedular::getItem()
    {
        _cv.wait(queueMutex, [this] { return this->items.size() > 0; });
        auto r = items.front();
        items.pop();
        queueMutex.unlock();
        return r;
    }
    // ItemSchedular END
    // ItemProcessor

    ItemProcessor::ItemProcessor(ItemSchedular& _sched, OcrHandlerQueue& _ocr)
        : thread("iprocess"), sched(_sched), ocr(_ocr)

    {
    }

    void* ItemProcessor::start(void*, void*, void*)
    {
        do {
            auto i = ItemSchedular::getSchedular().getItem();
            if (i == nullptr) {
                break;
            }
            i->processing();
            usleep(rand() % 200000);
            ocr.addItem(i);
        } while (true);
        return nullptr;
    }

    void ItemSchedular::stopSchedular()
    {
        for (int i = 0; i < processCount; i++) {
            ItemSchedular::getSchedular().addItem(nullptr);
        }
        for (auto& thr : processors) {
            thr->join();
            delete thr;
        }
#ifdef BUILD_WITH_LIBSSH
        sftp.addItem(nullptr);
        sftp.join();
#endif
        ocr->addItem(nullptr);
        ocr->join();
        sql.addItem(nullptr);
        sql.join();
    }

    ItemSchedular& ItemSchedular::getSchedular()
    {
        if (instance == nullptr) {
            instance = new ItemSchedular();
        }
        return *instance;
    }

    //ItemProcessor
    // item

    Item::Item(const char* _p1, const char* _p2, const char* _p3)
        : PIC_1(stringDUP(_p1)), PIC_2(stringDUP(_p2)), PIC_3(stringDUP(_p3))
    {
    }

    int Item::processing()
    {
        char* buf = requestMemory(1024);
        sprintf(
            buf, "Processing Item: %s %s %s, at thread %lx ", PIC_1, PIC_2, PIC_3, pthread_self());
        log_INFO(buf);
        releaseMemory(buf);
        return 0;
    }

    // item
#ifdef BUILD_WITH_LIBSSH
    void* SFTP::start(void*, void*, void*)
    {
        static char buffer[1024];
        do {
            _cv.wait(_m, [this] { return _q.size() > 0; });

            auto p = _q.front();
            if (p == nullptr) {
                break;
            }
            _q.pop();
            _m.unlock();

            const char* pname[3];
            p->getName(pname[0], pname[1], pname[2]);
            snprintf(buffer, 1024, "Uploading %s %s %s", pname[0], pname[1], pname[2]);
            log_INFO(buffer);
        } while (true);
        _m.unlock();
        return nullptr;
    }
#endif

    bool MySQLTimer::processing(std::queue<std::shared_ptr<Item>>& _q)
    {
        log_INFO("Inserting ..... into mysql");
        bool ret = false;
        while (_q.size() > 0) {
            auto p = _q.front();
            if (p == nullptr) {
                ret = true;
            }
            _q.pop();
        }
        return ret;
    }

    OcrHandlerQueue::~OcrHandlerQueue() {}

    void OcrHandlerQueue::addItem(Item* i)
    {
        _m.lock();
        _q.emplace(i);
        _cv.notify_all();
        _m.unlock();
    }

    void* OcrHandlerQueue::start(void*, void*, void*)
    {
        do {
            _cv.wait(_m, [this] { return _q.size() > 0; });
            auto i = _q.front();
            _q.pop();
            _m.unlock();
            if (i == nullptr) {
                return nullptr;
            }
            i->processing();
            std::shared_ptr<Item> iptr;
            iptr.reset(i);
            mysql(iptr);
            sftp(iptr);
        } while (true);
    }

    OcrHandlerQueue::OcrHandlerQueue(ItemSchedular& _sched,
                                     queueItemNext sqlnext,
                                     queueItemNext sshnext)
        : thread("ocr"), sched(_sched), mysql(sqlnext), sftp(sshnext)
    {
    }


    void Start(FcHandler& handler)
    {
        handler.AddDirectory(globalConfig.rawPath.c_str());
    }
}  // namespace fc


int main(int argc, char* argv[])
{
    srand(time(nullptr));
    tb::SetThreadName("main");
    if (argc > 1)
        fcInit(argv[1]);
    else
        version(argv[0]);
    auto& handler = fc::FcHandler::getHandler();
    auto& sc = fc::ItemSchedular::getSchedular();
    sc.buildProcessor(globalConfig.threadCount);
    fc::Start(handler);
    sc.stopSchedular();

    tb::Logger::DestoryLogger();
    fc::ImageProcessingDestroy();
    tb::remote::MySQLWorker::destroyMySQLInstance();
#ifdef BUILD_WITH_LIBSSH
    tb::remote::SFTPWorker::destrypSFTPInstance();
#endif
    fc::ItemSchedular::destroyItemSchedular();
}
