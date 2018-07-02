
#include "logger.h"
#include "taobao.h"

#include <sys/prctl.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <iostream>

using namespace tb;
using namespace std;
using f = void (*)(const char*);

extern f func[];
extern pthread_barrier_t b;
extern size_t count;
const char* rstr();
extern pthread_mutex_t m;
[[noreturn]] void* doit(void*);

f func[] = {log_TRACE, log_DEBUG, log_INFO, log_WARNING, log_ERROR, log_FATAL};

pthread_barrier_t b;
pthread_mutex_t m;

size_t count = sizeof func / sizeof func[0];

const char* rstr()
{
    const char str[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t length = 5 + rand() % 10;
    char* ret = new char[length + 1];
    for (size_t i = 0; i < length; i++) {
        ret[i] = str[static_cast<unsigned long>(rand()) % (sizeof str / sizeof str[0])];
    }
    ret[length] = 0;
    return ret;
}

[[noreturn]] void* doit(void*) {
    const char* ptr;
    prctl(PR_SET_NAME, "doit");
    int times = 1 + rand() % 10;
    pthread_mutex_lock(&m);
    cout << "TID: " << pthread_self() << " TIMES: " << times << endl << flush;
    pthread_mutex_unlock(&m);
    for (int i = times; i > 0; i--) {
        auto fun = func[static_cast<unsigned long>(rand()) % ::count];
        ptr = rstr();
        fun(ptr);
        delete[] ptr;
        usleep(rand() % 10000 + 10000);
    }
    pthread_exit(nullptr);
}

const int tc = 5;

int main(void)
{
    pthread_mutex_init(&m, nullptr);
    Logger::getLogger().AddConsoleBackend(TRACE).AddFileBackend("/tmp/main.log", DEBUG);
    srand(static_cast<unsigned int>(time(nullptr)));
    pthread_t arr[tc];
    int threads = tc;
    cout << "THREADS: " << threads << endl << flush;
    for (int i = 0; i < threads; i++) {
        pthread_t tid;
        pthread_create(&tid, nullptr, doit, &m);
        arr[i] = tid;
    }
    for (auto tid : arr) {
        pthread_join(tid, nullptr);
    }
    pthread_mutex_destroy(&m);
    Logger::DestoryLogger();
}
