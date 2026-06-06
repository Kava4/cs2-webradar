#include "pch.hpp"

static void resolve_game_rules_proxy()
{
	if (i::m_game_rules_proxy)
		return;

	static auto s_next_attempt = std::chrono::steady_clock::time_point{};
	const auto now = std::chrono::steady_clock::now();
	if (now < s_next_attempt)
		return;
	s_next_attempt = now + std::chrono::seconds(1);

	for (int32_t idx = 1; idx < 512; idx++)
	{
		const auto entity = i::m_game_entity_system->get(idx);
		if (!entity)
			continue;

		if (entity->get_schema_class_name() == "C_CSGameRulesProxy")
		{
			i::m_game_rules_proxy = reinterpret_cast<c_cs_game_rules_proxy*>(entity);
			return;
		}
	}
}

void f::run()
{
	m_data = nlohmann::json{};
	m_player_data = nlohmann::json{};

	resolve_game_rules_proxy();
	f::roundinfo::get_round_info();
	get_map();

	if (!sdk::m_local_controller)
		return;

	const auto local_team = sdk::m_local_controller->m_iTeamNum();
	if (local_team == e_team::none || local_team == e_team::spec)
		return;

	m_data["m_local_team"] = local_team;

	get_player_info();
	compute_team_stats();
	f::grenades::get_grenade_info();
}

void f::get_map()
{
	static bool s_reload_sent = false;

	const auto map_name = i::m_global_vars->m_map_name();
	if (map_name.empty() || map_name.find("<empty>") != std::string::npos)
	{
		if (!s_reload_sent)
		{
			utils::send_reload();
			s_reload_sent = true;
		}

		m_data["m_map"] = "invalid";

		LOG_WARNING("failed to get map name! updating m_global_vars");
		i::m_global_vars = m_memory->read_t<c_global_vars*>(
			m_memory->find_pattern(CLIENT_DLL, GET_GLOBAL_VARS)->rip().as<c_global_vars*>());
		i::m_game_rules_proxy = nullptr;
		return;
	}

	s_reload_sent = false;
	m_data["m_map"] = map_name;
}

void f::get_player_info()
{
	m_data["m_players"] = nlohmann::json::array();

	const auto highest_idx = 1024;
	for (int32_t idx = 0; idx < highest_idx; idx++)
	{
		const auto entity = i::m_game_entity_system->get(idx);
		if (!entity)
			continue;

		const auto entity_handle = entity->get_ref_e_handle();
		if (!entity_handle.is_valid())
			continue;

		const auto class_name = entity->get_schema_class_name();
		if (class_name.empty())
			continue;

		const auto hashed_class_name = fnv1a::hash(class_name);

		if (hashed_class_name == fnv1a::hash("CCSPlayerController"))
		{
			const auto player = i::m_game_entity_system->get<c_cs_player_controller*>(entity_handle);
			if (!player)
				continue;

			const auto player_pawn = player->get_player_pawn();
			if (!player_pawn)
				continue;

			if (!f::players::get_data(idx, player, player_pawn))
				continue;

			f::players::get_weapons(player_pawn);
			f::players::get_active_weapon(player_pawn);

			m_player_data["m_is_local"] = (player == sdk::m_local_controller);

			m_data["m_players"].push_back(m_player_data);
		}
		else if (hashed_class_name == fnv1a::hash("C_C4"))
		{
			f::bomb::get_carried_bomb(entity);
		}
		else if (hashed_class_name == fnv1a::hash("C_PlantedC4"))
		{
			f::bomb::get_planted_bomb(reinterpret_cast<c_planted_c4*>(entity));
		}
	}
}

void f::compute_team_stats()
{
	int32_t t_alive = 0, ct_alive = 0;
	int32_t t_money = 0, ct_money = 0;

	for (const auto& player : m_data["m_players"])
	{
		const auto team = player.value("m_team", 0);
		const auto dead = player.value("m_is_dead", false);
		const auto money = player.value("m_money", 0);

		if (team == static_cast<int>(e_team::t))
		{
			if (!dead) t_alive++;
			t_money += money;
		}
		else if (team == static_cast<int>(e_team::ct))
		{
			if (!dead) ct_alive++;
			ct_money += money;
		}
	}

	m_data["m_alive_count"] = { {"t", t_alive}, {"ct", ct_alive} };
	m_data["m_team_economy"] = { {"t", t_money}, {"ct", ct_money} };
}
