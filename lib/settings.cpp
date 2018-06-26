/*
 * settings
 *
 */

#include "settings.h"

#include "rapidjson/pointer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>


namespace
{
    const char *ReadFile(const char *Path, unsigned int &Size)
    {
        struct stat StatBuffer;
        int StatRET = stat(Path, &StatBuffer);
        Size = 0;
        if (StatRET == 0) {
            StatRET = access(Path, F_OK | R_OK);
            if (StatRET == 0) {
                int fd = open(Path, O_RDONLY);
                assert(fd > 0);
                char *ret = new char[StatBuffer.st_size + 1];
                memset(ret, 0, StatBuffer.st_size + 1);
                Size = ::read(fd, reinterpret_cast<void *>(ret), StatBuffer.st_size);
                assert(Size == StatBuffer.st_size);
                close(fd);
                return ret;

            } else {
                std::cerr << "Read File Failed " << Path << ": " << strerror(errno) << std::endl;
                return nullptr;
            }
        } else {
            std::cerr << "Get Status of File " << Path << "Failed: " << strerror(errno)
                      << std::endl;
            return nullptr;
        }
    }

    inline char *SearchNul(char *str)
    {
        while (*str) {
            str++;
        }
        return str;
    }
};


namespace tb::settings
{
    Settings *Settings::global = nullptr;

    Settings::Settings(const char *JsonBuffer)
    {
        doc.Parse(JsonBuffer);
    }

    const Settings &Settings::InitSettings(const char *JsonFilePath)
    {
        if (Settings::global) {
            return *global;
        } else {
            unsigned int FileSize = 0;
            const char *Buffer = ReadFile(JsonFilePath, FileSize);
            if (Buffer == nullptr) {
                std::cerr << "Read Settings Failed. Exit." << std::endl;
                exit(-1);
            } else {
                Settings::global = new Settings(Buffer);
            }
            delete[] Buffer;
            return *Settings::global;
        }
    }


    bool Settings::Read(const char *Path, settings::Value &val)
    {
        const auto value = rapidjson::Pointer(Path).Get(Settings::global->doc);
        val.Object = nullptr;
        if (value->IsString()) {
            val.type = ValueType::String;
            val.String = value->GetString();
        } else if (value->IsDouble()) {
            val.type = ValueType::Double;
            val.Double = value->GetDouble();
        } else if (value->IsInt()) {
            val.type = ValueType::Integer;
            val.Integer = value->GetInt();
        } else if (value->IsBool()) {
            val.type = ValueType::Bool;
            val.Bool = value->GetBool();
        } else if (value->IsNull()) {
            val.type = ValueType::Null;
        } else if (value->IsObject()) {
            val.type = ValueType::Object;
        } else if (value->IsArray()) {
            val.type = ValueType::Array;
        }
        return true;
    }
}
