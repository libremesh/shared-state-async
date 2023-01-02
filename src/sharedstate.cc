#include "sharedstate.hh"
#include <algorithm>
#include <optional>
#include <expected.hpp>
#include "shared_state_error_code.hh"
#include <chrono>
#include "socket.hh"

namespace SharedState
{
 

    std::error_condition reqSync (const std::string& stateSlice,std::string& newState)
    {
        newState = stateSlice;
        //llamar a lua shared state reqSync
        return std::error_condition();
    }

    //std::error_condition merge(const std::string& stateSlice)
    int mergestate(std::string stateSlice, std::string &output)
//    int merge(const std::string& stateSlice, std::string& output)
    {
        const int bufsize = 128;
        std::array<char, bufsize> buffer;
        std::string cmd = "sleep 10 && echo '" + stateSlice + "'";

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
/*
    std::string mergestate(std::string arguments)//, Socket* s)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "sleep 1 && echo '" + arguments + "'";
        auto begin = std::chrono::high_resolution_clock::now();
        auto pipe = popen(cmd.c_str(), "r");
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "popen..:" << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << std::endl;        
        if (!pipe)
            throw std::runtime_error("popen() failed!");
        
        begin = std::chrono::high_resolution_clock::now();
         
        //Socket filesocket{pipe,s};
        //ssize_t nbRecv = co_await filesocket.recvfile(buffer.data(),128);
        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }
        end = std::chrono::high_resolution_clock::now();
        std::cout<< "fgets..:"  << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << std::endl;
        //filesocket.~Socket();
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
*/
std::string mergestate(std::string arguments)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "sleep 1 && echo '" + arguments + "'";
        auto begin = std::chrono::high_resolution_clock::now();
        auto pipe = popen(cmd.c_str(), "r");
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "popen..:" << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << std::endl;        
        if (!pipe)
            throw std::runtime_error("popen() failed!");
        
        begin = std::chrono::high_resolution_clock::now();
         
        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }
        end = std::chrono::high_resolution_clock::now();
        std::cout<< "fgets..:"  << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << std::endl;
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
    std::optional<std::string> optMergeState(std::string arguments)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "sleep 1 && echo '" + arguments + "'";
        auto pipe = popen(cmd.c_str(), "r");

        if (!pipe){
            std::cerr << "error opening pipe";

            return {};
        }

        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }

        auto rc = pclose(pipe);

        if (rc == EXIT_SUCCESS)
        {
        }
        else if (rc == EXIT_FAILURE)
        {
            std::cerr << "error on merge";
            return {};
        }
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
        return result;
    }
    

    /// @brief error_condition ... > es como error code pero crossplatform 
    /// error_code ... es dependiente de plataforma 
    /// @param arguments 
    /// @return 
    tl::expected<std::string,std::error_condition> expMergestate(std::string arguments,bool willFail)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "echo '" + arguments + "'";
        auto pipe = popen(cmd.c_str(), "r");

        if (!pipe || willFail)
        {
            return tl::unexpected<std::error_condition> {make_error_condition(SharedStateErrorCode::OpenPipeError)};
        }
        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }

        auto rc = pclose(pipe);

        if (rc == EXIT_FAILURE)
        {
            return tl::unexpected<std::error_condition> (make_error_condition(SharedStateErrorCode::OpenPipeError));
        }
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
        return result;
    }
    
}