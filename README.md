# C++ Socket server epool coroutines (c++20)    

-   Sources, headers and mains separated in distinct folders
-   Use of modern [CMake](https://cmake.org/) for much easier compiling
-   Setup for tests using [doctest](https://github.com/onqtam/doctest)
-   Code coverage reports (TODO:review this) including automatic upload to [Coveralls.io](https://coveralls.io/) and/or [Codecov.io](https://codecov.io)
-   Code documentation with [Doxygen](http://www.stack.nl/~dimitri/doxygen/)

## Requirements

doxygen lcov build-essential gdb cmake

sudo apt install doxygen lcov build-essential gdb cmake



## Structure

You can find `main` executable in [app/main.cc](app/main.cpp) under the `Build` section in [CMakeLists.txt](CMakeLists.txt).

## Building, Testing and Running

Build by making a build directory (i.e. `build/`), run `cmake` in that dir, and then use `make` to build the desired target.

Example:

``` bash
> mkdir build && cd build
> cmake .. -DCMAKE_BUILD_TYPE=[Debug | Coverage | Release]
> make
```
For testing, first start the main executable and then run the python test script in another console
``` bash
> ./main         # Runs the main executable
> python3 ../tests/python-testclient/pythonTcpClient.py #runs a sample python client 
```
For unit testing folow this instructions


``` bash
> make test      # Makes and runs the tests.
> make coverage  # Makes and runs the tests, then Generates a coverage report. (requires "cmake .. -DCMAKE_BUILD_TYPE=Coverage")
> make doc       # Generate html documentation.
> ./unit_tests -s #runs unittests with details
> 
```
Testing 

There are some simple tests implemented in C++ 
the most important test is a set of scripts

start the main app in a console and in another console type one of the following  
/tests/python-testclient$ python pythonTcpClient.py -ip 127.0.0.1 
/tests/python-testclient$ python pythonTcpClient.py -ip 127.0.0.1 -f ../samplestate.txt
/tests/python-testclient$ ./runclientlocal.sh