#pragma once

#include <system_error>

enum class SharedStateErrorCode
{
  // no 0
  OpenPipeError = 10, // requested airport doesn't exist
  ProtocolViolation    = 30, // e.g., bad XML
  ConnectionError,           // could not connect to server
  ResourceError,             // service run short of resources
  Timeout                   // did not respond in time
};

// Overload of standard library make_error_code for FlightsErrorCode
std::error_code make_error_code(SharedStateErrorCode);

namespace std
{
    template<>
    struct is_error_code_enum<SharedStateErrorCode> : true_type {};
}
