
#include "fchecker.h"
#include "image.h"
#include "logger.h"

#include <fcntl.h>
#include <json/json.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <string>

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
}  // namespace

void StartLog(const Json::Value &doc, tb::thread_ns::barrier *b)
{
    auto &log = Logger::getLogger(b);
    b->wait();
    if (doc.isMember("logger")) {
        auto &array = doc["logger"];
        if (array.isArray()) {
            for (unsigned int i = 0; i < array.size(); i++) {
                auto &obj = array[i];
                auto &fnObj = obj["filePath"];
                auto &severityObj = obj["severity"];
                auto &appendObj = obj["append"];
                const char *fn = nullptr;
                const char *severity = nullptr;
                bool append = false;
                if (!fnObj.isNull()) {
                    fn = fnObj.asCString();
                }
                if (!severityObj.isNull()) {
                    severity = severityObj.asCString();
                }
                if (!appendObj.isNull()) {
                    append = appendObj.asBool();
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
        if (parent.isMember(#name)) {       \
            auto &obj = parent[#name];      \
            if (obj.is##type()) {           \
                value = obj.as##type();     \
            }                               \
        }                                   \
    } while (false)

void StartSystem(const Json::Value &jsonRoot)
{
    string dir = "";
    string rawDir = "";
    string productDir = "";
    bool useInotify;
    int uid, gid;
    uid = gid = -1;
    auto &root = jsonRoot["system"];
    getValue(rootDirectory, root, String, dir);
    getValue(useInotify, root, Bool, useInotify);
    getValue(gid, root, Int, gid);
    getValue(uid, root, Int, uid);
    getValue(rawDirectory, root, String, rawDir);
    getValue(productDir, root, String, productDir);

    globalConfig.path = (dir);
    globalConfig.uid = uid;
    globalConfig.gid = gid;
    globalConfig.useInotify = useInotify;
    globalConfig.rawPath = (rawDir);
    globalConfig.productPath = (productDir);

    char *buffer = requestMemory((dir.length()) + 128);
#ifdef USE_INOTIFY
    if (useInotify) {
        globalConfig.inotifyFD = inotify_init();
    }
    inotify_add_watch(globalConfig.inotifyFD, dir.c_str(), IN_CREATE | IN_MODIFY | IN_ONLYDIR);
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
    using namespace Json;  //jsoncpp

    atexit(fcExit);
    char *error;
    size_t size;
    char *buffer = reinterpret_cast<char *>(tb::utils::openFile(json, size, &error));
    if (buffer == nullptr) {
        log_FATAL(error);
        releaseMemory(error);
        exit(-1);
    }
    Reader *parser = new Reader(Features::strictMode());
    Value root;
    if (!parser->parse(buffer, root)) {
        //Failed to parse json file.
        exit(-3);
    }
    tb::barrier b(2);
    StartLog(root, &b);
    StartSystem(root);
    fc::ImageProcessingStartup(root);
    tb::utils::destroyFile(buffer, size, &error);
}

void fcExit()
{
    Logger::DestoryLogger();
    fc::ImageProcessingDestroy();
}


#ifndef PROJECT_VERSION
#define PROJECT_VERSION "unknown"
#endif
#include <iostream>
using namespace std;

void version(const char *name)
{
    cout << name << " Version: " << PROJECT_VERSION << endl << "----------" << endl;
    cout << "Build with "
#ifdef USE_POSIX_THREAD
         << "POSIX Thread Model." << endl;
#else
         << "C++ 11 Thread Model" << endl;
#endif
    cout << "Build with Boost Version " << TB_BOOST_VERSION << endl;
    cout << "Build with ZLIB Version " << TB_ZLIB_VERSION << endl;
    exit(0);
}
