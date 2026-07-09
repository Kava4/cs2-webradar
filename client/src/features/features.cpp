#include "pch.hpp"
#include "utils/appdata.hpp"
#include "utils/map_utils.hpp"

static std::string normalize_map_for_radar(const std::string& raw)
{
	if (!map_utils::looks_like_cs2_map(raw))
		return {};

	auto name = appdata::normalize_map_name(raw);
	if (name.size() > 4 && name.ends_with(".bsp"))
		name.resize(name.size() - 4);

	if (!map_utils::looks_like_cs2_map(name))
		return {};

	return name;
}

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
	if (!i::m_game_entity_system || !i::m_global_vars)
		return;

	m_data = nlohmann::json{};
	m_player_data = nlohmann::json{};

	resolve_game_rules_proxy();
	f::roundinfo::get_round_info();
	get_map();

	get_player_info();
	compute_team_stats();
	f::grenades::get_grenade_info();

	if (!sdk::m_local_controller)
		return;

	const auto local_team = sdk::m_local_controller->m_iTeamNum();
	if (local_team == e_team::none || local_team == e_team::spec)
		return;

	m_data["m_local_team"] = local_team;
}

static c_global_vars* refresh_global_vars()
{
	return offset_resolver::resolve_global_vars();
}

static bool is_player_controller(c_base_entity* entity)
{
	const auto class_name = entity->get_schema_class_name();
	if (!class_name.empty())
		return fnv1a::hash(class_name) == fnv1a::hash("CCSPlayerController");

	const auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
	if (!controller->m_hPawn().is_valid())
		return false;

	const auto name = controller->m_sSanitizedPlayerName();
	if (name.empty())
		return false;

	const auto team = controller->m_iTeamNum();
	return team == e_team::t || team == e_team::ct;
}

void f::get_map()
{
	static std::string s_cached_map;

	i::m_global_vars = refresh_global_vars();

	const auto gv_base = i::m_global_vars
		? reinterpret_cast<uintptr_t>(i::m_global_vars)
		: 0u;
	const auto raw_map = map_utils::resolve_map_name(gv_base);

	auto map_name = normalize_map_for_radar(raw_map);
	if (map_name.empty() && !s_cached_map.empty())
	{
		m_data["m_map"] = s_cached_map;
		return;
	}

	if (map_name.empty())
	{
		m_data["m_map"] = s_cached_map.empty() ? "invalid" : s_cached_map;
		return;
	}

	const auto map_changed = (map_name != s_cached_map);
	s_cached_map = map_name;
	m_data["m_map"] = map_name;

	if (map_changed)
	{
		i::m_game_rules_proxy = nullptr;
		utils::send_reload();
	}

	appdata::ensure_map_assets(map_name);
}

void f::get_player_info()
{
	m_data["m_players"] = nlohmann::json::array();

	const auto highest_idx = offset_resolver::highest_entity_index();
	for (int32_t idx = 0; idx < highest_idx; idx++)
	{
		const auto entity = i::m_game_entity_system->get(idx);
		if (!entity)
			continue;

		const auto class_name = entity->get_schema_class_name();
		const auto hashed_class_name = class_name.empty() ? 0u : fnv1a::hash(class_name);

		if (is_player_controller(entity))
		{
			const auto player = reinterpret_cast<c_cs_player_controller*>(entity);

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
