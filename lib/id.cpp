
#include "id.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <cstring>
#include <string>
#include <vector>

namespace only
{
    using namespace boost;

    bool checkBarCodeValidate(const std::string& bar)
    {
        static regex barCode("^[123][012]\\d[1234][0-9A-Z]{5}\\d{4}$", regex::perl | regex::icase);
        return regex_match(bar, barCode);
    }


    bool checkFullCode(const std::string& _code)
    {
        static regex fullCode("^[123][012]\\d[1234][0-9A-Z]{5}[A-Z0-9]{2}\\d{3}",
                              regex::perl | regex::icase);
        return regex_match(_code, fullCode);
    }

    bool restoreFullCode(const std::string& raw, const std::string& bar, std::string& result)
    {
        static regex fullCodeSuffix("\\d*[A-Z0-9]{3}[0-9]{3}$", regex::perl | regex::icase);
        auto start = raw.cbegin();
        auto end = raw.cend();

        boost::match_results<decltype(start)> what;
        boost::match_flag_type flags = boost::match_default;

        if (regex_search(start, end, what, fullCodeSuffix, flags)) {
            auto rawPointer = raw.cend() - 7;
            auto barPointer = bar.cbegin() + 8;
            if (*rawPointer != *barPointer) {
                return false;
            }
            result = bar.substr(0, 9);
            std::string suffix(what[0].first, what[0].second);
            result.append(suffix.substr(suffix.length() - 6, suffix.length()));
            return true;
        }
        return false;
    }


    bool checkFullBarCode(const std::string& f, const std::string& b)
    {
        return f.substr(0, 8) == b.substr(0, 8);
    }

    bool checkOcrOutput(const char* ocr, std::string& fcode, std::string& bcode)
    {
        static regex separator("\\s");
        std::vector<std::string> splited;
        boost::algorithm::split(splited, ocr, boost::algorithm::is_space());
        splited.erase(
            std::remove_if(
                splited.begin(), splited.end(), [&](std::string& sp) { return sp.length() < 8; }),
            splited.end());
        if (splited.size() == 2) {
            fcode = splited[0];
            bcode = splited[1];
            return true;
        }
        return false;
    }

}  // namespace only
