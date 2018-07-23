
#include "logger.h"
#include "remote.h"
#include "taobao.h"
#include "tests.h"

using namespace tb::remote;

namespace
{
}

int main(void)
{
    SFTPWorker &work = SFTPWorker::initSFTPInstance(
        "10.70.20.1", "wangxiao", "", "", "", "/apple/dfdorange/cdgapple", 22, true);
    work.tryConnect();
    work.sendFile("a.out", "a.out");
    return 0;
}
