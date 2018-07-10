
#include <boost/pool/pool.hpp>
#include "taobao.h"

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
    }  // namespace utils
}  // namespace tb
