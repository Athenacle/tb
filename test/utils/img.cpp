
#include <unistd.h>
#include <iostream>
#include "image.h"
#include "taobao.h"
#include "tests.h"

using namespace std;
using namespace tb;
using namespace tb::utils;

int doit(const char *fn)
{
    if (::access(fn, R_OK) != 0) {
        return (-1);
    }
    int price;
    fc::Image i(fn);
    const size_t size = 64;
    char *buffer = new char[size];
    std::string fcode, bcode;
    fc::OcrResult ocr;
    i.AddWaterPrint();
    int curl;
    int ret = i.getItemCode(fcode, bcode, price, curl, ocr);
    cerr << endl
         << fn << ": " << ret << "\tfull: " << fcode << " bar: " << bcode << " price: " << price
         << endl;
    cerr << ocr.getJson();
    ocr.clear();
    fc::OcrResult res;
    ret = i.getItemAccurateCode(fcode, bcode, price, curl, res);
    cerr << endl
         << fn << ": " << ret << "\tfull: " << fcode << " bar: " << bcode << " price: " << price
         << endl;
    cerr << res.getJson() << endl;

    delete[] buffer;
    return 0;
}

int start(const char *json)
{
    using namespace Json;  //jsoncpp

    tb::utils::InitCoreUtilties();
    char *error;
    size_t size;
    char *buffer = reinterpret_cast<char *>(tb::utils::openFile(json, size, &error));
    if (buffer == nullptr) {
        log_FATAL(error);
        releaseMemory(error);
        exit(-1);
    }

    Json::CharReaderBuilder builder;
    auto r = builder.newCharReader();
    Json::CharReader *reader(r);
    Json::Value root;
    JSONCPP_STRING errs;
    bool ok = reader->parse(buffer, buffer + size, &root, &errs);
    if (!ok) {
        //Failed to parse json file.
        char *buffer = tb::utils::requestMemory(1024);
        snprintf(buffer, 1024, "Open JSON File \"%s\" failed: %s", json, errs.c_str());
        log_FATAL(buffer);
        exit(-3);
    }
    fc::ImageProcessingStartup(root);
    tb::utils::destroyFile(buffer, size, &error);
    delete r;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc <= 2) {
        cerr << "usage: ./imgTest <config.json> <file list>" << endl;
        return -1;
    } else {
        start(argv[1]);
        for (int i = 2; i < argc; i++) {
            doit(argv[i]);
        }
    }
    fc::ImageProcessingDestroy();
}
