/*
 * Shared State
 *
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
        return "shared_state";
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



// Definition of make_error_code MUST BE declared in shared_state_error_code.h
std::error_condition make_error_condition(SharedState::SharedStateErrorCode e) noexcept
{
    return {static_cast<int>(e), theErrorCategory};
}
}