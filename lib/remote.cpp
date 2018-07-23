
#include "remote.h"
#include "logger.h"
#include "taobao.h"

#include <cassert>

#ifdef BUILD_WITH_LIBSSH
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif


namespace tb
{
#ifdef BUILD_WITH_LIBSSH
    namespace
    {
        int string2ip(const string& _ip, void* _buf)
        {
            return inet_pton(AF_INET, _ip.c_str(), _buf);
        }
    };  // namespace
#endif

    namespace remote
    {
        MySQLWorker* MySQLWorker::instance = nullptr;

#ifdef BUILD_WITH_LIBSSH
        SFTPWorker* SFTPWorker::instance = nullptr;

        void* SFTPWorker::SFTPKeepAliveTimer::start(void* _m, void* _v, void*)
        {
            tb::thread_ns::mutex* m = reinterpret_cast<tb::thread_ns::mutex*>(_m);
            int* p = reinterpret_cast<int*>(_v);
            do {
                sleep(7);
                m->lock();
                auto value = *p;
                m->unlock();
                if (value == 0) {
                    log_INFO("SFTP Keep Alive Timer Closed.");
                    break;
                }
                SFTPWorker::getSFTPInstance().keepAlive();
            } while (true);
            return nullptr;
        }

        void SFTPWorker::keepAlive()
        {
            int next = 10;
            libssh2_keepalive_send(_session, &next);
            log_INFO("Send Keep Alive Timer to remote.");
        }

        SFTPWorker& SFTPWorker::initSFTPInstance(const string& _addr,
                                                 const string& _user,
                                                 const string& _pass,
                                                 const string& _path,
                                                 const string& _passph,
                                                 const string& _fpath,
                                                 unsigned int _port,
                                                 bool _enable,
                                                 bool _enableTimer)
        {
            if (SFTPWorker::instance != nullptr) {
                assert(0);
            }
            SFTPWorker::instance = new SFTPWorker(
                _addr, _user, _pass, _path, _passph, _fpath, _port, _enable, _enableTimer);
            return SFTPWorker::getSFTPInstance();
        }

        SFTPWorker::SFTPWorker(const string& _addr,
                               const string& _user,
                               const string& _pass,
                               const string& _path,
                               const string& _passph,
                               const string& _fpath,
                               unsigned int _port,
                               bool _enable,
                               bool _enableTimer)
            : addr(_addr),
              user(_user),
              pass(_pass),
              path(_path),
              passphrase(_passph),
              remotePath(_fpath),
              port(_port),
              enableTimer(_enableTimer),
              enabled(_enable)
        {
            value = 1;
            _session = nullptr;
            _sftpsession = nullptr;
            ip = 0;
            status = CONNECTION_NOT_REAL_CONNECT;
            enabled = true;
            errString = tb::utils::requestMemory(esize);
        }

        void SFTPWorker::destrypSFTPInstance()
        {
            if (SFTPWorker::instance != nullptr) {
                SFTPWorker::instance->close();
                delete SFTPWorker::instance;
                SFTPWorker::instance = nullptr;
            }
        }

        void SFTPWorker::doConnect()
        {
            sshConnect();
            if (status == CONNECTION_SUCCESS) {
                sftpConnect();
            }
        }

        void SFTPWorker::close()
        {
            log_INFO("Begin SFTP Connection Shutdown proceudre.");
            tm.lock();
            value = 0;
            tm.unlock();
            timer.join();
            libssh2_sftp_shutdown(_sftpsession);
            libssh2_session_disconnect(_session, "close");
            libssh2_session_free(_session);
            libssh2_exit();
            ::close(_socket);
            log_INFO("SFTP Connection Closed");
        }

        SFTPWorker::~SFTPWorker() {}

        void SFTPWorker::sftpConnect()
        {
            assert(status == CONNECTION_SUCCESS);
            size_t bsize = 1024;
            char* buffer = tb::utils::requestMemory(bsize);
            _sftpsession = libssh2_sftp_init(_session);
            if (_sftpsession == nullptr) {
                checkSSHError();
                snprintf(buffer, bsize, "Initlize SFTP Session failed: %s", errString);
                log_ERROR(buffer);
                status = CONNECTION_FAILED;
                return;
            } else {
                snprintf(buffer, bsize, "Initlize SFTP Session successfully.");
                log_INFO(buffer);
            }
        }


