/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023  Asociación Civil Altermundi <info@altermundi.net>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "sharedstate.hh"
#include "shared_state_error_code.hh"

#include <chrono>
#include <iostream>
#include <regex>
#include <algorithm>
#include <optional>
#include <arpa/inet.h>

#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>

namespace SharedState
{
std::task<int> receiveNetworkMessage(
        Socket& socket, NetworkMessage& networkMessage,
        std::error_condition* errbub )
{
	RS_DBG4(socket);
	// TODO: define and use proper error_conditions to return
	// TODO: deal with socket errors

	int receivedBytes = 0;

	networkMessage.mTypeName.clear();
	networkMessage.mData.clear();


	uint8_t dataTypeNameLenght = 0;
	receivedBytes += co_await socket.recv(&dataTypeNameLenght, 1);

	RS_DBG2(socket, " dataTypeNameLenght: ", static_cast<int>(dataTypeNameLenght));

	if(dataTypeNameLenght < 1 || dataTypeNameLenght > DATA_TYPE_NAME_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            " ", socket,
		            " Got data type name invalid lenght: ",
		            static_cast<int>(dataTypeNameLenght) );
		co_return -receivedBytes;
	}

	networkMessage.mTypeName.resize(dataTypeNameLenght, static_cast<char>(0));
	receivedBytes += co_await socket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mTypeName.data()),
	            dataTypeNameLenght );

	RS_DBG2(socket, " networkMessage.mTypeName: ", networkMessage.mTypeName);

	uint32_t dataLenght = 0;
	receivedBytes += co_await socket.recv(
	            reinterpret_cast<uint8_t*>(&dataLenght), 4 );
	dataLenght = ntohl(dataLenght);


	if(dataLenght < 2 || dataLenght > DATA_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            socket, " Got data invalid lenght: ", dataLenght);
		co_return -receivedBytes;
	}

	networkMessage.mData.resize(dataLenght, 0);
	auto receivedDataBytes =  co_await
	        socket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mData.data()),
	            dataLenght );
	receivedBytes += receivedDataBytes;

	RS_DBG2( socket,
	         " Expected data lenght: ", dataLenght,
	         " received data bytes: ", receivedDataBytes );

	RS_DBG4( socket, " networkMessage.mData: ", networkMessage.mData);

	RS_DBG2( socket, " Total received bytes: ", receivedBytes);
	co_return receivedBytes;
}

std::task<int> sendNetworkMessage(
        Socket& socket, const NetworkMessage& netMsg,
        std::error_condition* errbub )
{
	RS_DBG2(socket);

	int sentBytes = 0;

	uint8_t dataTypeLen = netMsg.mTypeName.length();
	sentBytes += co_await socket.send(&dataTypeLen, 1);

	RS_DBG2(socket, " sent dataTypeLen: ", static_cast<int>(dataTypeLen));

	sentBytes += co_await socket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mTypeName.data()),
	            dataTypeLen );

	RS_DBG2( socket, " sent netMsg.mTypeName: ", netMsg.mTypeName );

	uint32_t dataTypeLenNetOrder = htonl(netMsg.mData.size());
	sentBytes += co_await socket.send(
	            reinterpret_cast<uint8_t*>(&dataTypeLenNetOrder), 4);

	RS_DBG2( socket, " sent netMsg.mData.size(): ", netMsg.mData.size() );

	sentBytes += co_await socket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mData.data()),
	            netMsg.mData.size() );

	RS_DBG4( socket, " sent netMsg.mData: ", netMsg.mData);

	RS_DBG2( socket, " Total bytes sent: ", sentBytes );
	co_return sentBytes;
}

    std::string extractCommand(std::string &inputString)
    {
        std::string delimiter = "\n";
        size_t pos = 0;
        if ((pos = inputString.find(delimiter)) != std::string::npos)
        {
            std::string command = inputString.substr(0, pos);
            inputString.erase(0, pos + delimiter.length());
            return command;
        }
        return "";
    }

    std::error_condition extractCommand(std::string &inputString, std::string &command)
    {
        std::string delimiter = "\n";
        size_t pos = 0;
        if ((pos = inputString.find(delimiter)) != std::string::npos)
        {
            command = inputString.substr(0, pos);
            inputString.erase(0, pos + delimiter.length());
            return std::error_condition();
        }
        return make_error_condition(SharedStateErrorCode::NoCommand);
    }

    std::error_condition reqSync(const std::string &stateSlice, std::string &newState)
    {
        newState = stateSlice;
        // llamar a lua shared state reqSync
        return std::error_condition();
    }

    // std::error_condition merge(const std::string& stateSlice)
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
        output = std::regex_replace(output, std::regex("\\r\\n|\\r|\\n"), "");
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
            RS_DBG0("")<< "popen..:" << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << std::endl;
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
            //result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
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
        RS_DBG0("popen..:", std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
        if (!pipe)
            throw std::runtime_error("popen() failed!");

        begin = std::chrono::high_resolution_clock::now();

        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }
        end = std::chrono::high_resolution_clock::now();
        RS_DBG0("fgets..:", std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
        auto rc = pclose(pipe);

        if (rc == EXIT_SUCCESS)
        { // == 0
        }
        else if (rc == EXIT_FAILURE)
        { // EXIT_FAILURE is not used by all programs, maybe needs some adaptation.
        }

        result = std::regex_replace(result, std::regex("\\r\\n|\\r|\\n"), "");
        return result;
    }
    std::optional<std::string> optMergeState(std::string arguments)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "sleep 1 && echo '" + arguments + "'";
        auto pipe = popen(cmd.c_str(), "r");

        if (!pipe)
        {
            RS_ERR("error opening pipe");

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
            RS_ERR("eror on merge");
            return {};
        }
        result = std::regex_replace(result, std::regex("\\r\\n|\\r|\\n"), "");
        return result;
    }

    /*
    /// @brief error_condition ... > es como error code pero crossplatform
    /// error_code ... es dependiente de plataforma
    /// @param arguments
    /// @return
    tl::expected<std::string, std::error_condition> expMergestate(std::string arguments, bool willFail)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::string cmd = "echo '" + arguments + "'";
        auto pipe = popen(cmd.c_str(), "r");

        if (!pipe || willFail)
        {
            return tl::unexpected<std::error_condition>{make_error_condition(SharedStateErrorCode::OpenPipeError)};
        }
        while (!feof(pipe))
        {
            if (fgets(buffer.data(), 128, pipe) != nullptr)
                result += buffer.data();
        }

        auto rc = pclose(pipe);

        if (rc == EXIT_FAILURE)
        {
            return tl::unexpected<std::error_condition>(make_error_condition(SharedStateErrorCode::OpenPipeError));
        }
        result = std::regex_replace(result, std::regex("\\r\\n|\\r|\\n"), "");
        return result;
    }*/

}
