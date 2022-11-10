#include "sharedstate.hh"
#include <algorithm>

namespace SharedState
{

int mergestate(std::string arguments, std::string &output)
{
    const int bufsize = 128;
    std::array<char, bufsize> buffer;
    std::string cmd = "echo '" + arguments +"'";

    auto pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");

    size_t count;
    do
    {
        if ((count = fread(buffer.data(), 1, bufsize, pipe)) > 0)
        {
            output.insert(output.end(), std::begin(buffer), std::next(std::begin(buffer), count));
        }
    } while (count > 0);
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.cend());
    return pclose(pipe);
}

std::string mergestate(std::string arguments)
{
    std::array<char, 128> buffer;
    std::string result;
    std::string cmd = "echo '" + arguments +"'";
    auto pipe = popen(cmd.c_str(), "r");

    if (!pipe)
        throw std::runtime_error("popen() failed!");

    while (!feof(pipe))
    {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }

    auto rc = pclose(pipe);

    if (rc == EXIT_SUCCESS)
    { // == 0
    }
    else if (rc == EXIT_FAILURE)
    { // EXIT_FAILURE is not used by all programs, maybe needs some adaptation.
    
    }
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
    return result;
}
}