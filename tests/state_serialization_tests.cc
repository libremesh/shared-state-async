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

/* Serialization of the types that travel on the wire.
 *
 * These are the shapes every peer must agree on, so they are worth
 * pinning in-process: a black-box test can only observe that a sync
 * produced no changes, which is the same thing it observes when a
 * neighbour genuinely has nothing to say. */

#include "doctest/doctest.h"

#include "sharedstate.hh"

#include <serialiser/rsserializer.h>
#include <serialiser/rstypeserializer.h>
#include <util/rsjson.h>

#include <map>
#include <string>
#include <type_traits>

namespace
{

/// Serialize a value the way the wire path does, returning the JSON.
template<typename T> RsJson toJson(T& value, const char* memberName)
{
	RsGenericSerializer::SerializeJob j(RsGenericSerializer::TO_JSON);
	RsGenericSerializer::SerializeContext ctx;
	RsTypeSerializer::serial_process(j, ctx, value, memberName);
	return std::move(ctx.mJson);
}

/// Deserialize, reporting whether the serializer was happy.
template<typename T> bool fromJson(
        RsJson& json, T& value, const char* memberName )
{
	RsGenericSerializer::SerializeJob j(RsGenericSerializer::FROM_JSON);
	RsGenericSerializer::SerializeContext ctx;
	ctx.mJson.CopyFrom(json, ctx.mJson.GetAllocator());
	RsTypeSerializer::serial_process(j, ctx, value, memberName);
	return ctx.mOk;
}

SharedState::StateEntry makeEntry(
        const std::string& author, int64_t ttl, const char* dataJson )
{
	SharedState::StateEntry e;
	e.mAuthor = author;
	e.mTtl = std::chrono::seconds(ttl);
	e.mData.Parse(dataJson);
	return e;
}

} // namespace

TEST_CASE("StateEntry survives a serialization round trip")
{
	auto original = makeEntry("LiMe-abc123", 2401, R"({"hostname":"node-a"})");

	RsJson json = toJson(original, "entry");

	SharedState::StateEntry restored;
	REQUIRE(fromJson(json, restored, "entry"));

	CHECK(restored.mAuthor == original.mAuthor);
	CHECK(restored.mTtl.count() == original.mTtl.count());
	REQUIRE(restored.mData.IsObject());
	REQUIRE(restored.mData.HasMember("hostname"));
	CHECK(std::string(restored.mData["hostname"].GetString()) == "node-a");
}

TEST_CASE("StateEntry keeps its member names on the wire")
{
	/* Peers identify members by name, so renaming one silently breaks
	 * interoperability with every deployed node. Pin the names. */
	auto entry = makeEntry("author-x", 60, R"({"k":1})");
	RsJson json = toJson(entry, "entry");

	REQUIRE(json.HasMember("entry"));
	const auto& v = json["entry"];
	CHECK(v.HasMember("mAuthor"));
	CHECK(v.HasMember("mTtl"));
	CHECK(v.HasMember("mData"));
}

/// Parse literal JSON as if it had arrived from a peer.
RsJson parse(const char* json)
{
	RsJson doc;
	doc.Parse(json);
	REQUIRE_FALSE(doc.HasParseError());
	return doc;
}

TEST_CASE("an entry in the currently deployed shape is accepted")
{
	/* This is the literal payload a deployed node puts on the wire: three
	 * members, no more. Freezing it as a fixture rather than round-tripping
	 * whatever this build happens to emit is the point — a round trip
	 * agrees with itself no matter how the format drifts.
	 *
	 * If a member is ever added to StateEntry, this case is what tells you
	 * whether nodes that do not emit it yet can still be understood. */
	RsJson wire = parse(R"({"entry":{
	        "mAuthor":"LiMe-abc123",
	        "mTtl":{"xint64":2401,"xstr64":"2401"},
	        "mData":{"hostname":"node-a"}}})");

	SharedState::StateEntry entry;
	REQUIRE(fromJson(wire, entry, "entry"));

	CHECK(entry.mAuthor == "LiMe-abc123");
	CHECK(entry.mTtl.count() == 2401);
	REQUIRE(entry.mData.HasMember("hostname"));
	CHECK(std::string(entry.mData["hostname"].GetString()) == "node-a");
}

TEST_CASE("a slice in the currently deployed shape is accepted whole")
{
	RsJson wire = parse(R"({"stateSlice":[
	    {"key":"key-a","value":{"mAuthor":"node-a",
	     "mTtl":{"xint64":100,"xstr64":"100"},"mData":{"v":1}}},
	    {"key":"key-b","value":{"mAuthor":"node-b",
	     "mTtl":{"xint64":200,"xstr64":"200"},"mData":{"v":2}}}]})");

	std::map<SharedState::StateKey, SharedState::StateEntry> slice;
	REQUIRE(fromJson(wire, slice, "stateSlice"));

	REQUIRE(slice.size() == 2);
	CHECK(slice["key-a"].mAuthor == "node-a");
	CHECK(slice["key-b"].mTtl.count() == 200);
}

