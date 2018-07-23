
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

        const char* end = s;
        for (; *end; end++)
            ;

        for (; end > s && *end != '/'; end--)
            ;

        end++;
        while (*end) {
            if (*end == '.') {
                break;
            }
            if (isdigit(*end)) {
                matchNumber = true;
                ret = ret * 10 + *end - '0';
            } else if ((*end == '-' || *end == '_' || isalpha(*end)) && matchNumber) {
                char* buf = requestMemory(strlen(s) + 32);
                sprintf(buf, "Parse FileNumber of %s Failed", s);
                log_ERROR(buf);
                releaseMemory(buf);
                return -1;
            }
            end++;
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
            string path = p;
            tb::utils::formatDirectoryPath(path);
            p = stringDUP(path.c_str());
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
        string name;
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
                if (strcmp(parentPTR, "") == 0) {
                    name = fname;
                } else {
                    name = string(parentPTR) + "/" + fname;
                }
                struct stat st;
                int statRet = stat(name.c_str(), &st);
                if (statRet == -1) {
                    char* buffer = requestMemory(length + nameMax + 128);
                    sprintf(buffer,
                            "System Call \"stat\" failed with file %s: %s",
                            name.c_str(),
                            strerror(errno));
                    log_ERROR(buffer);
                    releaseMemory(buffer);
                    continue;
                }
                if (S_ISREG(st.st_mode)) {
                    //regular file.
                    IMGs.push_back(
                        std::make_tuple<>(stringDUP(name.c_str()), getNumber(name.c_str())));
                } else if (S_ISDIR(st.st_mode)) {
                    // directory
                    AddDirectory(name.c_str());
                } else {
                    char* buffer = requestMemory(length + nameMax + 128);
                    sprintf(buffer, "Unknown file type %s", name.c_str());
                    log_ERROR(buffer);
                    releaseMemory(buffer);
                }
            }
        } while (true);
        closedir(dir);
        if (IMGs.size() % 3 != 0) {
            char* buffer = requestMemory(length + 128);
            sprintf(buffer, "Image Number in directory %s cannot be divided by 3 well.", parentPTR);
            log_ERROR(buffer);
            releaseMemory(buffer);
        } else {
            std::sort(IMGs.begin(), IMGs.end(), [&](auto& a, auto& b) {
                return (std::get<NUMBER>(a) - std::get<NUMBER>(b)) <= 0;
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
        ocr->addItem(nullptr);
        ocr->join();

        sql.addItem(nullptr);
        sql.join();
#ifdef BUILD_WITH_LIBSSH
        sftp.addItem(nullptr);
        sftp.join();
#endif
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

    void Item::getCode(string& fc, string& bc, int& price)
    {
        ocr.getFullCode(fc);
        ocr.getBarCode(bc);
        ocr.getPrice(this->price);
        std::swap(fc, fcode);
        std::swap(bc, bcode);
        price = this->price;
    }

    void Item::getOcrJson(string& json)
    {
        json = string(ocr.dumpJson());
    }

    Item::Item(const char* _p1, const char* _p2, const char* _p3)
        : PIC_1(stringDUP(_p1)),
          PIC_2(stringDUP(_p2)),
          PIC_3(stringDUP(_p3)),
          front(PIC_1),
          back(PIC_2),
          board(PIC_3)
    {
        ocrfailed = 0;
    }

    Item::~Item()
    {
        releaseMemory(PIC_1);
        releaseMemory(PIC_2);
        releaseMemory(PIC_3);
    }

    void Item::setDestPath(string& p1, string& p2, string& p3)
    {
        std::swap(destPIC[0], p1);
        std::swap(destPIC[1], p2);
        std::swap(destPIC[2], p3);
    }

    void Item::SaveFile(const string& product, bool del)
    {
        string p1 = product + "/" + PIC_1;
        size_t bsize = 1024;
        char* buffer = tb::utils::requestMemory(bsize);
        auto ret = tb::utils::mkParentDir(p1.c_str());
        if (ret != 0) {
            snprintf(buffer,
                     bsize,
                     "Create Parent Directory of file \"%s\" failed: %s",
                     p1.c_str(),
                     strerror(ret));
            log_ERROR(buffer);
            return;
        }
        int saved = 0;
        string parent;
        tb::utils::getParentDir(p1, parent);
        getcwd(buffer, bsize);
        chdir(parent.c_str());

        saved += front.WriteToFile(p1.c_str());

        string p2 = product + "/" + PIC_2;
        saved += back.WriteToFile(p2.c_str());

        string p3 = product + "/" + PIC_3;
        saved += board.WriteToFile(p3.c_str());

        chdir(buffer);
        if (del) {
            unlink(PIC_1);
            unlink(PIC_2);
            unlink(PIC_3);
        }
        snprintf(buffer,
                 bsize,
                 "Saving %s -> %s, Status %d",
                 PIC_1,
                 (product + "/" + PIC_1).c_str(),
                 saved);
        setDestPath(p1, p2, p3);
        log_DEBUG(buffer);
    }

    int Item::processingAccurateOCR(int& curl)
    {
        static char buffer[1024];
        int ret = board.getItemAccurateCode(fcode, bcode, price, curl, ocr);
        snprintf(buffer,
                 1024,
                 "Get Item Accurate: %s ret -> %d : fc-> %s, bc -> %s, price -> %d",
                 PIC_3,
                 ret,
                 fcode.c_str(),
                 bcode.c_str(),
                 price);
        log_INFO(buffer);
        return ret;
    }

    int Item::processingOCR(int& curl)
    {
        static char buffer[1024];
        int ret = board.getItemCode(fcode, bcode, price, curl, ocr);
        snprintf(buffer,
                 1024,
                 "Get Item: %s ret-> %d, fc -> %s, bc -> %s, price -> %d",
                 PIC_3,
                 ret,
                 fcode.c_str(),
                 bcode.c_str(),
                 price);
        log_INFO(buffer);
        return ret;
    }

    int Item::processing()
    {
        front.AddWaterPrint();
        back.AddWaterPrint();
        board.AddWaterPrint();
        return 0;
    }

    // item
#ifdef BUILD_WITH_LIBSSH
    void* SFTP::start(void*, void*, void*)
    {
        static int prefix = globalConfig.productPrefixLength;
        static const auto end = std::string::npos;
        do {
            _cv.wait(_m, [this] { return _q.size() > 0; });

            auto p = _q.front();
            _q.pop();
            _m.unlock();

            if (p == nullptr) {
                break;
            }

            string pname[3];
            const char* rawName[3];
            p->getName(rawName[0], rawName[1], rawName[2]);
            p->getDestName(pname[0], pname[1], pname[2]);
            sftp.sendFile(pname[0].c_str(), pname[0].substr(prefix, end).c_str());
            sftp.sendFile(pname[1].c_str(), pname[1].substr(prefix, end).c_str());
            sftp.sendFile(pname[2].c_str(), pname[2].substr(prefix, end).c_str());
        } while (true);
        return nullptr;
    }
#endif

    bool MySQLTimer::processing(std::queue<std::shared_ptr<Item>>& _q)
    {
        bool ret = false;
        instance.beginTransation();
        const size_t bsize = 2048;
        static char buffer[bsize];
        while (_q.size() > 0) {
            string sql =
                "INSERT INTO `Clothes` (`BarCode`, `FullCode`, `FrontPath`, `BackPath`, "
                "`BoardPath`, `BoardPrice`, `OcrResult`, `DirectoryID`) VALUES ";

            int i = 0;
            do {
                i = 0;
                auto p = _q.front();
                _q.pop();
                if (p == nullptr) {
                    if (_q.size() == 0) {
                        ret = true;
                        break;
                    } else {
                        continue;
                    }
                }
                const char* pic[3];
                string barcode, fullcode;
                int price;
                string json;
                p->getName(pic[0], pic[1], pic[2]);
                p->getOcrJson(json);
                p->getCode(fullcode, barcode, price);
                string parent;
                tb::utils::getParentDir(pic[0], parent);
                uint64_t did = globalConfig.getDirectoryID(parent);
                snprintf(buffer,
                         bsize,
                         "('%s', '%s', '%s', '%s','%s', %d, '%s', %ld)",
                         barcode.c_str(),
                         fullcode.c_str(),
                         pic[0],
                         pic[1],
                         pic[2],
                         price,
                         json.c_str(),
                         did);
                sql = sql + buffer + ",";
                i++;
            } while (i < 10 && _q.size() > 0);
            if (i > 0) {
                auto last = sql.find_last_of(',');
                sql.at(last) = ';';
                snprintf(buffer, bsize, "Inserting %d into mysql", i);
                log_DEBUG(buffer);
                instance.query(sql.c_str());
            }
        }
        instance.commit();
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
        static char buffer[1024];
        do {
            _cv.wait(_m, [this] { return _q.size() > 0; });
            auto i = _q.front();
            _q.pop();
            _m.unlock();
            if (i == nullptr) {
                _m.lock();
                auto s = _q.size();
                _m.unlock();
                if (s == 0) {
                    return nullptr;
                } else {
                    addItem(nullptr);
                    continue;
                }
            }
            int f = i->getFailed();
            int curl;

            auto func = f == 0 ? std::bind(&Item::processingOCR, i, std::placeholders::_1)
                               : std::bind(&Item::processingAccurateOCR, i, std::placeholders::_1);

            int slTime = 0;
            do {
                int ostatus = 0;
                ostatus = func(curl);
                if (ostatus == -1) {
                    int t = std::pow(2, slTime);
                    int err;
                    auto o = i->getOcrResult().getError(err);
                    snprintf(buffer,
                             1024,
                             "Get Error \"%s\" (%d) from OCR Handler, Sleep for %d seconds",
                             o,
                             err,
                             t);
                    log_WARNING(buffer);
                    sleep(t);
                } else {
                    break;
                }
                slTime++;
            } while (true);

            int price;
            string fc, bc;
            i->getCode(fc, bc, price);
            if (fc != "" || price < 100) {
                i->SaveFile(globalConfig.productPath, globalConfig.deleteRaw);
                std::shared_ptr<Item> iptr;
                iptr.reset(i);
                mysql(iptr);
                sftp(iptr);
            } else if (f < 3) {
                i->ocrFailed();
                this->addItem(i);
            } else {
                size_t bsize = 512;
                char* buf = requestMemory(bsize);
                snprintf(buf,
                         bsize,
                         "Get Ocr Result of file %s failed 5 times. Skip this file.",
                         i->getBoardName());
                log_WARNING(buf);
                releaseMemory(buf);
                delete i;
            }
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
        handler.AddDirectory("./");
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

    fc::ImageProcessingDestroy();
    tb::remote::MySQLWorker::destroyMySQLInstance();
#ifdef BUILD_WITH_LIBSSH
    tb::remote::SFTPWorker::destrypSFTPInstance();
#endif
    fc::ItemSchedular::destroyItemSchedular();
    tb::Logger::DestoryLogger();
}
