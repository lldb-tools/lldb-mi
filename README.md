# lldb-mi

LLDB's machine interface driver.

# Build

lldb-mi uses CMake to build. The only dependencies needed for lldb-mi are a C++11 compiler and LLDB itself (including its dependencies: Clang and LLVM). These dependencies should be installed in a way that they can be found via CMake's [`find_package`](https://cmake.org/cmake/help/latest/command/find_package.html) functionality. You need both the LLDB/Clang/LLVM headers and their compiled libraries for the build to work, but not the respective source files. 

# Building against system LLDB

If your distribution or operating system already ships a correctly configured LLDB/Clang/LLVM installation, lldb-mi can be build by simple running:


```bash
cmake .
cmake --build .
```

# Building against custom LLDB

You can also build lldb-mi against a LLDB that you compiled yourself. For that compile LLDB as described [here](https://lldb.llvm.org/resources/build.html) but set `CMAKE_INSTALL_PREFIX` to a local directory. Afterwards point towards that prefix directory when building lldb-mi by settings `CMAKE_MODULE_PATH` (e.g. `cmake -DCMAKE_MODULE_PATH=/home/me/my-lldb/install` .`).
