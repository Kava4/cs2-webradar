#include "pch.hpp"

// ---------------------------------------------------------------------------
// Steam avatar URL cache
// Fetches the full avatar URL from Steam's public XML profile endpoint.
// Uses plain libcurl to match the rest of the project (utils.cpp style).
// Cache is keyed by SteamID64; a zero SteamID returns the default avatar.
// ---------------------------------------------------------------------------
static const std::string DEFAULT_AVATAR_URL =
	"https://avatars.fastly.steamstatic.com/fef49e7fa7e1997310d705b2a6158ff8dc1cdfeb_full.jpg";

static std::unordered_map<uint64_t, std::string> s_avatar_cache;

static size_t avatar_write_cb(void* contents, size_t size, size_t nmemb, std::string* out)
{
	out->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

// Synchronous fetch — called only once per unique SteamID, result is cached.
// Returns empty string on failure so callers can omit the field gracefully.
static std::string fetch_avatar_url(uint64_t steam_id)
{
	if (steam_id == 0)
		return DEFAULT_AVATAR_URL;

	const auto it = s_avatar_cache.find(steam_id);
	if (it != s_avatar_cache.end())
		return it->second;

	// Mark as in-progress with an empty string so concurrent iterations skip it.
	s_avatar_cache[steam_id] = {};

	const auto profile_url = std::format(
		"https://steamcommunity.com/profiles/{}/?xml=1", steam_id);

	std::string response;
	const auto curl = curl_easy_init();
	if (!curl)
		return {};

	curl_easy_setopt(curl, CURLOPT_URL,           profile_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     "libcurl-agent/1.0");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, avatar_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L); // don't block the loop

	const auto res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
	{
		LOG_WARNING("avatar fetch failed for %llu: %s", steam_id, curl_easy_strerror(res));
		return {};
	}

	// Parse <avatarFull>...</avatarFull> (may be wrapped in CDATA)
	const std::string open_tag  = "<avatarFull>";
	const std::string close_tag = "</avatarFull>";

	const auto open_pos = response.find(open_tag);
	if (open_pos == std::string::npos)
		return {};

	const auto close_pos = response.find(close_tag, open_pos + open_tag.size());
	if (close_pos == std::string::npos)
		return {};

	std::string url = response.substr(
		open_pos + open_tag.size(),
		close_pos - (open_pos + open_tag.size()));

	// Trim whitespace
	const auto trim = [](std::string& s)
	{
		const auto first = s.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) { s.clear(); return; }
		const auto last = s.find_last_not_of(" \t\r\n");
		s = s.substr(first, last - first + 1);
	};
	trim(url);

	// Strip CDATA wrapper if present
	const std::string cdata_open  = "<![CDATA[";
	const std::string cdata_close = "]]>";
	if (url.starts_with(cdata_open))
	{
		const auto end_pos = url.find(cdata_close, cdata_open.size());
		if (end_pos != std::string::npos)
			url = url.substr(cdata_open.size(), end_pos - cdata_open.size());
		trim(url);
	}

	s_avatar_cache[steam_id] = url;
	return url;
}

// ---------------------------------------------------------------------------
// f::players::get_data
// ---------------------------------------------------------------------------
bool f::players::get_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn)
{
	const auto health    = player_pawn->m_iHealth();
	const auto is_dead   = health <= 0;
	const auto vec_origin = player->get_vec_origin();
	const auto team      = player->m_iTeamNum();
	const auto steam_id  = player->m_steamID();

	m_player_data["m_idx"]        = idx;
	m_player_data["m_name"]       = player->m_sSanitizedPlayerName();
	m_player_data["m_color"]      = player->get_color();
	m_player_data["m_team"]       = team;
	m_player_data["m_health"]     = health;
	m_player_data["m_is_dead"]    = is_dead;
	m_player_data["m_model_name"] = player_pawn->get_model_name();
	m_player_data["m_steam_id"]   = std::to_string(steam_id);
	m_player_data["m_money"]      = player->m_pInGameMoneyServices()->m_iAccount();
	m_player_data["m_armor"]      = player_pawn->m_ArmorValue();
	m_player_data["m_position"]["x"] = vec_origin.m_x;
	m_player_data["m_position"]["y"] = vec_origin.m_y;
	m_player_data["m_eye_angle"]  = player_pawn->m_angEyeAngles().m_y;
	m_player_data["m_has_helmet"] = player_pawn->m_pItemServices()->m_bHasHelmet();
	m_player_data["m_has_defuser"]= player_pawn->m_pItemServices()->m_bHasDefuser();
	m_player_data["m_weapons"]    = nlohmann::json{};

	m_player_data["m_flash_duration"] = player_pawn->m_flFlashDuration();
	m_player_data["m_flash_alpha"]    = player_pawn->m_flFlashOverlayAlpha();

	m_player_data["m_iPing"] = player->m_iPing();

	// KDA
	{
		const auto ats = player->m_pActionTrackingServices();
		if (ats)
		{
			m_player_data["m_kills"]   = ats->get_kills();
			m_player_data["m_deaths"]  = ats->get_deaths();
			m_player_data["m_assists"] = ats->get_assists();
		}
		else
		{
			m_player_data["m_kills"]   = 0;
			m_player_data["m_deaths"]  = 0;
			m_player_data["m_assists"] = 0;
		}
	}

	// Rank
	const auto inv = player->m_pInventoryServices();
	m_player_data["m_rank"] = inv ? inv->m_rank_id() : 0;

	m_player_data["m_competitive_ranking"] = player->m_iCompetitiveRanking();

	// Avatar URL — fetched once and cached by SteamID
	m_player_data["m_avatar_url"] = fetch_avatar_url(steam_id);

	if (team == e_team::t && !is_dead)
		m_player_data["m_has_bomb"] = m_bomb_idx == (player->m_hPawn().get_entry_idx() & 0xffff);

	return true;
}

