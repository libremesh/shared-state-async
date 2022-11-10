#include <iostream>
#include <array>
#include <unistd.h>

namespace SharedState
{
    int mergestate(std::string arguments, std::string &output);
    std::string mergestate(std::string arguments);
}