# lldb-mi

LLDB's machine interface driver.

# Build

lldb-mi uses CMake to build. The only dependencies needed for lldb-mi are a C++14 compiler and LLDB itself (including its dependencies: Clang and LLVM). These dependencies should be installed in a way that they can be found via CMake's [`find_package`](https://cmake.org/cmake/help/latest/command/find_package.html) functionality. You need both the LLDB/Clang/LLVM headers and their compiled libraries for the build to work, but not the respective source files. 

# Building against system LLDB

If your distribution or operating system already ships a correctly configured LLDB/Clang/LLVM installation, lldb-mi can be build by simply running:


```bash
cmake .
cmake --build .
```

# Building against custom LLDB

You can also build lldb-mi against a LLDB that you compiled yourself. For that compile LLDB as described [here](https://lldb.llvm.org/resources/build.html) but set `CMAKE_INSTALL_PREFIX` to a local directory and build the LLVM shared library by passing `-DLLVM_BUILD_LLVM_DYLIB=On` to CMake. Afterwards point towards that prefix directory when building lldb-mi by settings `CMAKE_PREFIX_PATH` (e.g. `cmake -DCMAKE_PREFIX_PATH=/home/yourname/lldb-mi/install`).

This example script should build LLVM and lldb-mi on an average UNIX system in the ~/buildspace subfolder:
```
cd
mkdir buildspace

# Download LLVM/Clang/LLDB and build them.
git clone https://github.com/llvm/llvm-project.git
mkdir llvm-inst
mkdir llvm-build
cd llvm-build

cmake -DLLVM_ENABLE_PROJECTS="clang;lldb;libcxx;libcxxabi" -DCMAKE_INSTALL_PREFIX=~/buildspace/llvm-inst/ -GNinja ../llvm-project/llvm
ninja
ninja install

# Download lldb-mi and build it against our custom installation.
cd ~/buildspace
git clone https://github.com/lldb-tools/lldb-mi
cd lldb-mi

# Create a separate build directory for building lldb-mi.
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=~/buildspace/llvm-inst/ -GNinja ..
ninja
```

# Building against custom LLDB.framework (Darwin Only)

You can also build lldb-mi against a LLDB.framework that you compiled yourself. It is the same instructions as above but build the LLVM framework by passing `-DLLDB_BUILD_FRAMEWORK=1` to CMake instead of using the shared library. Then build LLDB-MI with `-DUSE_LLDB_FRAMEWORK=1`

The snippits below change the cmake calls from the above script.
```
// Building llvm-project
cmake -DLLVM_ENABLE_PROJECTS="clang;lldb;libcxx;libcxxabi" -DCMAKE_INSTALL_PREFIX=~/buildspace/llvm-inst/ -DLLDB_BUILD_FRAMEWORK=1 -GNinja ../llvm-project/llvm
```

```
// Building lldb-mi
cmake -DCMAKE_PREFIX_PATH=~/buildspace/llvm-inst/ -DUSE_LLDB_FRAMEWORK=1 -GNinja ..

```

# Notes

On some architectures (e. g. Linux, x86-64), LLDB dinamic library fails to determine its location. That results to inability of locating a gdbserver stub:
```bash
process launch failed: unable to locate lldb-server
```

The workaraund is to set LLDB_DEBUGSERVER_PATH environment variable before running LLDB-MI.
