
/* shared
 *
 */

#ifndef TAOBAO_SHARED_H
#define TAOBAO_SHARED_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>  // for O_RDONLY
#include <string>

namespace tb
{
    class Settings;
    class Logger;

    namespace thread_ns
    {
        class thread;
        class thread_arguments;

    }  // namespace thread_ns

    namespace remote
    {
        enum {
            CONNECTION_NOT_REAL_CONNECT = 0,
            CONNECTION_FAILED = 1,
            CONNECTION_SUCCESS = 2,
            CONNECTION_SUCCESS_DB_CHANGED = 4
        };

#ifdef BUILD_WITH_LIBSSH
        class SFTPWorker;
#endif
        class MySQLWorker;
    }  // namespace remote

    namespace utils
    {
        // lib/utils.cpp
        void InitCoreUtilties();
        void DestroyCoreUtilites();

        char* requestMemory(unsigned long);
        void releaseMemory(const void*);

        int gzCompress(unsigned char*, size_t, unsigned char**);

        int base64Encode(unsigned char*, size_t, char**, bool = false);

        size_t checkFileCanRead(const char*, char*, size_t);

        void* openFile(const char*, size_t&, char**, unsigned int = O_RDONLY);

        int destroyFile(void*, size_t, char**);

        int mkParentDir(const std::string&);

        bool getParentDir(const std::string&, std::string&);

        void formatDirectoryPath(std::string&);
    };  // namespace utils
}  // namespace tb


#endif
