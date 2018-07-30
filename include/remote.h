

#ifndef DB_H_
#define DB_H_

#include "taobao.h"
#include "threads.h"

#include <mysql.h>

#ifdef BUILD_WITH_LIBSSH
#include <libssh2.h>
#include <libssh2_sftp.h>
#endif

#include <string>

namespace tb
{
    namespace remote
    {
        using std::string;
        using tb::thread_ns::thread;

#ifdef BUILD_WITH_LIBSSH
        class SFTPWorker
        {
            using SW = SFTPWorker;
            using csr = const string&;

            struct SFTPKeepAliveTimer : public tb::thread_ns::thread {
                SFTPKeepAliveTimer() : thread("sshTi") {}
                virtual void* start(void*, void*, void* = nullptr) override;
            };

            unsigned int ip;

            string addr;
            string user;
            string pass;
            string path;
            string passphrase;
            string remotePath;
            unsigned int port;

            unsigned int status;

            bool enableTimer;
            bool enabled;

            static SFTPWorker* instance;

            SFTPKeepAliveTimer timer;
            tb::thread_ns::mutex tm;
            int value;

            int _socket;
            LIBSSH2_SESSION* _session;
            LIBSSH2_SFTP* _sftpsession;

            int errNo;
            char* errString;
            const int esize = 256;

            void sshConnect();
            void sftpConnect();
            void doConnect();

            void close();

            int mkparent(const string&);
            int SFTPMkParentDir(const string&);

            void checkSSHError()
            {
                errNo = libssh2_session_last_error(_session, &errString, nullptr, esize);
            }

            void clearSSHError()
            {
                errNo = 0;
            }

            SFTPWorker(csr, csr, csr, csr, csr, csr, unsigned int, bool, bool = true);
            ~SFTPWorker();

            void keepAlive();

        public:
            static SW& getSFTPInstance();
            static SW& initSFTPInstance(
                csr, csr, csr, csr, csr, csr, unsigned int, bool, bool = true);
            static void destrypSFTPInstance();

            int sendFile(const char*, const char*);

            const char* tryConnect();
        };
#endif

        class SQLObject
        {
        public:
            virtual const char* getHeader() = 0;
            virtual const char* format() = 0;
        };

        class MySQLWorker : public thread
        {
            using csr = const string&;

            static const int AUTO_COMMIT_TRUE = 1;
            static const int AUTO_COMMIT_FALSE = 0;


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

            void checkDBError()
            {
                errNo = mysql_errno(_remote);
                errString = mysql_error(_remote);
            }

        public:
            virtual void* start(void*, void*, void*) override;

            void beginTransation();
            void commit();
            int query(const char*);

            const char* getRemoteServerInfo();
            bool tryConnect(const char**);
            const char* getErrorString() const;
            static MySQLWorker& getMySQLInstance();
            static MySQLWorker& initMySQLInstance(csr, csr, csr, csr, unsigned int, bool);
            static void destroyMySQLInstance();
            static const char* parseVersion(unsigned int);
        };
    }  // namespace remote
}  // namespace tb
#endif
