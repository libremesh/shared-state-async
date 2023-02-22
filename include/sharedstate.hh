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
    // std::string mergestate(std::string arguments);
    // std::string mergestate(std::string arguments);//, Socket* s);
    std::string mergestate(std::string arguments);
    std::optional<std::string> optMergeState(std::string arguments);
    tl::expected<std::string, std::error_condition> expMergestate(std::string arguments, bool willFail = false);

}