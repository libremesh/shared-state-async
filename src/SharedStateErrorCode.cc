#include "SharedStateErrorCode.hh"

namespace
{

struct FlightsErrorCategory : std::error_category
{
    const char* name() const noexcept override;
    std::string message(int ev) const override;
};

const char* FlightsErrorCategory::name() const noexcept
{ return "flights"; }

/**
 * @brief Message associated with the error
 * 
 * @param ev 
 * @return std::string 
 */
std::string FlightsErrorCategory::message(int ev) const
{
    switch (static_cast<SharedStateErrorCode>(ev))
    {
        case SharedStateErrorCode::OpenPipeError:
            return "unable to open filesystem pipe";
        case SharedStateErrorCode::ProtocolViolation:
            return "received malformed request";
        case SharedStateErrorCode::ConnectionError:
            return "could not connect to server";
        case SharedStateErrorCode::ResourceError:
            return "insufficient resources";
        case SharedStateErrorCode::Timeout:
            return "processing timed out";
        default:
            return "(unrecognized error)";
    }
}

// Instantiation of the FlightsErrorCategory used by make_error_code
const FlightsErrorCategory theFlightsErrorCategory {};

}

// Definition of make_error_code MUST BE declared in FlightsErrorCode.h
std::error_condition make_error_condition(SharedStateErrorCode e) noexcept
{ return {static_cast<int>(e), theFlightsErrorCategory}; }
