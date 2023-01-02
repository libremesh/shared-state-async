#include <iostream>
#include <array>
#include <unistd.h>
#include <optional>
#include <expected.hpp>
#include "socket.hh"

namespace SharedState
{
    enum class Status
    {
        Ok,
        AccessDenied,
        DataSourceError,
        PipeError,
    };

    int mergestate(std::string arguments, std::string &output);
    //std::string mergestate(std::string arguments);
    //std::string mergestate(std::string arguments);//, Socket* s);
    std::string mergestate(std::string arguments);
    std::optional<std::string> optMergeState(std::string arguments);
    tl::expected<std::string,std::error_condition> expMergestate(std::string arguments,bool willFail = false);

}