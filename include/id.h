
#ifndef ID_H_
#define ID_H_

#include <string>
#include "taobao.h"

namespace only
{
    bool checkBarCodeValidate(const std::string&);

    bool checkFullCode(const std::string&);

    bool restoreFullCode(const std::string&, const std::string&, std::string&);

    int InitCodeChecker();

    bool checkFullBarCode(const std::string&, const std::string&);

    bool checkOcrOutput(const char*, std::string&, std::string&);

    int checkPrice(const std::string&);

}  // namespace only

#endif
