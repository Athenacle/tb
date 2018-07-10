

#include <iostream>

#include "fchecker.h"

using std::cout;
using std::endl;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "unknown"
#endif

void version(const char* name)
{
    cout << name << " Version: " << PROJECT_VERSION << endl << "----------" << endl;
    cout << "Build with "
#ifdef USE_POSIX_THREAD
         << "POSIX Thread Model." << endl;
#else
         << "C++ 11 Thread Model" << endl;
#endif
    cout << "Build with Boost Version " << BOOST_VERSION << endl;
    cout << "Build with ZLIB Version " << ZLIB_VERSION << endl;
    exit(0);
}
