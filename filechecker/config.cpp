
#include "fchecker.h"
#include "image.h"
#include "logger.h"
#include "remote.h"

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
    void PrintConfigInfo(char *buffer, size_t bsize)
    {
        snprintf(buffer, bsize, "System Configuration:");
        log_INFO(buffer);
        snprintf(buffer, bsize, "\trootDirectory: %s", globalConfig.path.c_str());
        log_INFO(buffer);
        snprintf(buffer, bsize, "\trawDirectory: %s", globalConfig.rawPath.c_str());
        log_INFO(buffer);
        snprintf(buffer, bsize, "\tproductDirectory: %s", globalConfig.productPath.c_str());
        log_INFO(buffer);
        snprintf(buffer, bsize, "\tUse Inotify: %s", globalConfig.useInotify ? "True" : "False");
        log_INFO(buffer);
        snprintf(buffer, bsize, "\tUID: %d, GID: %d", globalConfig.uid, globalConfig.gid);
        log_INFO(buffer);
        snprintf(buffer,
                 bsize,
                 "\tWill forkToBackground: %s, workerThreadCount: %d",
                 globalConfig.forkToBackground ? "True" : "False",
                 globalConfig.threadCount);
        log_INFO(buffer);
    }

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

void StartSFTP()
{
#ifdef BUILD_WITH_LIBSSH
    const int bsize = 1024;
    char *buffer = tb::utils::requestMemory(bsize);

    if (globalConfig.sftpPort < 0 || globalConfig.sftpPort >= 65535) {
        snprintf(buffer,
                 bsize,
                 "Invalid SFTP Port Value %d, assume as defualt 22",
                 globalConfig.sftpPort);
        globalConfig.sftpPort = 22;
        log_WARNING(buffer);
    }

    snprintf(buffer, bsize, "SFTP Settings");
    log_INFO(buffer);
    snprintf(buffer,
             bsize,
             "\tAddress: %s, Port: %d",
             globalConfig.sftpAddress.c_str(),
             globalConfig.sftpPort);
    log_INFO(buffer);
    snprintf(buffer,
             bsize,
             "\tUsername: %s, Password: %s",
             globalConfig.sftpUsername.c_str(),
             globalConfig.sftpPassword == "" ? "--empty--" : "*****");
    log_INFO(buffer);
    const char *err;
    auto &ssh = tb::remote::SFTPWorker::initSFTPInstance(globalConfig.sftpAddress,
                                                         globalConfig.sftpUsername,
                                                         globalConfig.sftpPassword,
                                                         globalConfig.sftpIndentifyPath,
                                                         globalConfig.sftpPassphrase,
                                                         globalConfig.sftpRemotePath,
                                                         globalConfig.sftpPort,
                                                         globalConfig.sftpEnable);
    auto s = ssh.tryConnect();
    if (s != nullptr) {
        snprintf(buffer, 1024, "\tSFTP Channel Failed: %s", s);
        log_ERROR(buffer);
    } else {
        snprintf(buffer, bsize, "\tSFTP Channel esatblished successfully.", err);
        log_INFO(buffer);
    }
    tb::utils::releaseMemory(buffer);
#endif
}

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

void StartMYSQL()
{
    const int bsize = 1024;
    char *buffer = tb::utils::requestMemory(bsize);

    if (globalConfig.mysqlPort < 0 || globalConfig.mysqlPort >= 65535) {
        snprintf(buffer,
                 bsize,
                 "Invalid MySQL Port Value %d, assume as defualt 3306",
                 globalConfig.mysqlPort);
        globalConfig.mysqlPort = 3306;
        log_WARNING(buffer);
    }

    snprintf(buffer, bsize, "MySQL Settings");
    log_INFO(buffer);
    snprintf(buffer,
             bsize,
             "\tAddress: %s, Port: %d",
             globalConfig.mysqlAddress.c_str(),
             globalConfig.mysqlPort);
    log_INFO(buffer);
    snprintf(buffer,
             bsize,
             "\tUsername: %s, Password: %s, Compress: %s",
             globalConfig.mysqlUserName.c_str(),
             globalConfig.mysqlPassword == "" ? "--empty--" : "*****",
             globalConfig.mysqlCompress ? "True" : "False");
    log_INFO(buffer);
    snprintf(buffer, bsize, "\tLocal MySQL Client: %s", mysql_get_client_info());
    log_INFO(buffer);

    const char *err;
    auto &sql = tb::remote::MySQLWorker::initMySQLInstance(globalConfig.mysqlAddress,
                                                           globalConfig.mysqlUserName,
                                                           globalConfig.mysqlPassword,
                                                           globalConfig.mysqlDB,
                                                           globalConfig.mysqlPort,
                                                           globalConfig.mysqlCompress);
    auto s = sql.tryConnect(&err);
    if (s) {
        snprintf(buffer,
                 1024,
                 "\tMySQL Connection established successfully. Remote Server: %s",
                 sql.getRemoteServerInfo());
        log_INFO(buffer);
    } else {
        snprintf(buffer, bsize, "\tConnect to remote failed: %s", err);
        log_WARNING(buffer);
    }
    tb::utils::releaseMemory(buffer);
}

