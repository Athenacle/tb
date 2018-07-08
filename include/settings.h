
#ifndef SETTINGS_H
#define SETTINGS_H

#include "taobao.h"

#include "rapidjson/document.h"

#include <string>

namespace tb
{
    namespace settings
    {
        using rapidjson::Document;
        using std::string;

        enum class ValueType { String, Integer, Double, Bool, Null, Object, Array, ERROR };

        struct Value {
            ValueType type;
            union {
                const char* String;
                int64_t Integer;
                double Double;
                bool Bool;
                void* Null;
                void* Object;
                void* Array;
            };
        };

        class Settings
        {
            static Settings* global;
            Document doc;

            Settings(const char*);

        public:
            static const Settings& InitSettings(const char*);

            static void DeInitSettings()
            {
                delete Settings::global;
                global = nullptr;
            }

            static bool checkPathExists(const char*);
            static bool Read(const char*, settings::Value&);
        };
    };
}


#endif
