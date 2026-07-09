#include "pch.hpp"

bool i::setup()
{
	const auto client_base = offset_resolver::client_base();
	if (!client_base.has_value())
		return false;

	m_schema_system = offset_resolver::resolve_schema_system();

	m_global_vars = offset_resolver::resolve_global_vars();

	m_game_entity_system = offset_resolver::read_client_ptr<c_game_entity_system*>(
		cs2_dumper::offsets::client_dll::dwGameEntitySystem);

	if (!m_game_entity_system)
	{
		const auto entity_list_pattern = m_memory->find_pattern(CLIENT_DLL, GET_ENTITY_LIST);
		if (entity_list_pattern.has_value())
		{
			m_game_entity_system = m_memory->read_t<c_game_entity_system*>(
				entity_list_pattern->rip().as<c_game_entity_system*>());
		}
	}

	return m_schema_system && m_global_vars && m_game_entity_system;
}
