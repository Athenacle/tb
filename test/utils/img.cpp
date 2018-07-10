
#include <unistd.h>
#include <iostream>
#include "image.h"
#include "taobao.h"
#include "tests.h"
using namespace std;


int doit(const char *fn)
{
    if (::access(fn, R_OK) != 0) {
        return (-1);
    }
    fc::Image i(fn);
    const size_t size = 64;
    char *buffer = new char[size];
    std::string fcode, bcode;

    int ret = i.getItemCode(fcode, bcode);
    cout << fn << ": " << ret << "\tfull: ";
    cout << fcode << " bar: " << bcode << endl;
    delete[] buffer;
    return 0;
}

int main(int argc, char *argv[])
{
    //fc::ImageProcessingStartup();
    if (argc == 0) {
        cerr << "usage: ./imgTest <file list>" << endl;
        return -1;
    } else {
        for (int i = 1; i < argc; i++) {
            doit(argv[i]);
        }
    }
    fc::ImageProcessingDestroy();
}
