

#ifndef DB_H_
#define DB_H_

#include "taobao.h"
#include "threads.h"

#include <mysql.h>
#include <string>

namespace tb
{
    namespace db
    {
        using std::string;
        using tb::thread_ns::thread;

        class MySQLWorker : public thread
        {
            using csr = const string&;
            enum { CONNECTION_NOT_REAL_CONNECT = 0, CONNECTION_FAILED = 1, CONNECTION_SUCCESS = 2 };
            static MySQLWorker* instance;

            unsigned int status;
            unsigned int port;
            int errNo;

            MYSQL* _remote;
            const char* errString;

            string addr;
            string user;
            string pass;
            string db;
            bool compress;

            MySQLWorker(const MySQLWorker&) = delete;
            MySQLWorker(csr, csr, csr, csr, unsigned int, bool);

            void doConnect();

            void close();
            virtual ~MySQLWorker();

        public:
            virtual void* start(void*, void*, void*) override;

            const char* getRemoteServerInfo(unsigned int&);
            bool tryConnect(const char**);
            const char* getErrorString() const;
            MySQLWorker* getMySQLInstance();
            static MySQLWorker& initMySQLInstance(csr, csr, csr, csr, unsigned int, bool);
            static void destroyMySQLInstance();
        };
    }  // namespace db
}  // namespace tb
#endif
