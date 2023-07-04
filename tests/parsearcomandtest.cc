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

#include "doctest/doctest.h"
#include "sharedstate.hh"
#include "shared_state_error_code.hh"
#include "debug/rsdebuglevel2.h"

// Tests that don't naturally fit in the headers/.cpp files directly
// can be placed in a tests/*.cpp file. Integration tests are a good example.

TEST_CASE("return command")
{
  std::string completo = "comando\ndatos";
  std::string comando = "";
  comando = SharedState::extractCommand(completo);
  CHECK(comando == "comando");
  CHECK(completo == "datos");
}

TEST_CASE("return command overload")
{
  std::string completo = "comando\ndatos";
  std::string comando = "";
  SharedState::extractCommand(completo,comando);
  CHECK(comando == "comando");
  CHECK(completo == "datos");
}

TEST_CASE("return empty string")
{
  std::string completo = "comandodatos";
  std::string comando = "";
  comando = SharedState::extractCommand(completo);
  CHECK(comando == "");
  CHECK(completo == "comandodatos");
}

TEST_CASE("return empty string")
{
  std::string completo = "comandodatos";
  std::string comando = "";
  std::error_condition ec = SharedState::extractCommand(completo,comando);
  CHECK(comando == "");
  CHECK(completo == "comandodatos");
  CHECK(ec == make_error_condition(SharedState::SharedStateErrorCode::NoCommand));
}