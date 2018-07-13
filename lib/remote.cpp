
#include "remote.h"
#include "taobao.h"

#include <cassert>

namespace tb
{
    namespace remote
    {
        MySQLWorker* MySQLWorker::instance = nullptr;

        void MySQLWorker::close()
        {
            mysql_close(_remote);
        }

        MySQLWorker::~MySQLWorker()
        {
            delete _remote;
        }

        void MySQLWorker::destroyMySQLInstance()
        {
            MySQLWorker::instance->close();
            delete MySQLWorker::instance;
        }

        const char* MySQLWorker::getRemoteServerInfo()
        {
            return mysql_get_server_info(_remote);
        }


        MySQLWorker::MySQLWorker(const string& _add,
                                 const string& _user,
                                 const string& _pass,
                                 const string& _db,
                                 unsigned int _port,
                                 bool _comp)
            : port(_port), addr(_add), user(_user), pass(_pass), db(_db), compress(_comp)
        {
            status = CONNECTION_NOT_REAL_CONNECT;
            _remote = new MYSQL;
            errString = nullptr;
            mysql_init(_remote);
        }

        void MySQLWorker::doConnect()
        {
            unsigned long mask = CLIENT_MULTI_STATEMENTS;
            if (compress) {
                mask |= CLIENT_COMPRESS;
            }
            if (!mysql_real_connect(_remote,
                                    addr.c_str(),
                                    user.c_str(),
                                    pass.c_str(),
                                    nullptr,
                                    port,
                                    nullptr,
                                    mask)) {
                checkDBError();
                status = CONNECTION_FAILED;
                return;
            } else {
                status = CONNECTION_SUCCESS;
            }
            if (0 != mysql_select_db(_remote, db.c_str())) {
                checkDBError();
            } else {
                status = CONNECTION_SUCCESS_DB_CHANGED;
            }
        }

        void* MySQLWorker::start(void*, void*, void*)
        {
            //FIXME: add code.
            return nullptr;
        }


        bool MySQLWorker::tryConnect(const char** err)
        {
            if (status == CONNECTION_NOT_REAL_CONNECT) {
                doConnect();
            }
            if (status != CONNECTION_SUCCESS_DB_CHANGED) {
                *err = errString;
                return false;
            } else {
                return true;
            }
        }

        MySQLWorker& MySQLWorker::initMySQLInstance(const string& _add,
                                                    const string& _user,
                                                    const string& _pass,
                                                    const string& _db,
                                                    unsigned int _port,
                                                    bool _comp)
        {
            if (MySQLWorker::instance == nullptr) {
                MySQLWorker::instance = new MySQLWorker(_add, _user, _pass, _db, _port, _comp);
            } else {
                assert(0);
            }
            return *MySQLWorker::instance;
        }

    }  // namespace db
}  // namespace tb
