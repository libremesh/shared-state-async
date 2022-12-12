#include "shared_state_error_code.hh"

namespace SharedState
{

    struct SharedStateCategory : std::error_category
    {
        const char *name() const noexcept override;
        std::string message(int ev) const override;
    };

    const char *SharedStateCategory::name() const noexcept
    {
        return "flights";
    }

    /**
     * @brief Message associated with the error
     *
     * @param ev
     * @return std::string
     */
    std::string SharedStateCategory::message(int ev) const
    {
        switch (static_cast<SharedStateErrorCode>(ev))
        {
        case SharedState::SharedStateErrorCode::OpenPipeError:
            return "unable to open filesystem pipe";
        case SharedState::SharedStateErrorCode::ProtocolViolation:
            return "received malformed request";
        case SharedState::SharedStateErrorCode::ConnectionError:
            return "could not connect to server";
        case SharedState::SharedStateErrorCode::ResourceError:
            return "insufficient resources";
        case SharedState::SharedStateErrorCode::Timeout:
            return "processing timed out";
        default:
            return "(unrecognized error)";
        }
    }

    // Instantiation of the category used by make_error_code
    const SharedStateCategory theErrorCategory{};



// Definition of make_error_code MUST BE declared in FlightsErrorCode.h
std::error_condition make_error_condition(SharedState::SharedStateErrorCode e) noexcept
{
    return {static_cast<int>(e), theErrorCategory};
}
}