TEST_CASE("deserialization stops at the first unreadable entry")
{
	/* Where the damage lands when an entry cannot be read: parsing stops
	 * there. Entries earlier in the map survive and everything after is
	 * lost. The serialization context records the failure, but
	 * NetworkMessage::toStateSlice() returns void and discards ctx.mOk,
	 * and neither of its call sites checks it — so a node merges the
	 * surviving prefix without ever learning that the rest was dropped.
	 *
	 * The practical consequence is easy to underestimate. If a member is
	 * added and required on read, a slice from a peer that does not emit
	 * it yet has every entry in an incompatible legacy shape, so the very
	 * first one fails: that peer's neighbour learns nothing at all from
	 * it — not a degraded view, an empty one. Keys are ordered here to
	 * make the boundary visible; std::map iterates in key order. */
	RsJson wire = parse(R"({"stateSlice":[
	    {"key":"a-before","value":{"mAuthor":"node-a",
	     "mTtl":{"xint64":100,"xstr64":"100"},"mData":{"v":1}}},
	    {"key":"b-unreadable","value":{"mAuthor":"node-b",
	     "mData":{"v":2}}},
	    {"key":"c-after","value":{"mAuthor":"node-c",
	     "mTtl":{"xint64":300,"xstr64":"300"},"mData":{"v":3}}}]})");

	std::map<SharedState::StateKey, SharedState::StateEntry> slice;
	const bool ok = fromJson(wire, slice, "stateSlice");

	CHECK_FALSE(ok);                        // recorded, but discarded
	                                        // by toStateSlice() in production
	CHECK(slice.count("a-before") == 1);    // parsed before the failure
	CHECK(slice.count("b-unreadable") == 0);
	CHECK(slice.count("c-after") == 0);     // never reached
}

TEST_CASE("StateEntry is copy-constructible but not copy-assignable")
{
	/* Pinning a sharp edge rather than a behaviour: `mData` is a
	 * rapidjson Document whose copy assignment is private, so
	 * `map[key] = entry` does not compile while `map.emplace(key, entry)`
	 * does. Worth stating explicitly — the error it produces points deep
	 * into <map> and reads as though the container is at fault. */
	static_assert(std::is_copy_constructible_v<SharedState::StateEntry>);
	static_assert(!std::is_copy_assignable_v<SharedState::StateEntry>);
	CHECK(true);
}

TEST_CASE("DataTypeConf survives a serialization round trip")
{
	/* The on-disk configuration format: a node that cannot read it back
	 * loses every registered data type. */
	SharedState::DataTypeConf conf;
	conf.mName = "wifi_links_info";
	conf.mScope = "community";
	conf.mUpdateInterval = std::chrono::seconds(30);
	conf.mBleachTTL = std::chrono::seconds(2400);

	RsJson json = toJson(conf, "conf");

	/* Freeze the member names: a rename would keep a round trip green
	 * while making every existing configuration file unreadable. */
	REQUIRE(json.HasMember("conf"));
	const auto& c = json["conf"];
	CHECK(c.HasMember("mName"));
	CHECK(c.HasMember("mScope"));
	CHECK(c.HasMember("mUpdateInterval"));
	CHECK(c.HasMember("mBleachTTL"));

	SharedState::DataTypeConf restored;
	REQUIRE(fromJson(json, restored, "conf"));

	CHECK(restored.mName == conf.mName);
	CHECK(restored.mScope == conf.mScope);
	CHECK(restored.mUpdateInterval.count() == conf.mUpdateInterval.count());
	CHECK(restored.mBleachTTL.count() == conf.mBleachTTL.count());
}

TEST_CASE("A map of entries round trips as a whole slice")
{
	/* What a sync actually carries. */
	std::map<SharedState::StateKey, SharedState::StateEntry> slice;
	/* emplace, not operator[]= : StateEntry is copy-constructible but
	 * not copy-assignable (see the test below) */
	slice.emplace("key-a", makeEntry("node-a", 100, R"({"v":1})"));
	slice.emplace("key-b", makeEntry("node-b", 200, R"({"v":2})"));

	RsJson json = toJson(slice, "stateSlice");

	std::map<SharedState::StateKey, SharedState::StateEntry> restored;
	REQUIRE(fromJson(json, restored, "stateSlice"));

	REQUIRE(restored.size() == 2);
	CHECK(restored["key-a"].mAuthor == "node-a");
	CHECK(restored["key-b"].mTtl.count() == 200);
}
