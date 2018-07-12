
#ifndef THREADS_H
#define THREADS_H

#include "taobao.h"

#ifdef UNIX_HAVE_SYS_PRCTL
#include <sys/prctl.h>
#endif
#include <iostream>
#ifdef USE_CXX_THREAD
#include <boost/thread/barrier.hpp>
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
    namespace thread_ns
    {
        class thread_arguments
        {
        public:
            void* arg1;
            void* arg2;
            void* arg3;
            thread* const thisPointer;
            thread_arguments(thread* const tp,
                             void* a1 = nullptr,
                             void* a2 = nullptr,
                             void* a3 = nullptr)
                : arg1(a1), arg2(a2), arg3(a3), thisPointer(tp)
            {
            }
        };

        inline void SetThreadName(const char* n)
        {
#if defined UNIX_HAVE_PRCTL && defined UNIX_HAVE_SYS_PRCTL
            prctl(PR_SET_NAME, n);
#endif
        }
#ifdef USE_CXX_THREAD
        using boost::barrier;
        using boost::mutex;
        using tid = boost::thread::id;

        class thread
        {
            boost::thread thisThread;
            const char* threadName;

            tid getTID()
            {
                return boost::this_thread::get_id();
            }

            static void* thread_exec(void* para)
            {
                auto a = reinterpret_cast<thread_arguments*>(para);

                if (a != nullptr) {
                    SetThreadName(a->thisPointer->threadName);
                }
                return a->thisPointer->start(a->arg1, a->arg2, a->arg3);
            }

        public:
            virtual void* start(void*, void*, void*) = 0;

            void begin(void* p1 = nullptr, void* p2 = nullptr, void* p3 = nullptr)
            {
                auto arg = new thread_arguments(this, p1, p2, p3);
                boost::thread(thread_exec, arg);
            }

            thread(const char* n = nullptr)
            {
                threadName = n;
            }

            virtual ~thread() {}

            void join(void** = nullptr)
            {
                return thisThread.join();
            }
        };

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
            const char* threadName;

            static void* thread_exec(void* para)
            {
                auto a = reinterpret_cast<thread_arguments*>(para);
                auto p1 = a->arg1;
                auto p2 = a->arg2;
                auto p3 = a->arg3;
                auto thsP = a->thisPointer;
                if (thsP->threadName != nullptr) {
                    SetThreadName(thsP->threadName);
                }
                delete a;
                return thsP->start(p1,p2,p3);
            }

            thread(thread&) = delete;

        protected:
            virtual void* start(void*, void*, void*) = 0;

            virtual ~thread() {}
        public:
            void begin(void* p1 = nullptr, void* p2 = nullptr, void* p3 = nullptr)
            {
                auto arg = new thread_arguments(this, p1, p2, p3);
                pthread_create(&TID, nullptr, thread_exec, arg);
            }

            thread(const char* n = nullptr)
            {
                TID = 0;
                threadName = n;
            }

            void join(void** retVal = nullptr)
            {
                pthread_join(TID, retVal);
            }

            tid getTid() const
            {
                return TID;
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
}  // namespace tb

#endif
