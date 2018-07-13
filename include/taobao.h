
/* shared
 *
 */

#ifndef TAOBAO_SHARED_H
#define TAOBAO_SHARED_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>  // for O_RDONLY

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
        class MySQLWorker;
    }

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

    };  // namespace utils
}  // namespace tb


#endif
