#pragma once

#include <string>
#include <cctype>
#include <vector>

#include "sdk/dump_offsets.hpp"

namespace map_utils
{
	inline constexpr std::ptrdiff_t k_game_types_map_name = 0x120;

	inline bool is_printable_ascii(const std::string& value)
	{
		if (value.empty())
			return false;

		for (const unsigned char ch : value)
		{
			if (ch < 0x21 || ch > 0x7E)
				return false;
		}

		return true;
	}

	inline bool looks_like_cs2_map(const std::string& value)
	{
		if (!is_printable_ascii(value))
			return false;

		if (value.find("<empty>") != std::string::npos)
			return false;

		const std::string prefixes[] = { "de_", "cs_", "ar_", "gd_", "dz_", "fm_" };
		for (const auto& prefix : prefixes)
		{
			if (value.starts_with(prefix))
				return true;
		}

		if (value.find("/de_") != std::string::npos
			|| value.find("/cs_") != std::string::npos
			|| value.find("/ar_") != std::string::npos
			|| value.find("maps/") != std::string::npos)
			return true;

		const auto slash = value.find_last_of("/\\");
		const auto leaf = slash == std::string::npos ? value : value.substr(slash + 1);
		for (const auto& prefix : prefixes)
		{
			if (leaf.starts_with(prefix))
				return true;
		}

		return false;
	}

	inline std::string coerce_map_name(const std::string& raw)
	{
		if (looks_like_cs2_map(raw))
			return raw;

		const std::string prefixes[] = { "de_", "cs_", "ar_", "gd_", "dz_", "fm_" };
		for (const auto& prefix : prefixes)
		{
			const auto pos = raw.find(prefix);
			if (pos == std::string::npos)
				continue;

			auto end = raw.find_first_of(" \t\r\n/\\", pos);
			if (end == std::string::npos)
				end = raw.size();

			const auto candidate = raw.substr(pos, end - pos);
			if (looks_like_cs2_map(candidate))
				return candidate;
		}

		return {};
	}

	inline std::string read_utl_string(const uintptr_t field_address)
	{
		if (!field_address)
			return {};

		const auto str_ptr = m_memory->read_t<uintptr_t>(field_address);
		if (!str_ptr || str_ptr <= 0x10000)
			return {};

		return m_memory->read_t<std::string>(str_ptr);
	}

	inline std::string read_remote_cstring(const uintptr_t address, const size_t max_len = 128)
	{
		if (!address || address <= 0x10000)
			return {};

		std::vector<char> buffer(max_len + 1, '\0');
		if (!m_memory->read_t(address, buffer.data(), max_len))
			return {};

		buffer[max_len] = '\0';
		return std::string(buffer.data());
	}

	inline std::string read_map_field(const uintptr_t field_address)
	{
		if (!field_address)
			return {};

		if (const auto coerced = coerce_map_name(read_utl_string(field_address)); !coerced.empty())
			return coerced;

		if (const auto coerced = coerce_map_name(read_remote_cstring(field_address, 64)); !coerced.empty())
			return coerced;

		const auto raw_ptr = m_memory->read_t<uintptr_t>(field_address);
		if (!raw_ptr || raw_ptr <= 0x10000)
			return {};

		for (const auto delta : { -2, 0, 2, -4, 4 })
		{
			if (const auto coerced = coerce_map_name(read_remote_cstring(raw_ptr + delta)); !coerced.empty())
				return coerced;
		}

		return {};
	}

	inline std::string scan_for_map(const uintptr_t base, const std::ptrdiff_t start, const std::ptrdiff_t end)
	{
		if (!base)
			return {};

		for (std::ptrdiff_t offset = start; offset <= end; offset += 8)
		{
			if (const auto name = read_map_field(base + offset); !name.empty())
				return name;
		}

		return {};
	}

	inline std::string read_global_vars_map(const uintptr_t base)
	{
		if (!base)
			return {};

		// Original cs2_webradar: CUtlString @ 0x188
		if (const auto name = read_map_field(base + 0x188); !name.empty())
			return name;

		// UC/community: pointer @ 0x230, string at ptr-2
		const auto ptr_230 = m_memory->read_t<uintptr_t>(base + 0x230);
		if (ptr_230 > 0x10000)
		{
			for (const auto delta : { -2, 0, 2 })
			{
				if (const auto name = coerce_map_name(read_remote_cstring(ptr_230 + delta)); !name.empty())
					return name;
			}
		}

		constexpr std::ptrdiff_t known_offsets[] = { 0x180, 0x190, 0x228, 0x178, 0x230 };
		for (const auto offset : known_offsets)
		{
			if (const auto name = read_map_field(base + offset); !name.empty())
				return name;
		}

		return scan_for_map(base, 0x140, 0x280);
	}

	inline std::string read_map_from_matchmaking()
	{
		const auto [module_base, module_size] = m_memory->get_module_info("matchmaking.dll");
		if (!module_base.has_value())
			return {};

		const auto types_direct = *module_base + cs2_dumper::offsets::matchmaking_dll::dwGameTypes;

		// Primary: CGameTypes struct is embedded at dwGameTypes (not a pointer).
		if (const auto name = read_map_field(types_direct + k_game_types_map_name); !name.empty())
			return name;

		if (const auto name = scan_for_map(types_direct, 0xE0, 0x1A0); !name.empty())
			return name;

		// Fallback: pointer stored at dwGameTypes (older builds).
		const auto types_ptr = m_memory->read_t<uintptr_t>(types_direct);
		if (!types_ptr)
			return {};

		if (const auto name = read_map_field(types_ptr + k_game_types_map_name); !name.empty())
			return name;

		return scan_for_map(types_ptr, 0xE0, 0x1A0);
	}

	inline std::string read_map_from_engine()
	{
		const auto [module_base, module_size] = m_memory->get_module_info("engine2.dll");
		if (!module_base.has_value())
			return {};

		const auto net_client = m_memory->read_t<uintptr_t>(
			*module_base + cs2_dumper::offsets::engine2_dll::dwNetworkGameClient);
		if (!net_client)
			return {};

		return scan_for_map(net_client, 0x1C0, 0x320);
	}

	inline std::string resolve_map_name(const uintptr_t global_vars_base)
	{
		if (const auto mm = read_map_from_matchmaking(); !mm.empty())
			return mm;

		if (global_vars_base)
		{
			if (const auto gv = read_global_vars_map(global_vars_base); !gv.empty())
				return gv;
		}

		if (const auto eng = read_map_from_engine(); !eng.empty())
			return eng;

		return {};
	}
}