// ---------------------------------------------------------------------------
// f::players::get_weapons
// ---------------------------------------------------------------------------
void f::players::get_weapons(c_cs_player_pawn* player_pawn)
{
	const auto weapon_services = player_pawn->m_pWeaponServices();
	if (!weapon_services)
		return;

	const auto my_weapons = weapon_services->m_hMyWeapons();
	if (!my_weapons.m_size)
		return;

	std::set<std::string> utilities_set{};
	std::set<std::string> melee_set{};

	for (size_t idx = 0; idx < my_weapons.m_size; idx++)
	{
		const auto weapon = my_weapons.m_elements->get(idx);
		if (!weapon)
			continue;

		const auto weapon_data = weapon->m_WeaponData();
		if (!weapon_data)
			continue;

		auto weapon_name = weapon_data->m_szName();
		if (weapon_name.empty())
			continue;

		weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);

		const auto weapon_type = weapon_data->m_WeaponType();
		switch (weapon_type)
		{
		case e_weapon_type::submachinegun:
		case e_weapon_type::rifle:
		case e_weapon_type::shotgun:
		case e_weapon_type::sniper_rifle:
		case e_weapon_type::machinegun:
			m_player_data["m_weapons"]["m_primary"] = weapon_name;
			break;
		case e_weapon_type::pistol:
			m_player_data["m_weapons"]["m_secondary"] = weapon_name;
			break;
		case e_weapon_type::knife:
		case e_weapon_type::taser:
			melee_set.insert(weapon_name);
			break;
		case e_weapon_type::grenade:
			utilities_set.insert(weapon_name);
			break;
		default:
			break;
		}
	}

	m_player_data["m_weapons"]["m_melee"]     = std::vector<std::string>(melee_set.begin(),     melee_set.end());
	m_player_data["m_weapons"]["m_utilities"] = std::vector<std::string>(utilities_set.begin(), utilities_set.end());
}

// ---------------------------------------------------------------------------
// f::players::get_active_weapon
// ---------------------------------------------------------------------------
void f::players::get_active_weapon(c_cs_player_pawn* player_pawn)
{
	const auto weapon_services = player_pawn->m_pWeaponServices();
	if (!weapon_services)
		return;

	const auto weapon_handle = weapon_services->m_hActiveWeapon();
	if (!weapon_handle.is_valid())
		return;

	const auto active_weapon = i::m_game_entity_system->get<c_base_player_weapon*>(weapon_handle);
	if (!active_weapon)
		return;

	const auto weapon_data = active_weapon->m_WeaponData();
	if (!weapon_data)
		return;

	auto weapon_name = weapon_data->m_szName();
	if (weapon_name.empty())
		return;

	weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);
	m_player_data["m_weapons"]["m_active"] = weapon_name;

	const auto max_clip = weapon_data->m_iMaxClip1();
	if (max_clip > 0)
	{
		m_player_data["m_weapons"]["m_ammo_clip"]     = active_weapon->m_iClip1();
		m_player_data["m_weapons"]["m_ammo_reserve"]  = active_weapon->get_reserve_ammo();
		m_player_data["m_weapons"]["m_ammo_max_clip"] = max_clip;
	}
}