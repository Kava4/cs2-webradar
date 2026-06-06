#include "pch.hpp"

// ---------------------------------------------------------------------------
// Helper: resolve the thrower entity index from a grenade's m_hThrower handle.
// Returns -1 when unresolvable.
// ---------------------------------------------------------------------------
static int32_t get_thrower_idx(c_base_grenade* grenade)
{
	const auto handle = grenade->m_hThrower();
	if (!handle.is_valid())
		return -1;

	return handle.get_entry_idx() & ENT_ENTRY_MASK;
}

// ---------------------------------------------------------------------------
// Helper: build a base JSON object shared by every grenade type.
// ---------------------------------------------------------------------------
static nlohmann::json make_grenade_entry(
	const std::string& type,
	e_team             team,
	int32_t            thrower_idx,
	const f_vector&    pos)
{
	return nlohmann::json{
		{ "type",        type },
		{ "team",        static_cast<uint8_t>(team) },
		{ "thrower_idx", thrower_idx },
		{ "x",           pos.m_x },
		{ "y",           pos.m_y }
	};
}

// ---------------------------------------------------------------------------
// f::grenades::get_grenade_info()
// ---------------------------------------------------------------------------
void f::grenades::get_grenade_info()
{
	f::m_data["m_grenades"] = nlohmann::json::array();

	constexpr int32_t highest_idx = 1024;
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

		const auto hashed = fnv1a::hash(class_name);

		// ------------------------------------------------------------------
		// Smoke
		// ------------------------------------------------------------------
		if (hashed == fnv1a::hash("C_SmokeGrenadeProjectile"))
		{
			const auto smoke = reinterpret_cast<c_smoke_grenade_projectile*>(entity);

			const auto pos = smoke->get_scene_origin();
			if (pos.is_zero())
				continue;

			auto entry = make_grenade_entry(
				"smoke",
				smoke->m_iTeamNum(),
				get_thrower_idx(smoke),
				pos);

			entry["m_did_smoke_effect"]     = smoke->m_bDidSmokeEffect();
			entry["m_smoke_effect_spawned"] = smoke->m_bSmokeEffectSpawned();

			// Once deployed, snap position to the fixed detonation point
			if (smoke->m_bDidSmokeEffect())
			{
				const auto det_pos = smoke->m_vSmokeDetonationPos();
				if (!det_pos.is_zero())
				{
					entry["x"] = det_pos.m_x;
					entry["y"] = det_pos.m_y;
				}
			}

			f::m_data["m_grenades"].push_back(std::move(entry));
		}

		// ------------------------------------------------------------------
		// Flashbang
		// ------------------------------------------------------------------
		else if (hashed == fnv1a::hash("C_FlashbangProjectile"))
		{
			const auto flash = reinterpret_cast<c_base_cs_grenade_projectile*>(entity);

			const auto pos = flash->get_scene_origin();
			if (pos.is_zero())
				continue;

			if (flash->m_bExplodeEffectBegan())
				continue;

			f::m_data["m_grenades"].push_back(
				make_grenade_entry(
					"flashbang",
					flash->m_iTeamNum(),
					get_thrower_idx(flash),
					pos));
		}

		// ------------------------------------------------------------------
		// HE Grenade
		// ------------------------------------------------------------------
		else if (hashed == fnv1a::hash("C_HEGrenadeProjectile"))
		{
			const auto he = reinterpret_cast<c_base_cs_grenade_projectile*>(entity);

			const auto pos = he->get_scene_origin();
			if (pos.is_zero())
				continue;

			if (he->m_bExplodeEffectBegan())
				continue;

			f::m_data["m_grenades"].push_back(
				make_grenade_entry(
					"he",
					he->m_iTeamNum(),
					get_thrower_idx(he),
					pos));
		}

		// ------------------------------------------------------------------
		// Molotov / Incendiary
		// ------------------------------------------------------------------
		else if (hashed == fnv1a::hash("C_MolotovProjectile"))
		{
			const auto molotov = reinterpret_cast<c_molotov_projectile*>(entity);

			const auto pos = molotov->get_scene_origin();
			if (pos.is_zero())
				continue;

			if (molotov->m_bExplodeEffectBegan())
				continue;

			const std::string type = molotov->m_bIsIncGrenade() ? "incendiary" : "molotov";

			f::m_data["m_grenades"].push_back(
				make_grenade_entry(
					type,
					molotov->m_iTeamNum(),
					get_thrower_idx(molotov),
					pos));
		}

		// ------------------------------------------------------------------
		// Decoy
		// ------------------------------------------------------------------
		else if (hashed == fnv1a::hash("C_DecoyProjectile"))
		{
			const auto decoy = reinterpret_cast<c_decoy_projectile*>(entity);

			const auto pos = decoy->get_scene_origin();
			if (pos.is_zero())
				continue;

			auto entry = make_grenade_entry(
				"decoy",
				decoy->m_iTeamNum(),
				get_thrower_idx(decoy),
				pos);

			entry["m_is_active"] = (decoy->m_nDecoyShotTick() != 0);

			f::m_data["m_grenades"].push_back(std::move(entry));
		}

		// ------------------------------------------------------------------
		// Inferno (fire entity spawned when molotov/incendiary lands)
		// ------------------------------------------------------------------
		else if (hashed == fnv1a::hash("C_Inferno"))
		{
			const auto inferno = reinterpret_cast<c_inferno*>(entity);

			if (inferno->m_bInPostEffectTime())
				continue;

			const auto fire_count = inferno->m_fireCount();
			if (fire_count <= 0)
				continue;

			const auto pos = inferno->get_scene_origin();
			if (pos.is_zero())
				continue;

			auto entry = make_grenade_entry(
				"inferno",
				inferno->m_iTeamNum(),
				-1,
				pos);

			entry["m_fire_count"]   = fire_count;
			entry["m_inferno_type"] = inferno->m_nInfernoType();

			f::m_data["m_grenades"].push_back(std::move(entry));
		}
	}
}