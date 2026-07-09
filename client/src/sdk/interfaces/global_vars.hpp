#pragma once

#include "utils/map_utils.hpp"

class c_global_vars
{
public:
	SCHEMA_ADD_OFFSET(float, m_curtime, 0x30);

	std::string m_map_name() const
	{
		return map_utils::read_global_vars_map(reinterpret_cast<uintptr_t>(this));
	}
};