        int SFTPWorker::mkparent(const string& f)
        {
            string parent;
            int ret;
            bool p = tb::utils::getParentDir(f, parent);
            if (p) {
                ret = mkparent(parent);
                if (ret == -1) {
                    return -1;
                }
            }
            errno = 0;
            LIBSSH2_SFTP_ATTRIBUTES attrib;

            do {
                ret = libssh2_sftp_stat(_sftpsession, f.c_str(), &attrib);
            } while (ret == LIBSSH2_ERROR_EAGAIN);
            if (ret == 0) {
                if (!LIBSSH2_SFTP_S_ISDIR(attrib.flags)) {
                    return 0;
                } else {
                    return -1;
                }
            } else {
                char buf[512];
                snprintf(buf, 512, "Remote mkdir of %s", f.c_str());
                errno = 0;
                clearSSHError();
                ret = libssh2_sftp_mkdir(_sftpsession, f.c_str(), 0755);
                return ret;
            }
        }

        int SFTPWorker::SFTPMkParentDir(const string& f)
        {
            // f -> /some/path/file.ext
            // #1 extract parent directory /some/path
            string parent;  // parent => /some/path
            tb::utils::getParentDir(f, parent);

            return mkparent(parent);
        }


        int SFTPWorker::sendFile(const char* f, const char *rf)
        {
            if (status == CONNECTION_FAILED) {
                log_DEBUG("CONNECTION_FAILED");
                return -1;
            }
            size_t fsize;
            char* buffer;
            const size_t bsize = 256;
            char buf[bsize];
            int ret = 0;
            char* file = reinterpret_cast<char*>(tb::utils::openFile(f, fsize, &buffer));
            if (file == nullptr) {
                log_ERROR(buffer);
                tb::utils::releaseMemory(buffer);
                return -1;
            }
            struct stat st;
            stat(f, &st);
            string remotefile = remotePath + "/" + rf;
            tb::utils::formatDirectoryPath(remotePath);

            ret = SFTPMkParentDir(remotefile);
            if (ret == -1) {
                return -1;
            }
            auto channel = libssh2_scp_send(_session, remotefile.c_str(), st.st_mode & 0755, fsize);
            errno = 0;
            if (channel == nullptr) {
                checkSSHError();
                snprintf(buf, bsize, "Open SCP Channel failed: %s", errString);
                log_ERROR(buf);
                return -1;
            } else {
                log_DEBUG("Open SCP Channel successfully");
            }
            errno = 0;
            auto wrote = libssh2_channel_write(channel, file, fsize);
            if (wrote < 0) {
                snprintf(buf, bsize, "Write file to Remote error: %s.", strerror(errno));
                log_ERROR(buf);
                ret = -1;
            } else {
                libssh2_channel_send_eof(channel);
                libssh2_channel_wait_eof(channel);
                libssh2_channel_wait_closed(channel);
                libssh2_channel_free(channel);
                snprintf(buf, bsize, "Send file %s -> %s successfully.", f, remotefile.c_str());
                log_INFO(buf);
            }
        }

        const char* SFTPWorker::tryConnect()
        {
            doConnect();
            return status == CONNECTION_SUCCESS ? nullptr : errString;
        }

