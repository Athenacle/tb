
#include "image.h"
#include "taobao.h"
#include "tests.h"
#include <unistd.h>
#include <iostream>
using namespace std;


int doit(const char *fn)
{
    if (::access(fn, R_OK) != 0){
        return (-1);
    }
    fc::Image i(fn);
    const size_t size = 64;
    char *buffer = new char[size];
    int ret = i.getBarCode(buffer, size);
    cout << fn << ": "<< ret << "\t";
    if (ret == 1) {
        cout << buffer;
    }
    cout << endl;
    delete []buffer;
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 0){
        cerr << "usage: ./imgTest <file list>" << endl;
        return -1;
    }else{
        for (int i = 1; i < argc; i++){
            doit(argv[i]);
        }
    }
}
