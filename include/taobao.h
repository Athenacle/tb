
/* shared
 *
 */

#ifndef TAOBAO_SHARED_H
#define TAOBAO_SHARED_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef UNIX_HAVE_SYS_PRCTL
#include <sys/prctl.h>
#endif

#ifdef USE_CXX_THREAD
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#elif defined USE_POSIX_THREAD
#include <pthread.h>
#include <functional>
#else
#error "Please choose a thread library."
#endif

namespace tb
{
    class Settings;
    class Logger;

    namespace thread_ns
    {
        inline void SetThreadName(const char* n)
        {
#if defined UNIX_HAVE_PRCTL && defined UNIX_HAVE_SYS_PRCTL
            prctl(PR_SET_NAME, n);
#endif
        }

#ifdef USE_CXX_THREAD
        using boost::mutex;
        using boost::thread;

        using tid = boost::thread::id;

        inline tid getTID()
        {
            return boost::this_thread::get_id();
        }

        class condition_variable
        {
        private:
            boost::condition_variable _cv;

        public:
            condition_variable() : _cv() {}

            ~condition_variable() {}

            int notify_all()
            {
                _cv.notify_all();
                return 0;
            }

            int notify_one()
            {
                _cv.notify_one();
                return 0;
            }

            template <class Predicate>
            void wait(mutex& lock, Predicate pred)
            {
                boost::unique_lock<boost::mutex> __lock(lock);
                _cv.wait(__lock, pred);
            }
        };


#elif defined USE_POSIX_THREAD

        using tid = pthread_t;

        class mutex;
        class condition_varaible;


        inline tid getTID()
        {
            return pthread_self();
        }

        class mutex
        {
            friend class condition_variable;

        private:
            pthread_mutex_t _m;

        public:
            mutex()
            {
                pthread_mutex_init(&_m, nullptr);
            }
            ~mutex()
            {
                pthread_mutex_destroy(&_m);
            }
            int lock()
            {
                return pthread_mutex_lock(&_m);
            }
            int unlock()
            {
                return pthread_mutex_unlock(&_m);
            }
        };

        using cond_test_fn = std::function<bool(void)>;

        class condition_variable
        {
        private:
            pthread_cond_t _t;

        public:
            condition_variable()
            {
                pthread_cond_init(&_t, nullptr);
            }
            ~condition_variable()
            {
                pthread_cond_destroy(&_t);
            }
            int signal()
            {
                return pthread_cond_signal(&_t);
            }
            int notify_all()
            {
                return pthread_cond_broadcast(&_t);
            }
            int wait(mutex& m, cond_test_fn fn)
            {
                while (true) {
                    m.lock();
                    while (!fn()) {
                        pthread_cond_wait(&_t, &m._m);
                    }
                    return 0;
                }
            }
        };

        using th_fn = void* (*)(void*);

        class thread
        {
        private:
            tid TID;

        public:
            thread()
            {
                TID = 0;
            }
            thread(thread& thr)
            {
                TID = thr.TID;
            }
            explicit thread(th_fn fn, void* args = nullptr)
            {
                pthread_create(&TID, nullptr, fn, args);
            }

            void join(void** retVal = nullptr)
            {
                pthread_join(TID, retVal);
            }
        };

        class barrier
        {
            pthread_barrier_t _b;

        public:
            barrier(unsigned int i)
            {
                pthread_barrier_init(&_b, nullptr, i);
            }
            void wait()
            {
                pthread_barrier_wait(&_b);
            }
            ~barrier()
            {
                pthread_barrier_destroy(&_b);
            }
        };
#endif
    }  // namespace thread_ns

    namespace utils {
        // lib/utils.cpp
        void InitCoreUtilties();
        void DestroyCoreUtilites();

        char* requestMemory(unsigned long);
        void releaseMemory(const void*);
    };
}  // namespace tb


#endif
