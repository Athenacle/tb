
#include "taobao.h"
#include "threads.h"

#include <cstdio>
#include <cstring>

#ifdef UNIX_USE_MMAP
#include <sys/mman.h>
#endif

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
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
        tb::thread_ns::mutex _m;
        const int blockSize = 256;
    }  // namespace
    namespace utils
    {
        void InitCoreUtilties() {}

        void DestroyCoreUtilties() {}

        char* requestMemory(unsigned long _size)
        {
            return (char*)malloc(_size);
        }

        void releaseMemory(const void* _ptr)
        {
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

        void MD5Hash(const char* src, size_t size, std::string& out)
        {
            MD5_CTX ctx;

            unsigned char md[16] = {0};
            char tmp[8] = {0};

            MD5_Init(&ctx);
            MD5_Update(&ctx, src, size);
            MD5_Final(md, &ctx);

            for (int i = 0; i < 16; ++i) {
                memset(tmp, 0, sizeof(tmp));
                sprintf(tmp, "%02x", md[i]);
                out += tmp;
            }
        }

        int MD5HashFile(const char* fname, std::string& out)
        {
            out = "";
            char* err;
            size_t size;
            void* ptr = openFile(fname, size, &err);
            if (ptr == nullptr) {
                return -1;
            }
            MD5Hash(reinterpret_cast<char*>(ptr), size, out);
            destroyFile(ptr, size, &err);
            return 0;
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
            (*_out)[length] = 0;
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

        namespace
        {
            int mk(const std::string& dir)
            {
                auto sep = dir.find_last_of('/');
                int ret = 1;
                struct stat st;
                if (stat(dir.c_str(), &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        return 0;
                    }
                }

                if (sep != std::string::npos) {
                    ret = mk(dir.substr(0, sep));
                    if (ret == 0) {
                        ret = mkdir(dir.c_str(), 0755);
                        if (ret == EEXIST) {
                            return 0;
                        } else {
                            return ret;
                        }
                    } else {
                        return ret;
                    }
                } else {
                    if (mkdir(dir.c_str(), 0755) == -1) {
                        if (errno == EEXIST) {
                            return 0;
                        } else {
                            return errno;
                        }
                    }
                }
                return 0;
            }
        }  // namespace

        int mkParentDir(const std::string& p)
        {
            std::string par;
            getParentDir(p, par);
            return mk(par);
        }

        bool getParentDir(const std::string& p, std::string& parent)
        {
            auto s = p.find_last_of('/');
            if (s == 0 || s == std::string::npos) {
                parent = p;
                return false;
            } else {
                parent = p.substr(0, s);
                return true;
            }
        }

        void formatDirectoryPath(std::string& p)
        {
            const char* ptr = p.c_str();
            std::string dest;
            dest.reserve(p.length());
            bool match = false;

            for (; *ptr; ptr++) {
                if (*ptr == '\t' || *ptr == ' ') {
                    if (match) {
                        dest.push_back(*ptr);
                    } else {
                        continue;
                    }
                } else {
                    if (*ptr == '.' && *(ptr + 1) == '/') {
                        ptr++;
                        if (match) {
                            dest.append("./");
                        } else {
                            continue;
                        }
                    } else if (*ptr == '/' && *(ptr + 1) == '/') {
                        continue;
                    }
                    match = true;
                    dest.push_back(*ptr);
                }
            }
            std::swap(p, dest);
        }

    }  // namespace utils
}  // namespace tb
