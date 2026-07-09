#pragma once

#include <algorithm>

#include "dump_offsets.hpp"
#include "dump_interfaces.hpp"

namespace offset_resolver
{
	inline c_schema_system* resolve_schema_system()
	{
		const auto pattern = m_memory->find_pattern(SCHEMASYSTEM_DLL, GET_SCHEMA_SYSTEM);
		if (pattern.has_value())
			return pattern->rip().as<c_schema_system*>();

		const auto [schema_base, schema_size] = m_memory->get_module_info(SCHEMASYSTEM_DLL);
		if (!schema_base.has_value() || !schema_size.has_value())
			return nullptr;

		// Slot address is used as the schema "this" pointer (not dereferenced).
		return reinterpret_cast<c_schema_system*>(
			*schema_base + cs2_dumper::interfaces::schemasystem_dll::SchemaSystem_001);
	}
	inline std::optional<uintptr_t> client_base()
	{
		const auto [base, size] = m_memory->get_module_info(CLIENT_DLL);
		if (!base.has_value() || !size.has_value())
			return std::nullopt;

		return base;
	}

	template<typename T>
	T read_client_ptr(const std::ptrdiff_t offset)
	{
		const auto base = client_base();
		if (!base.has_value())
			return nullptr;

		return m_memory->read_t<T>(*base + offset);
	}

	inline bool is_plausible_user_ptr(const uintptr_t address)
	{
		// Win64 game modules live in the upper canonical range; reject heap/stack noise.
		return address >= 0x7FF000000000ULL || address < 0x10000;
	}

	inline bool is_plausible_global_vars(c_global_vars* candidate)
	{
		if (!candidate)
			return false;

		const auto base = reinterpret_cast<uintptr_t>(candidate);
		if (base < 0x10000)
			return false;

		// Reject heap pointers (e.g. 0x54C...) mistaken for CGlobalVars during map loads.
		if (base < 0x7FF000000000ULL)
			return false;

		return true;
	}

	inline c_global_vars* resolve_global_vars()
	{
		const auto from_offset = read_client_ptr<c_global_vars*>(
			cs2_dumper::offsets::client_dll::dwGlobalVars);
		if (is_plausible_global_vars(from_offset))
			return from_offset;

		const auto pattern = m_memory->find_pattern(CLIENT_DLL, GET_GLOBAL_VARS);
		if (pattern.has_value())
		{
			const auto from_pattern = m_memory->read_t<c_global_vars*>(
				pattern->rip().as<uintptr_t>());
			if (is_plausible_global_vars(from_pattern))
				return from_pattern;
		}

		// During map loads the struct can briefly look invalid; keep last good pointer.
		return from_offset ? from_offset : nullptr;
	}

	inline int32_t highest_entity_index()
	{
		if (!i::m_game_entity_system)
			return 1024;

		const auto highest = m_memory->read_t<int32_t>(
			reinterpret_cast<uintptr_t>(i::m_game_entity_system)
			+ cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);

		if (highest < 0 || highest > 8192)
			return 1024;

		return std::min(highest + 1, 1024);
	}
}
