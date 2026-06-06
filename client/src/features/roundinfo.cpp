// AimSync WebRadar
// roundinfo.cpp - round info + team score feature
#include "pch.hpp"

void f::roundinfo::get_round_info()
{
	// Always emit the key so the frontend never gets a missing field.
	f::m_data["m_round_info"] = nlohmann::json::object();

	if (!i::m_game_rules_proxy)
		return;

	const auto rules = i::m_game_rules_proxy->get_game_rules();
	if (!rules)
		return;

	const auto curtime   = i::m_global_vars->m_curtime();
	const auto is_warmup = rules->m_bWarmupPeriod();
	const auto is_freeze = rules->m_bFreezePeriod();

	// ── Timer ─────────────────────────────────────────────────────────────────
	float remaining = 0.f;
	if (is_warmup)
	{
		// m_fWarmupPeriodEnd = absolute server time warmup ends (offset 0x44)
		const auto warmup_end = rules->m_fWarmupPeriodEnd();
		remaining = std::max(0.f, warmup_end - curtime);
	}
	else if (is_freeze)
	{
		// m_flRestartRoundTime = absolute time freeze ends (offset 0x74)
		const auto freeze_end = rules->m_flRestartRoundTime();
		remaining = std::max(0.f, freeze_end - curtime);
	}
	else
	{
		const auto round_start    = rules->m_fRoundStartTime();
		const auto round_duration = static_cast<float>(rules->m_iRoundTime());
		remaining = std::max(0.f, round_duration - (curtime - round_start) + 1.0f);
	}

	// ── Round number ──────────────────────────────────────────────────────────
	// m_totalRoundsPlayed is 0-based completed rounds → +1 = current round.
	const auto total_played = rules->m_totalRoundsPlayed();
	const auto round_num    = is_warmup ? 0 : (total_played + 1);

	f::m_data["m_round_info"]["m_round_number"]     = round_num;
	f::m_data["m_round_info"]["m_is_freeze_period"] = is_freeze;
	f::m_data["m_round_info"]["m_is_warmup"]        = is_warmup;
	f::m_data["m_round_info"]["m_time_remaining"]   = remaining;
	f::m_data["m_round_info"]["m_game_phase"]       = rules->m_gamePhase();
	f::m_data["m_round_info"]["m_bomb_planted"]     = rules->m_bBombPlanted();
	f::m_data["m_round_info"]["m_bomb_dropped"]     = rules->m_bBombDropped();
	f::m_data["m_round_info"]["m_overtime"]         = rules->m_nOvertimePlaying();

	// ── Team scores ───────────────────────────────────────────────────────────
	// CCSTeam entities live at low indices (typically 1-4).
	bool found_t = false, found_ct = false;

	for (int32_t idx = 1; idx < 128; idx++)
	{
		const auto entity = i::m_game_entity_system->get(idx);
		if (!entity)
			continue;

		// Try both client (C_CSTeam) and server (CCSTeam) class names
		const auto class_name = entity->get_schema_class_name();
		if (class_name != "C_CSTeam" && class_name != "CCSTeam")
			continue;

		const auto team     = reinterpret_cast<c_cs_team*>(entity);
		const auto team_num = team->m_iTeamNum();

		auto write_score = [&](const char* key)
		{
			f::m_data["m_round_info"][key]["m_score"]        = team->m_iScore();
			f::m_data["m_round_info"][key]["m_score_first"]  = team->m_scoreFirstHalf();
			f::m_data["m_round_info"][key]["m_score_second"] = team->m_scoreSecondHalf();
			f::m_data["m_round_info"][key]["m_score_ot"]     = team->m_scoreOvertime();
		};

		if (team_num == e_team::t && !found_t)
		{
			write_score("m_t_score");
			found_t = true;
		}
		else if (team_num == e_team::ct && !found_ct)
		{
			write_score("m_ct_score");
			found_ct = true;
		}

		if (found_t && found_ct)
			break;
	}

	// Emit zero scores if team entities not found yet (very early in load).
	if (!found_t)
	{
		f::m_data["m_round_info"]["m_t_score"]["m_score"]        = 0;
		f::m_data["m_round_info"]["m_t_score"]["m_score_first"]  = 0;
		f::m_data["m_round_info"]["m_t_score"]["m_score_second"] = 0;
		f::m_data["m_round_info"]["m_t_score"]["m_score_ot"]     = 0;
	}
	if (!found_ct)
	{
		f::m_data["m_round_info"]["m_ct_score"]["m_score"]        = 0;
		f::m_data["m_round_info"]["m_ct_score"]["m_score_first"]  = 0;
		f::m_data["m_round_info"]["m_ct_score"]["m_score_second"] = 0;
		f::m_data["m_round_info"]["m_ct_score"]["m_score_ot"]     = 0;
	}
}