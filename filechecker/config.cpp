
#include "fchecker.h"
#include "logger.h"
#include "image.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef USE_INOTIFY
#include <sys/inotify.h>
#endif

using tb::Logger;
using tb::LogSeverity;


namespace
{
    LogSeverity intToSeverity(int i)
    {
        switch (i) {
            case 0:
                return tb::TRACE;
            case 1:
                return tb::DEBUG;
            case 2:
                return tb::INFO;
            case 3:
                return tb::WARNING;
            case 4:
                return tb::ERROR;
            case 5:
                return tb::FATAL;
            default:
                return tb::NONE;
        }
    }

    LogSeverity getSeverity(const char *s)
    {
        auto len = strlen(s);
        LogSeverity ret = tb::NONE;
        if (len > 7) {
            return LogSeverity::INFO;
        } else {
            char *severity = requestMemory(len + 1);
            for (unsigned long i = 0; i < len; i++) {
                severity[i] = toupper(s[i]);
            }
            severity[len] = 0;
            const char level[][10] = {
                "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"};
            for (unsigned long i = 0; i < (sizeof level / sizeof level[0]); i++) {
                if (strcmp(severity, level[i]) == 0) {
                    ret = intToSeverity(i);
                    break;
                }
            }
            releaseMemory(severity);
        }
        return ret;
    }
    int fileSize = 0;

    char *OpenFile(const char *path)
    {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            char *buffer = requestMemory(strlen(path) + 128);
            sprintf(buffer, "Cannot open file %s for reading: %s. \n", path, strerror(errno));
            write(STDERR_FILENO, buffer, strlen(buffer));
            releaseMemory(buffer);
            exit(-1);
        } else {
            struct stat fs;
            fstat(fd, &fs);
            auto ret = reinterpret_cast<char *>(
                mmap(nullptr, fileSize = fs.st_size + 1, PROT_READ, MAP_SHARED, fd, 0));

            close(fd);
            return ret;
        }
    }

    void CloseFile(char *buffer)
    {
        munmap(buffer, fileSize);
    }

}  // namespace

void StartLog(const Document &doc, tb::thread_ns::barrier *b)
{
    auto &log = Logger::getLogger(b);
    b->wait();
    if (doc.HasMember("logger")) {
        auto &array = doc["logger"];
        if (array.IsArray()) {
            for (SizeType i = 0; i < array.Size(); i++) {
                auto &obj = array[i];
                auto &fnObj = obj["filePath"];
                auto &severityObj = obj["severity"];
                auto &appendObj = obj["append"];
                const char *fn = nullptr;
                const char *severity = nullptr;
                bool append = false;
                if (!fnObj.IsNull()) {
                    fn = fnObj.GetString();
                }
                if (!severityObj.IsNull()) {
                    severity = severityObj.GetString();
                }
                if (!appendObj.IsNull()) {
                    append = appendObj.GetBool();
                }
                if (fn == nullptr) {
                    log.AddConsoleBackend(getSeverity(severity));
                } else {
                    log.AddFileBackend(fn, getSeverity(severity), append);
                }
            }
        }
    }
}

#define getValue(name, parent, type, value) \
    do {                                    \
        if (parent.HasMember(#name)) {      \
            auto &obj = parent[#name];      \
            if (obj.Is##type()) {           \
                value = obj.Get##type();    \
            }                               \
        }                                   \
    } while (false)

void StartSystem(const Document &doc)
{
    const char *dir = nullptr;
    const char *rawDir = nullptr;
    const char *productDir = nullptr;
    bool useInotify;
    int uid, gid;
    uid = gid = -1;
    auto &root = doc["system"];
    getValue(rootDirectory, root, String, dir);
    getValue(useInotify, root, Bool, useInotify);
    getValue(gid, root, Int, gid);
    getValue(uid, root, Int, uid);
    getValue(rawDirectory, root, String, rawDir);
    getValue(productDir, root, String, productDir);

    globalConfig.path = stringDUP(dir);
    globalConfig.uid = uid;
    globalConfig.gid = gid;
    globalConfig.useInotify = useInotify;
    globalConfig.rawPath = stringDUP(rawDir);
    globalConfig.productPath = stringDUP(productDir);

    char *buffer = requestMemory(strlen(dir) + 128);
#ifdef USE_INOTIFY
    if (useInotify) {
        globalConfig.inotifyFD = inotify_init();
    }
    inotify_add_watch(globalConfig.inotifyFD, dir, IN_CREATE | IN_MODIFY | IN_ONLYDIR);
#endif

    if (uid >= 0) {
        if (setuid(uid) == -1) {
            sprintf(buffer, "Set UID of %d Failed: %s", uid, strerror(errno));
            log_ERROR(buffer);
        } else {
            sprintf(buffer, "Set UID of %d Success,,", uid);
            log_TRACE(buffer);
        }
    }
    if (gid >= 0) {
        if (setgid(gid) == -1) {
            sprintf(buffer, "Set GID of %d Failed: %s", gid, strerror(errno));
            log_ERROR(buffer);
        } else {
            sprintf(buffer, "Set GID of %d Success,,", gid);
            log_TRACE(buffer);
        }
    }
    releaseMemory(buffer);
}

#undef getValue

void fcInit(const char *json)
{
    atexit(fcExit);
    auto buffer = OpenFile(json);
    Document doc;
    doc.Parse(buffer);
    tb::barrier b(2);
    StartLog(doc, &b);
    StartSystem(doc);
    CloseFile(buffer);
    fc::ImageProcessingStartup();
}

void fcExit()
{
    Logger::DestoryLogger();
    fc::ImageProcessingDestroy();
    releaseMemory(globalConfig.path);
    releaseMemory(globalConfig.rawPath);
    releaseMemory(globalConfig.productPath);
}
