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
#pragma once

#include <system_error>
namespace SharedState
{

  enum class SharedStateErrorCode
  {
    // no 0
    OpenPipeError = 10,     //
    ProtocolViolation = 30, // e.g., bad XML
    ConnectionError,        // could not connect to server
    ResourceError,          // service run short of resources
    Timeout                 // did not respond in time
  };

  // Overload of standard library make_error_code
  //+++  must be in the same namespace of the enum +++
  std::error_condition make_error_condition(SharedStateErrorCode) noexcept;
}
namespace std
{
  template <>
  struct is_error_condition_enum<SharedState::SharedStateErrorCode> : true_type
  {
  };
}
