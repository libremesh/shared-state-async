#include <iostream>
#include <array>
#include <unistd.h>
#include <optional>
#include <expected.hpp>

namespace SharedState
{
    int mergestate(std::string arguments, std::string &output);
    std::string mergestate(std::string arguments);
    std::optional<std::string> optMergeState(std::string arguments);
}