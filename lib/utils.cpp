
#include "taobao.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <zlib.h>
#include <boost/pool/pool.hpp>
#include <cstring>

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
            return reinterpret_cast<char*>(__pool->ordered_malloc(_size));
        }

        void releaseMemory(const void* _ptr)
        {
            if (_ptr != nullptr) {
                __pool->ordered_free(const_cast<void*>(_ptr));
            }
        }

        int gzCompress(unsigned char* _in, size_t _in_size, unsigned char** _out)
        {
            size_t outBound = compressBound(_in_size);
            *_out = reinterpret_cast<unsigned char*>(requestMemory(outBound));
            int ret = compress(*_out, &outBound, _in, _in_size);
            if (ret == Z_OK) {
                return outBound;
            } else {
                return ret;
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

    }  // namespace utils
}  // namespace tb