        void SFTPWorker::sshConnect()
        {
            const int bsize = 1024;
            char* buffer = tb::utils::requestMemory(bsize);
            int ret = libssh2_init(0);
            if (ret != 0) {
                log_ERROR("Initlize Faild.");
                enabled = false;
                status = CONNECTION_FAILED;
            }
            _socket = socket(AF_INET, SOCK_STREAM, 0);

            struct sockaddr_in sin;
            sin.sin_family = AF_INET;
            sin.sin_port = htons(22);
            sin.sin_addr.s_addr = ip;
            string2ip(addr, &sin.sin_addr.s_addr);
            if (connect(_socket, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
                snprintf(buffer, bsize, "Connect to remote failed: %s", strerror(errno));
                log_ERROR(buffer);
                enabled = false;
                status = CONNECTION_FAILED;
            }
            _session = libssh2_session_init();
            libssh2_session_set_blocking(_session, 1);

            while ((ret = libssh2_session_handshake(_session, _socket)) == LIBSSH2_ERROR_EAGAIN)
                ;
            if (ret != 0) {
                checkSSHError();
                snprintf(buffer, bsize, "Session Handshake failed: %s", errString);
                log_ERROR(buffer);
                enabled = false;
                status = CONNECTION_FAILED;
                return;
            }

            auto fingerprint = libssh2_hostkey_hash(_session, LIBSSH2_HOSTKEY_HASH_SHA1);
            {
                char fpbuf[64];
                char fbuf[4];
                fpbuf[0] = 0;
                for (int i = 0; i < 20; i++) {
                    snprintf(fbuf, 4, "%02x:", (unsigned char)fingerprint[i]);
                    strcat(fpbuf, fbuf);
                }
                fpbuf[59] = 0;
                snprintf(buffer, bsize, "Remote Fingerprint: %s", fpbuf);
                log_INFO(buffer);
            }
            auto cUser = getpwuid(geteuid());
            auto cUsername = cUser->pw_name;
            auto cUDir = cUser->pw_dir;
            auto username = user == "" ? cUsername : user.c_str();

            if (pass != "") {
                // password is not empty
                // try auth via password
                while ((ret = libssh2_userauth_password(_session, username, pass.c_str()))
                       == LIBSSH2_ERROR_EAGAIN)
                    ;
                if (ret != 0) {
                    snprintf(buffer, bsize, "Password authentication failed, try public key");
                    log_WARNING(buffer);
                }
            }
            {
                const static char pubkeyFiles[][24] = {"id_rsa",
                                                       "id_dsa",
                                                       "id_dsa-cert",
                                                       "id_ecdsa",
                                                       "id_ecdsa-cert",
                                                       "id_ed25519",
                                                       "id_ed25519-cert",
                                                       "id_xmss",
                                                       "id_xmss-cert"};
                for (auto pf : pubkeyFiles) {
                    auto dirsize = strlen(cUDir) + 40;
                    char* pub = tb::utils::requestMemory(dirsize);
                    snprintf(buffer, bsize, "%s/.ssh/%s", cUDir, pf);
                    snprintf(pub, dirsize, "%s/.ssh/%s.pub", cUDir, pf);
                    if (access(buffer, R_OK) == 0 && access(pub, R_OK) == 0) {
                        do {
                            ret = libssh2_userauth_publickey_fromfile(
                                _session, username, pub, buffer, passphrase.c_str());
                        } while (ret == LIBSSH2_ERROR_EAGAIN);
                        if (ret == 0) {
                            status = CONNECTION_SUCCESS;
                            break;
                        }
                    }
                }
            }
            if (status == CONNECTION_SUCCESS) {
                snprintf(buffer, bsize, "SSH Channel established successfully.");
                if (enableTimer) {
                    value = 0;
                    timer.begin(&tm, &value);
                }
                log_INFO(buffer);
            } else {
                checkSSHError();
                snprintf(buffer,
                         bsize,
                         "SSH Channel established failed: %s. SFTPWorker diabled ",
                         errString);
                log_ERROR(buffer);
                enabled = false;
            }
        }

        SFTPWorker& SFTPWorker::getSFTPInstance()
        {
            if (SFTPWorker::instance == nullptr) {
                assert(0);
            }
            return *SFTPWorker::instance;
        }

#endif

        MySQLWorker& MySQLWorker::getMySQLInstance()
        {
            assert(MySQLWorker::instance != nullptr);
            return *MySQLWorker::instance;
        }

        void MySQLWorker::close()
        {
            mysql_close(_remote);
            log_INFO("MySQL Connection Closed.");
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

        void MySQLWorker::beginTransation()
        {
            if (status == CONNECTION_SUCCESS_DB_CHANGED) {
                mysql_autocommit(_remote, AUTO_COMMIT_FALSE);
            }
        }

        int MySQLWorker::query(const char* sql)
        {
            return mysql_query(_remote, sql);
        }

        void MySQLWorker::commit()
        {
            checkDBError();
            mysql_commit(_remote);
            mysql_autocommit(_remote, AUTO_COMMIT_TRUE);
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

    }  // namespace remote
}  // namespace tb
