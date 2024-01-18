/*
 * Shared State
 *
 * Copyright (C) 2024  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (C) 2024  Asociación Civil Altermundi <info@altermundi.net>
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

#include <util/rsdebug.h>

/**
 * @brief Shared State specific error conditions
 */
enum class SharedStateErrors : int32_t
{
	UNKOWN_DATA_TYPE = 2000
};

struct SharedStateErrorsCategory: std::error_category
{
	const char* name() const noexcept override
	{ return "SharedState"; }

	std::string message(int ev) const override
	{
		switch (static_cast<SharedStateErrors>(ev))
		{
		case SharedStateErrors::UNKOWN_DATA_TYPE:
			return "Unknown data type";
		default:
			return rsErrorNotInCategory(ev, name());
		}
	}

	const static SharedStateErrorsCategory instance;
};

/** Provide conversion to std::error_condition, must be in same namespace of
 *  the errors enum */
inline std::error_condition make_error_condition(SharedStateErrors e) noexcept
{
	return std::error_condition(
	            static_cast<int>(e), SharedStateErrorsCategory::instance );
};

namespace std
{
/** Register RsJsonApiErrorNum as an error condition enum, must be in std
 * namespace */
template<> struct is_error_condition_enum<SharedStateErrors> : true_type {};
}

