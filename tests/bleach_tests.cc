/*
 * Shared State
 *
 * Copyright (C) 2026  Asociación Civil Altermundi <info@altermundi.net>
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

/* Expiry semantics.
 *
 * `bleach()` decides when data leaves the network, so its edge cases are
 * the difference between an entry disappearing a second early and one
 * that never leaves. It is an ordinary function — no coroutine, no
 * socket — which makes it exactly the kind of thing worth testing in
 * process rather than by driving daemons. */

#include "doctest/doctest.h"

#include "sharedstate.hh"
#include "shared_state_errors.hh"

#include "io_context.hh"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <system_error>

namespace
{

/** `mStates` is protected, so a small subclass is the supported way to
 *  put a known state in front of the real implementation. Nothing here
 *  reimplements behaviour — the code under test is the real `bleach`. */
struct TestableSharedState : SharedState
{
	using SharedState::SharedState;

	void put(const std::string& type, const std::string& key, int64_t ttl)
	{
		StateEntry e;
		e.mAuthor = "test";
		e.mTtl = std::chrono::seconds(ttl);
		e.mData.Parse(R"({"v":1})");
		auto& entries = mStates[type];
		entries.erase(key);              // StateEntry is not assignable
		entries.emplace(key, e);
	}

	bool has(const std::string& type, const std::string& key) const
	{
		auto it = mStates.find(type);
		return it != mStates.end() && it->second.count(key) > 0;
	}

	int64_t ttl(const std::string& type, const std::string& key) const
	{
		return mStates.at(type).at(key).mTtl.count();
	}

	std::size_t size(const std::string& type) const
	{
		auto it = mStates.find(type);
		return it == mStates.end() ? 0 : it->second.size();
	}

	void ensureType(const std::string& type) { mStates[type]; }
};

/** SharedState needs an IOContext reference. bleach() never uses it, but
 *  forming a reference that does not designate an object is undefined
 *  behaviour whether or not anything reads it, so make a real one — an
 *  epoll descriptor is cheap. */
struct Fixture
{
	std::unique_ptr<IOContext> io = IOContext::setup();
	TestableSharedState state;

	Fixture(): state(*io) { REQUIRE(io != nullptr); }
};

constexpr const char* TYPE = "unit_type";

} // namespace

TEST_CASE("bleach removes entries at or below the elapsed time")
{
	Fixture f;
	auto& st = f.state;
	st.put(TYPE, "expires-exactly", 5);
	st.put(TYPE, "expires-under", 3);
	st.put(TYPE, "survives", 6);

	const ssize_t removed = st.bleach(TYPE, std::chrono::seconds(5));

	CHECK(removed == 2);
	CHECK_FALSE(st.has(TYPE, "expires-exactly")); // <= elapsed, so it goes
	CHECK_FALSE(st.has(TYPE, "expires-under"));
	CHECK(st.has(TYPE, "survives"));
}

TEST_CASE("bleach decrements the survivors by exactly the elapsed time")
{
	Fixture f;
	auto& st = f.state;
	st.put(TYPE, "a", 100);
	st.put(TYPE, "b", 10);

	st.bleach(TYPE, std::chrono::seconds(4));

	CHECK(st.ttl(TYPE, "a") == 96);
	CHECK(st.ttl(TYPE, "b") == 6);
}

TEST_CASE("repeated bleaching is equivalent to one longer bleach")
{
	/* The daemon bleaches on a timer and compensates for a late tick by
	 * passing the real elapsed time, so these two paths must agree or
	 * entries live longer on a busy node than on an idle one. */
	Fixture fs, fo;
	auto& stepwise = fs.state;
	auto& atOnce = fo.state;
	for(auto* st : {&stepwise, &atOnce}) st->put(TYPE, "k", 30);

	for(int i = 0; i < 5; ++i) stepwise.bleach(TYPE, std::chrono::seconds(2));
	atOnce.bleach(TYPE, std::chrono::seconds(10));

	CHECK(stepwise.ttl(TYPE, "k") == atOnce.ttl(TYPE, "k"));
}

TEST_CASE("bleach rejects a non-positive interval instead of corrupting TTLs")
{
	Fixture f;
	auto& st = f.state;
	st.put(TYPE, "k", 10);

	std::error_condition ec;
	const ssize_t ret = st.bleach(TYPE, std::chrono::seconds(0), &ec);

	CHECK(ret == -1);
	CHECK(bool(ec));
	CHECK(st.ttl(TYPE, "k") == 10);   // untouched
}

TEST_CASE("bleach reports an unknown data type")
{
	Fixture f;
	auto& st = f.state;

	std::error_condition ec;
	const ssize_t ret = st.bleach("never-registered",
	                              std::chrono::seconds(1), &ec);

	CHECK(ret == -1);
	CHECK(ec == SharedStateErrors::UNKOWN_DATA_TYPE);
}

TEST_CASE("bleaching an empty but registered type is a no-op, not an error")
{
	Fixture f;
	auto& st = f.state;
	st.ensureType(TYPE);

	std::error_condition ec;
	const ssize_t ret = st.bleach(TYPE, std::chrono::seconds(1), &ec);

	CHECK(ret == 0);
	CHECK_FALSE(bool(ec));
	CHECK(st.size(TYPE) == 0);
}
