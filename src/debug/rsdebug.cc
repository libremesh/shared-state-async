/*******************************************************************************
 * libretroshare/src/util: rsdebug.cc                                          *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright (C) 2004-2008  Robert Fernie <retroshare@lunamutt.com>            *
 * Copyright (C) 2020-2021  Gioacchino Mazzurco <gio@eigenlab.org>             *
 * Copyright (C) 2020-2021  Asociación Civil Altermundi <info@altermundi.net>  *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include <iomanip>

#include "debug/rsdebug.h"

std::ostream &operator<<(std::ostream &out, const std::error_condition &err)
{
	return out << " error: " << err.value() << " " << err.message()
			   << " category: " << err.category().name();
}

std::string rsErrorNotInCategory(int errNum, const std::string &categoryName)
{
	return "Error message for error: " + std::to_string(errNum) +
		   " not available in category: " + categoryName;
}

std::error_condition rs_errno_to_condition(int errno_code)
{
	return std::make_error_condition(static_cast<std::errc>(errno_code));
}

std::ostream &hex_dump(std::ostream &os, const uint8_t*buffer,
					   std::size_t bufsize, bool showPrintableChars /*= true*/)
{
	if (buffer == nullptr)
	{
		return os;
	}
	auto oldFormat = os.flags();
	auto oldFillChar = os.fill();
	constexpr std::size_t maxline{8};
	// create a place to store text version of string
	char renderString[maxline + 1];
	char *rsptr{renderString};
	// convenience cast
	const unsigned char *buf{reinterpret_cast<const unsigned char *>(buffer)};

	for (std::size_t linecount = maxline; bufsize; --bufsize, ++buf)
	{
		os << std::setw(2) << std::setfill('0') << std::hex
		   << static_cast<unsigned>(*buf) << ' ';
		*rsptr++ = std::isprint(*buf) ? *buf : '.';
		if (--linecount == 0)
		{
			*rsptr++ = '\0'; // terminate string
			if (showPrintableChars)
			{
				os << " | " << renderString;
			}
			os << '\n';
			rsptr = renderString;
			linecount = std::min(maxline, bufsize);
		}
	}
	// emit newline if we haven't already
	if (rsptr != renderString)
	{
		if (showPrintableChars)
		{
			for (*rsptr++ = '\0'; rsptr != &renderString[maxline + 1]; ++rsptr)
			{
				os << "   ";
			}
			os << " | " << renderString;
		}
		os << '\n';
	}

	os.fill(oldFillChar);
	os.flags(oldFormat);
	return os;
}