void StartRemote(const Json::Value &v)
{
    if (v.isMember("remote") && v["remote"].isObject()) {
        auto &remote = v["remote"];
        auto &mysql = remote["mysql"];
        if (!mysql.isNull()) {
            getValue(address, mysql, String, globalConfig.mysqlAddress);
            getValue(port, mysql, Int, globalConfig.mysqlPort);
            getValue(username, mysql, String, globalConfig.mysqlUserName);
            getValue(password, mysql, String, globalConfig.mysqlPassword);
            getValue(enable, mysql, Bool, globalConfig.mysqlEnable);
            getValue(compress, mysql, Bool, globalConfig.mysqlCompress);
            getValue(db, mysql, String, globalConfig.mysqlDB);
            StartMYSQL();
        }
#ifdef BUILD_WITH_LIBSSH
        auto &sftp = remote["sftp"];
        if (!mysql.isNull()) {
            getValue(enable, sftp, Bool, globalConfig.sftpEnable);
            getValue(address, sftp, String, globalConfig.sftpAddress);
            getValue(port, sftp, Int, globalConfig.sftpPort);
            getValue(username, sftp, String, globalConfig.sftpUsername);
            getValue(password, sftp, String, globalConfig.sftpPassword);
            getValue(identifyFile, sftp, String, globalConfig.sftpIndentifyPath);
            getValue(remotePath, sftp, String, globalConfig.sftpRemotePath);
            getValue(passphrase, sftp, String, globalConfig.sftpPassphrase);
            StartSFTP();
        }
#endif
    }
}

void StartSystem(const Json::Value &jsonRoot)
{
    string dir = "";
    string rawDir = "";
    string productDir = "";
    bool useInotify;
    int uid, gid;
    int workCount;
    uid = gid = -1;
    auto &root = jsonRoot["system"];
    getValue(rootDirectory, root, String, dir);
    getValue(useInotify, root, Bool, useInotify);
    getValue(gid, root, Int, gid);
    getValue(uid, root, Int, uid);
    getValue(rawDirectory, root, String, rawDir);
    getValue(productDir, root, String, productDir);
    getValue(workerThreadCount, root, Int, workCount);

    globalConfig.threadCount = workCount;
    globalConfig.path = (dir);
    globalConfig.uid = uid;
    globalConfig.gid = gid;
    globalConfig.useInotify = useInotify;
    globalConfig.rawPath = (rawDir);
    globalConfig.productPath = (productDir);

    size_t bsize = dir.length() + 4096;
    char *buffer = requestMemory(bsize);
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
            sprintf(buffer, "Set UID of %d Success", uid);
            log_TRACE(buffer);
        }
    }
    if (gid >= 0) {
        if (setgid(gid) == -1) {
            sprintf(buffer, "Set GID of %d Failed: %s", gid, strerror(errno));
            log_ERROR(buffer);
        } else {
            sprintf(buffer, "Set GID of %d Success", gid);
            log_TRACE(buffer);
        }
    }
    PrintConfigInfo(buffer, bsize);
    releaseMemory(buffer);
}

#undef getValue

void fcInit(const char *json)
{
    using namespace Json;  //jsoncpp

    tb::utils::InitCoreUtilties();
    char *error;
    size_t size;
    char *buffer = reinterpret_cast<char *>(tb::utils::openFile(json, size, &error));
    if (buffer == nullptr) {
        log_FATAL(error);
        releaseMemory(error);
        exit(-1);
    }

    Json::CharReaderBuilder builder;
    auto r = builder.newCharReader();
    Json::CharReader *reader(r);
    Json::Value root;
    JSONCPP_STRING errs;
    bool ok = reader->parse(buffer, buffer + size, &root, &errs);
    if (!ok) {
        //Failed to parse json file.
        char *buffer = tb::utils::requestMemory(1024);
        snprintf(buffer, 1024, "Open JSON File \"%s\" failed: %s", json, errs.c_str());
        log_FATAL(buffer);
        exit(-3);
    }
    SystemConfig::buildDefaultSystemConfig();
    tb::barrier b(2);
    StartLog(root, &b);
    StartRemote(root);
    StartSystem(root);
    fc::ImageProcessingStartup(root);
    tb::utils::destroyFile(buffer, size, &error);
    delete r;
}

void SystemConfig::buildDefaultSystemConfig()
{
    auto g = ::globalConfig;
    g.path = g.rawPath = g.productPath = g.mysqlAddress = g.mysqlUserName = g.mysqlPassword =
        g.mysqlDB = std::string(20, ' ');
    g.mysqlEnable = g.useInotify = g.mysqlCompress = true;
    g.uid = g.gid = -1;
    g.threadCount = 1;
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
