
#include "taobao.h"

#include <cstdio>
#include <cstring>

#ifdef UNIX_USE_MMAP
#include <sys/mman.h>
#endif

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <zlib.h>
#include <boost/pool/pool.hpp>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace tb
{
    namespace
    {
        boost::pool<>* __pool = nullptr;
    }
    namespace utils
    {
        void InitCoreUtilties()
        {
            if (__pool == nullptr) {
                auto ___pool = reinterpret_cast<boost::pool<>*>(malloc(sizeof *__pool));
                __pool = new (___pool) boost::pool<>(sizeof(char), 1 << 12);
            }
        }

        void DestroyCoreUtilties()
        {
            if (__pool != nullptr) {
                delete __pool;
                __pool = nullptr;
            }
        }

        char* requestMemory(unsigned long _size)
        {
            //return reinterpret_cast<char*>(__pool->ordered_malloc(_size));
            return reinterpret_cast<char*>(malloc(_size));
        }

        void releaseMemory(const void* _ptr)
        {
        //     if (_ptr != nullptr) {
        //         __pool->ordered_free(const_cast<void*>(_ptr));
        //     }
            free((void*)_ptr);
        }

        int gzCompress(unsigned char* _in, size_t _in_size, unsigned char** _out)
        {
            size_t outBound = compressBound(_in_size);
            *_out = reinterpret_cast<unsigned char*>(requestMemory(outBound));
            int ret = compress(*_out, &outBound, _in, _in_size);
            if (ret == Z_OK) {
                return outBound;
            } else {
                return -1;
            }
        }

        int base64Encode(unsigned char* _in, size_t _in_size, char** _out, bool _newline)
        {
            BIO* bmem = NULL;
            BIO* b64 = NULL;
            BUF_MEM* bptr = NULL;
            size_t length = 0;
            b64 = BIO_new(BIO_f_base64());
            if (!_newline) {
                BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            }
            bmem = BIO_new(BIO_s_mem());
            b64 = BIO_push(b64, bmem);
            BIO_write(b64, _in, _in_size);
            BIO_flush(b64);
            BIO_get_mem_ptr(b64, &bptr);
            length = bptr->length;
            *_out = requestMemory(bptr->length + 1);
            std::memcpy(*_out, bptr->data, bptr->length);
            *_out[length] = 0;
            BIO_free_all(b64);

            return length;
        }

        size_t checkFileCanRead(const char* _path, char* buffer, size_t size)
        {
            struct stat st;
            int status = stat(_path, &st);
            if (status == -1) {
                snprintf(buffer, size, "Get status of file %s faild: %s", _path, strerror(errno));
                return -1u;
            }
            if (!S_ISREG(st.st_mode)) {
                snprintf(
                    buffer, size, "File: %s does not appear to be a regular file. Skip.", _path);
                return -1u;
            }
            status = access(_path, R_OK);
            if (status == -1) {
                snprintf(buffer, size, "File: %s cannot read: %s", _path, strerror(errno));
                return -1u;
            }
            return st.st_size;
        }

        void* openFile(const char* _path, size_t& size, char** buffer, unsigned int mask)
        {
            auto bufferLength = strlen(_path) + 64;
            *buffer = requestMemory(bufferLength);

            size = checkFileCanRead(_path, *buffer, bufferLength);
            if (size == -1u) {
                return nullptr;
            }
            int fd = open(_path, mask);
            if (fd == -1) {
                sprintf(*buffer, "Open file: %s failed: %s", _path, strerror(errno));
                return nullptr;
            }
            void* ret = nullptr;
#ifdef UNIX_USE_MMAP
            unsigned int mmapMask = 0;
            if ((mask & O_WRONLY) == O_WRONLY) {
                mmapMask |= PROT_WRITE;
            }
            if ((mask & O_RDONLY) == O_RDONLY) {
                mmapMask |= PROT_READ;
            }

            ret = mmap(nullptr, size, mmapMask, MAP_SHARED, fd, 0);
            if (ret == MAP_FAILED) {
                sprintf(*buffer, "mmap of file %s failed: %s", _path, strerror(errno));
                return nullptr;
            }
#else
            ret = malloc(size);
            if (ret == nullptr) {
                sprintf(*buffer, "read file %s failed: allocate memory of %d failed.", _path, size);
            }
            read(fd, ret, size);
            if (read != size) {
                sprintf(*buffer, "read file %s failed: %s", _path, strerror(errno));
                free(ret);
                return nullptr;
            }
#endif
            close(fd);

            releaseMemory(*buffer);
            return ret;
        }

        int destroyFile(void* p, size_t size, char** buffer)
        {
#ifdef UNIX_USE_MMAP
            int ret = munmap(p, size);
            if (ret == -1) {
                *buffer = requestMemory(128);
                sprintf(*buffer, "munmap 0x%p of size %lu failed: %s", p, size, strerror(errno));
                return -1;
            }
#else
            releaseMemory(p);
#endif
            return 0;
        }

    }  // namespace utils
}  // namespace tb
