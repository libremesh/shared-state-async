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

#include <util/rsdebuglevel2.h>

void print_debug_message()
{
  std::string original = "mensaje a verificar";
  RS_DBG0(" cout replacement " , "variable2");
  RS_DBG0("Hello 0 ", "my debug ", original, " message " , "variable2");
  RS_DBG1("Hello 1 ", "my debug ", original , " message " , "variable2");
  RS_DBG2("Hello 2 ", "my debug ", original , " message " , "variable2");
  //since "rsdebuglevel2.h" is included the following lines wont print anything
  RS_DBG3("Hello 3 ", "my debug ", original , " message " , "variable2");
  RS_DBG4("Hello 4 ", "my debug ", original , " message " , "variable2");
}

/*
this tests outputs this text 
D 1677074011.647 void print_debug_message() Hello 0 my debug mensaje a verificar message variable2
D 1677074011.647 void print_debug_message() Hello 1 my debug mensaje a verificar message variable2
D 1677074011.647 void print_debug_message() Hello 2 my debug mensaje a verificar message variable2
I 1677074011.647 void DOCTEST_ANON_FUNC_2() this is an information message, with no useful data
W 1677074011.647 void DOCTEST_ANON_FUNC_2() this is a warning, be careful! you should wear a hat
E 1677074011.647 void DOCTEST_ANON_FUNC_2() 2 + 2 = 5 in normal math this shouldn't happen
F 1677074011.647 void DOCTEST_ANON_FUNC_2() THIS IS THE END
*/
TEST_CASE("print different level debug messages")
{
  print_debug_message();
  RS_INFO("this is an information message, with no useful data");
  RS_WARN("this is a warning, be careful! you should wear a hat");
  RS_ERR("2 + 2 = 5 in normal math this shouldn't happen");
  RS_FATAL("THIS IS THE END");
}
