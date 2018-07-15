

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

            unsigned int ip;

            string addr;
            string user;
            string pass;
            string path;
            string passphrase;
            string remotePath;
            unsigned int port;

            unsigned int status;

            bool enabled;

            static SFTPWorker* instance;

            int _socket;
            LIBSSH2_SESSION* _session;
            LIBSSH2_SFTP* _sftpsession;
            LIBSSH2_SFTP_HANDLE* _handle;


            int errNo;
            char* errString;
            const int esize = 256;

            void sshConnect();
            void sftpConnect();
            void doConnect();

            void close();

            void checkSSHError()
            {
                errNo = libssh2_session_last_error(_session, &errString, nullptr, esize);
            }

            SFTPWorker(csr, csr, csr, csr, csr, csr, unsigned int, bool);
            ~SFTPWorker() ;

        public:
            static SW& getSFTPInstance();
            static SW& initSFTPInstance(csr, csr, csr, csr, csr, csr, unsigned int, bool);
            static void destrypSFTPInstance();

            const char* tryConnect();
        };
#endif

        class MySQLWorker : public thread
        {
            using csr = const string&;

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
