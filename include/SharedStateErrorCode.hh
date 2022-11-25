#pragma once

#include <system_error>
namespace SharedState
{

enum class SharedStateErrorCode
{
  // no 0
  OpenPipeError = 10, // requested airport doesn't exist
  ProtocolViolation    = 30, // e.g., bad XML
  ConnectionError,           // could not connect to server
  ResourceError,             // service run short of resources
  Timeout                   // did not respond in time
};

// Overload of standard library make_error_code 
//+++  must be in the same namespace of the enum +++
std::error_condition make_error_condition(SharedStateErrorCode) noexcept;
}
namespace std
{
    template<>
    struct is_error_condition_enum<SharedState::SharedStateErrorCode> : true_type {};
}
