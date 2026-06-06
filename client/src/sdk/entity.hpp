// AimSync WebRadar
// entity.hpp - game entity classes and schemas
// Offsets verified against client_dll.hpp dump 2026-04-23

#pragma once

enum class e_team : uint8_t
{
	none,
	spec,
	t,
	ct
};

enum class e_colors : uint32_t
{
	blue,
	green,
	yellow,
	orange,
	purple,
	white
};

enum class e_weapon_type : uint32_t
{
	knife,
	pistol,
	submachinegun,
	rifle,
	shotgun,
	sniper_rifle,
	machinegun,
	c4,
	taser,
	grenade,
	equipment,
	stackableitem,
	fists,
	breachcharge,
	bumpmine,
	tablet,
	melee,
	shield,
	zone_repulsor,
	unknown
};

enum class e_grenade_type : uint32_t
{
	smoke,
	flashbang,
	he_grenade,
	molotov,
	incendiary,
	decoy,
	inferno,
	unknown
};

// ── CEntityIdentity ───────────────────────────────────────────────────────────
// CEntityIdentity::m_pAttributes  @ 0x48  (first usable pointer after the vtbl)
// We keep the raw offsets that have always worked: pClassInfo at 0x08, Idx at 0x10.
// m_designerName / m_flags are resolved through the schema system, not raw offsets.
class c_entity_identity
{
public:
	SCHEMA_ADD_OFFSET(uintptr_t, m_pClassInfo, 0x08);
	SCHEMA_ADD_OFFSET(uint32_t, m_Idx, 0x10);
	SCHEMA_ADD_FIELD(const char*, m_designerName, "CEntityIdentity->m_designerName");
	SCHEMA_ADD_FIELD(uint32_t, m_flags, "CEntityIdentity->m_flags");

	bool is_valid()
	{
		return m_Idx() != INVALID_EHANDLE_IDX;
	}

	int32_t get_entry_idx()
	{
		if (!is_valid())
			return ENT_ENTRY_MASK;
		return m_Idx() & ENT_ENTRY_MASK;
	}

	int32_t get_serial_number()
	{
		return m_Idx() >> NUM_SERIAL_NUM_SHIFT_BITS;
	}
};

// ── CEntityInstance ───────────────────────────────────────────────────────────
// CEntityInstance::m_pEntity @ 0x10  (dump: CEntityInstance->m_pEntity = 0x10)
class c_entity_instance
{
public:
	SCHEMA_ADD_FIELD(c_entity_identity*, m_pEntity, "CEntityInstance->m_pEntity");

	const c_base_handle get_ref_e_handle();
	const std::string   get_schema_class_name();
};

// ── CGameSceneNode ────────────────────────────────────────────────────────────
// m_vecAbsOrigin @ 0xC8  (dump: CGameSceneNode->m_vecAbsOrigin = 0xC8)
class c_game_scene_node
{
public:
	SCHEMA_ADD_FIELD(f_vector, m_vecAbsOrigin, "CGameSceneNode->m_vecAbsOrigin");
};

// ── C_BaseEntity ──────────────────────────────────────────────────────────────
// m_pGameSceneNode @ 0x330  (dump: C_BaseEntity->m_pGameSceneNode = 0x330)
// m_iHealth        @ 0x34C  (dump: C_BaseEntity->m_iHealth        = 0x34C)
// m_iTeamNum       @ 0x3EB  (dump: C_BaseEntity->m_iTeamNum       = 0x3EB)
// m_hOwnerEntity   @ 0x520  (dump: C_BaseEntity->m_hOwnerEntity   = 0x520)
class c_base_entity : public c_entity_instance
{
public:
	SCHEMA_ADD_FIELD(c_game_scene_node*, m_pGameSceneNode, "C_BaseEntity->m_pGameSceneNode");
	SCHEMA_ADD_FIELD(int32_t, m_iHealth, "C_BaseEntity->m_iHealth");
	SCHEMA_ADD_FIELD(e_team, m_iTeamNum, "C_BaseEntity->m_iTeamNum");
	SCHEMA_ADD_FIELD(c_base_entity*, m_hOwnerEntity, "C_BaseEntity->m_hOwnerEntity");

	const f_vector& get_scene_origin();
};

// ── CPlayer_WeaponServices ────────────────────────────────────────────────────
// m_hActiveWeapon @ 0x60  (dump: CPlayer_WeaponServices->m_hActiveWeapon = 0x60)
// m_hMyWeapons    @ 0x48  (dump: CPlayer_WeaponServices->m_hMyWeapons    = 0x48)
class c_player_weapon_services
{
public:
	SCHEMA_ADD_FIELD(c_base_handle, m_hActiveWeapon, "CPlayer_WeaponServices->m_hActiveWeapon");
	SCHEMA_ADD_FIELD(c_network_utl_vector_base<class c_base_player_weapon>, m_hMyWeapons, "CPlayer_WeaponServices->m_hMyWeapons");
};

// ── CCSPlayer_ItemServices ────────────────────────────────────────────────────
// m_bHasDefuser @ 0x48  (dump: CCSPlayer_ItemServices->m_bHasDefuser = 0x48)
// m_bHasHelmet  @ 0x49  (dump: CCSPlayer_ItemServices->m_bHasHelmet  = 0x49)
// Note: m_bHasHeavyArmor is NOT in client_dll dump for CCSPlayer_ItemServices;
//       keep schema lookup so it resolves gracefully if present.
class c_player_item_services
{
public:
	SCHEMA_ADD_FIELD(bool, m_bHasDefuser, "CCSPlayer_ItemServices->m_bHasDefuser");
	SCHEMA_ADD_FIELD(bool, m_bHasHelmet, "CCSPlayer_ItemServices->m_bHasHelmet");
	// m_bHasHeavyArmor not in this dump — keep as schema field for forward compat
	SCHEMA_ADD_FIELD(bool, m_bHasHeavyArmor, "CCSPlayer_ItemServices->m_bHasHeavyArmor");
};

// ── C_BasePlayerPawn ──────────────────────────────────────────────────────────
// m_pWeaponServices @ 0x11E0  (dump: C_BasePlayerPawn->m_pWeaponServices = 0x11E0)
// m_pItemServices   @ 0x11E8  (dump: C_BasePlayerPawn->m_pItemServices   = 0x11E8)
class c_base_player_pawn : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(c_player_weapon_services*, m_pWeaponServices, "C_BasePlayerPawn->m_pWeaponServices");
	SCHEMA_ADD_FIELD(c_player_item_services*, m_pItemServices, "C_BasePlayerPawn->m_pItemServices");
};

// ── C_CSPlayerPawn ────────────────────────────────────────────────────────────
// m_ArmorValue       @ 0x1C74  (dump: C_CSPlayerPawn->m_ArmorValue       = 0x1C74)
// m_angEyeAngles     @ 0x3300  (dump: C_CSPlayerPawn->m_angEyeAngles     = 0x3300)
// m_flFlashDuration  @ 0x1400  (dump: C_CSPlayerPawnBase->m_flFlashDuration     = 0x1400)
// m_flFlashOverlayAlpha @ 0x13F4 (dump: C_CSPlayerPawnBase->m_flFlashOverlayAlpha = 0x13F4)
class c_cs_player_pawn : public c_base_player_pawn
{
public:
	SCHEMA_ADD_FIELD(int32_t, m_ArmorValue, "C_CSPlayerPawn->m_ArmorValue");
	SCHEMA_ADD_FIELD(f_vector, m_angEyeAngles, "C_CSPlayerPawn->m_angEyeAngles");
	SCHEMA_ADD_FIELD(float, m_flFlashDuration, "C_CSPlayerPawnBase->m_flFlashDuration");
	SCHEMA_ADD_FIELD(float, m_flFlashOverlayAlpha, "C_CSPlayerPawnBase->m_flFlashOverlayAlpha");

	const std::string get_model_name();
};

// ── CBasePlayerController ─────────────────────────────────────────────────────
// m_hPawn    @ 0x6BC  (dump: CBasePlayerController->m_hPawn    = 0x6BC)
// m_steamID  @ 0x778  (dump: CBasePlayerController->m_steamID  = 0x778)
class c_base_player_controller : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(c_base_handle, m_hPawn, "CBasePlayerController->m_hPawn");
	SCHEMA_ADD_FIELD(uint64_t, m_steamID, "CBasePlayerController->m_steamID");
};

// ── CCSPlayerController_InGameMoneyServices ───────────────────────────────────
// m_iAccount @ 0x40  (dump: CCSPlayerController_InGameMoneyServices->m_iAccount = 0x40)
class c_in_game_money_services
{
public:
	SCHEMA_ADD_FIELD(int32_t, m_iAccount, "CCSPlayerController_InGameMoneyServices->m_iAccount");
};

// ── CCSPlayerController_InventoryServices ────────────────────────────────────
// m_rank @ 0x5C  (dump: CCSPlayerController_InventoryServices->m_rank = 0x5C, MedalRank_t[6])
// We read the first element as uint32_t for the primary rank.
class c_inventory_services
{
public:
	SCHEMA_ADD_OFFSET(uint32_t, m_rank_id, 0x5C);
};

// ── CCSPlayerController_ActionTrackingServices ────────────────────────────────
// m_matchStats @ 0xA8  (dump: CCSPlayerController_ActionTrackingServices->m_matchStats = 0xA8)
// CSPerRoundStats_t::m_iKills   @ 0x30
// CSPerRoundStats_t::m_iDeaths  @ 0x34
// CSPerRoundStats_t::m_iAssists @ 0x38
class c_cs_player_controller_action_tracking_services
{
public:
	int32_t get_kills() const
	{
		return m_memory->read_t<int32_t>(
			reinterpret_cast<uintptr_t>(this)
			+ SCHEMA_GET_OFFSET("CCSPlayerController_ActionTrackingServices->m_matchStats")
			+ SCHEMA_GET_OFFSET("CSPerRoundStats_t->m_iKills"));
	}

	int32_t get_deaths() const
	{
		return m_memory->read_t<int32_t>(
			reinterpret_cast<uintptr_t>(this)
			+ SCHEMA_GET_OFFSET("CCSPlayerController_ActionTrackingServices->m_matchStats")
			+ SCHEMA_GET_OFFSET("CSPerRoundStats_t->m_iDeaths"));
	}

	int32_t get_assists() const
	{
		return m_memory->read_t<int32_t>(
			reinterpret_cast<uintptr_t>(this)
			+ SCHEMA_GET_OFFSET("CCSPlayerController_ActionTrackingServices->m_matchStats")
			+ SCHEMA_GET_OFFSET("CSPerRoundStats_t->m_iAssists"));
	}
};

// ── CCSPlayerController ───────────────────────────────────────────────────────
// m_pInGameMoneyServices      @ 0x800  (dump: CCSPlayerController->m_pInGameMoneyServices      = 0x800)
// m_pInventoryServices        @ 0x808  (dump: CCSPlayerController->m_pInventoryServices        = 0x808)
// m_pActionTrackingServices   @ 0x810  (dump: CCSPlayerController->m_pActionTrackingServices   = 0x810)
// m_iCompTeammateColor        @ 0x840  (dump: CCSPlayerController->m_iCompTeammateColor        = 0x840)
// m_iCompetitiveRanking       @ 0x878  (dump: CCSPlayerController->m_iCompetitiveRanking       = 0x878)
// m_iPing                     @ 0x820  (dump: CCSPlayerController->m_iPing                     = 0x820)
// m_iScore                    @ 0x92C  (dump: CCSPlayerController->m_iScore                    = 0x92C)
// m_sSanitizedPlayerName      @ 0x858  (dump: CCSPlayerController->m_sSanitizedPlayerName      = 0x858, CUtlString)
class c_cs_player_controller : public c_base_player_controller
{
public:
	SCHEMA_ADD_FIELD(c_in_game_money_services*, m_pInGameMoneyServices, "CCSPlayerController->m_pInGameMoneyServices");
	SCHEMA_ADD_FIELD(c_inventory_services*, m_pInventoryServices, "CCSPlayerController->m_pInventoryServices");
	SCHEMA_ADD_FIELD(c_cs_player_controller_action_tracking_services*, m_pActionTrackingServices, "CCSPlayerController->m_pActionTrackingServices");
	SCHEMA_ADD_FIELD(e_colors, m_iCompTeammateColor, "CCSPlayerController->m_iCompTeammateColor");
	SCHEMA_ADD_FIELD(int32_t, m_iCompetitiveRanking, "CCSPlayerController->m_iCompetitiveRanking");
	SCHEMA_ADD_FIELD(uint32_t, m_iPing, "CCSPlayerController->m_iPing");
	SCHEMA_ADD_FIELD(int32_t, m_iScore, "CCSPlayerController->m_iScore");
	SCHEMA_ADD_STRING(m_sSanitizedPlayerName, "CCSPlayerController->m_sSanitizedPlayerName");

	static c_cs_player_controller* get_local_player_controller();
	c_cs_player_pawn* get_player_pawn();
	const e_colors     get_color();
	const f_vector& get_vec_origin();
};

// ── C_PlantedC4 ───────────────────────────────────────────────────────────────
// m_bBombTicking     @ 0x1160  (dump: C_PlantedC4->m_bBombTicking     = 0x1160)
// m_flC4Blow         @ 0x1190  (dump: C_PlantedC4->m_flC4Blow         = 0x1190)
// m_bBombDefused     @ 0x11B4  (dump: C_PlantedC4->m_bBombDefused     = 0x11B4)
// m_bBeingDefused    @ 0x119C  (dump: C_PlantedC4->m_bBeingDefused    = 0x119C)
// m_flDefuseCountDown@ 0x11B0  (dump: C_PlantedC4->m_flDefuseCountDown= 0x11B0)
class c_planted_c4 : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(bool, m_bBombTicking, "C_PlantedC4->m_bBombTicking");
	SCHEMA_ADD_FIELD(float, m_flC4Blow, "C_PlantedC4->m_flC4Blow");
	SCHEMA_ADD_FIELD(bool, m_bBombDefused, "C_PlantedC4->m_bBombDefused");
	SCHEMA_ADD_FIELD(bool, m_bBeingDefused, "C_PlantedC4->m_bBeingDefused");
	SCHEMA_ADD_FIELD(float, m_flDefuseCountDown, "C_PlantedC4->m_flDefuseCountDown");
};

// ── CCSWeaponBaseVData ────────────────────────────────────────────────────────
// m_WeaponType @ 0x520  (dump: CCSWeaponBaseVData->m_WeaponType = 0x520, CSWeaponType)
// m_szName     @ 0x720  (dump: CCSWeaponBaseVData->m_szName     = 0x720, CGlobalSymbol)
// m_iMaxClip1  @ 0x4D0  (dump: CBasePlayerWeaponVData->m_iMaxClip1 = 0x4D0)
class c_cs_weapon_base_v_data
{
public:
	SCHEMA_ADD_FIELD(e_weapon_type, m_WeaponType, "CCSWeaponBaseVData->m_WeaponType");
	SCHEMA_ADD_STRING(m_szName, "CCSWeaponBaseVData->m_szName");
	SCHEMA_ADD_FIELD(int32_t, m_iMaxClip1, "CBasePlayerWeaponVData->m_iMaxClip1");
};

// ── C_BasePlayerWeapon ────────────────────────────────────────────────────────
// m_WeaponData : via C_BaseEntity->m_nSubclassID + 0x08  (unchanged pattern)
// m_iClip1     @ 0x16D8  (dump: C_BasePlayerWeapon->m_iClip1 = 0x16D8)
// m_pReserveAmmo @ 0x16E0 (dump: C_BasePlayerWeapon->m_pReserveAmmo = 0x16E0, int32[2] — read element[0])
class c_base_player_weapon : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD_OFFSET(c_cs_weapon_base_v_data*, m_WeaponData, "C_BaseEntity->m_nSubclassID", 0x08);
	SCHEMA_ADD_FIELD(int32_t, m_iClip1, "C_BasePlayerWeapon->m_iClip1");

	int32_t get_reserve_ammo() const
	{
		// m_pReserveAmmo is int32[2] stored inline; first element is reserve ammo
		const auto addr = reinterpret_cast<uintptr_t>(this)
			+ SCHEMA_GET_OFFSET("C_BasePlayerWeapon->m_pReserveAmmo");
		if (!addr || addr <= 0x10000)
			return 0;
		return m_memory->read_t<int32_t>(addr);
	}

	c_base_player_weapon* get(const int32_t idx);
};

// ── Grenade projectile classes ────────────────────────────────────────────────

// C_BaseGrenade
// m_bIsLive        @ 0x115A  (dump: C_BaseGrenade->m_bIsLive        = 0x115A)
// m_flDetonateTime @ 0x1160  (dump: C_BaseGrenade->m_flDetonateTime = 0x1160)
// m_hThrower       @ 0x1180  (dump: C_BaseGrenade->m_hThrower       = 0x1180)
class c_base_grenade : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(bool, m_bIsLive, "C_BaseGrenade->m_bIsLive");
	SCHEMA_ADD_FIELD(float, m_flDetonateTime, "C_BaseGrenade->m_flDetonateTime");
	SCHEMA_ADD_FIELD(c_base_handle, m_hThrower, "C_BaseGrenade->m_hThrower");
};

// C_BaseCSGrenadeProjectile
// m_vInitialVelocity   @ 0x11AC  (dump: C_BaseCSGrenadeProjectile->m_vInitialVelocity   = 0x11AC)
// m_flSpawnTime        @ 0x11D8  (dump: C_BaseCSGrenadeProjectile->m_flSpawnTime        = 0x11D8)
// m_bExplodeEffectBegan@ 0x11EC  (dump: C_BaseCSGrenadeProjectile->m_bExplodeEffectBegan= 0x11EC)
class c_base_cs_grenade_projectile : public c_base_grenade
{
public:
	SCHEMA_ADD_FIELD(f_vector, m_vInitialVelocity, "C_BaseCSGrenadeProjectile->m_vInitialVelocity");
	SCHEMA_ADD_FIELD(float, m_flSpawnTime, "C_BaseCSGrenadeProjectile->m_flSpawnTime");
	SCHEMA_ADD_FIELD(bool, m_bExplodeEffectBegan, "C_BaseCSGrenadeProjectile->m_bExplodeEffectBegan");
};

// C_SmokeGrenadeProjectile
// m_bDidSmokeEffect      @ 0x1254  (dump: C_SmokeGrenadeProjectile->m_bDidSmokeEffect      = 0x1254)
// m_bSmokeEffectSpawned  @ 0x1299  (dump: C_SmokeGrenadeProjectile->m_bSmokeEffectSpawned  = 0x1299)
// m_vSmokeDetonationPos  @ 0x1268  (dump: C_SmokeGrenadeProjectile->m_vSmokeDetonationPos  = 0x1268)
class c_smoke_grenade_projectile : public c_base_cs_grenade_projectile
{
public:
	SCHEMA_ADD_FIELD(bool, m_bDidSmokeEffect, "C_SmokeGrenadeProjectile->m_bDidSmokeEffect");
	SCHEMA_ADD_FIELD(bool, m_bSmokeEffectSpawned, "C_SmokeGrenadeProjectile->m_bSmokeEffectSpawned");
	SCHEMA_ADD_FIELD(f_vector, m_vSmokeDetonationPos, "C_SmokeGrenadeProjectile->m_vSmokeDetonationPos");
};

// C_MolotovProjectile
// m_bIsIncGrenade @ 0x1238  (dump: C_MolotovProjectile->m_bIsIncGrenade = 0x1238)
class c_molotov_projectile : public c_base_cs_grenade_projectile
{
public:
	SCHEMA_ADD_FIELD(bool, m_bIsIncGrenade, "C_MolotovProjectile->m_bIsIncGrenade");
};

// C_DecoyProjectile
// m_nDecoyShotTick @ 0x1238  (dump: C_DecoyProjectile->m_nDecoyShotTick = 0x1238)
class c_decoy_projectile : public c_base_cs_grenade_projectile
{
public:
	SCHEMA_ADD_FIELD(int32_t, m_nDecoyShotTick, "C_DecoyProjectile->m_nDecoyShotTick");
};

// C_Inferno
// m_fireCount        @ 0x1958  (dump: C_Inferno->m_fireCount        = 0x1958)
// m_nInfernoType     @ 0x195C  (dump: C_Inferno->m_nInfernoType     = 0x195C)
// m_nFireLifetime    @ 0x1960  (dump: C_Inferno->m_nFireLifetime    = 0x1960)
// m_bInPostEffectTime@ 0x1964  (dump: C_Inferno->m_bInPostEffectTime= 0x1964)
class c_inferno : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(int32_t, m_fireCount, "C_Inferno->m_fireCount");
	SCHEMA_ADD_FIELD(uint32_t, m_nInfernoType, "C_Inferno->m_nInfernoType");
	SCHEMA_ADD_FIELD(float, m_nFireLifetime, "C_Inferno->m_nFireLifetime");
	SCHEMA_ADD_FIELD(bool, m_bInPostEffectTime, "C_Inferno->m_bInPostEffectTime");
};

// ── C_CSGameRules ─────────────────────────────────────────────────────────────
// All offsets from dump namespace C_CSGameRules (2026-04-23):
// m_bFreezePeriod        @ 0x40
// m_bWarmupPeriod        @ 0x41
// m_fWarmupPeriodEnd     @ 0x44
// m_iRoundTime           @ 0x68
// m_fRoundStartTime      @ 0x70
// m_flRestartRoundTime   @ 0x74
// m_gamePhase            @ 0x84
// m_totalRoundsPlayed    @ 0x88
// m_nRoundsPlayedThisPhase @ 0x8C
// m_nOvertimePlaying     @ 0x90
// m_bBombPlanted         @ 0x8C7
// m_bBombDropped         @ 0x9A8
// m_iRoundWinStatus      @ 0x9AC
class c_cs_game_rules : public c_base_entity
{
public:
	SCHEMA_ADD_FIELD(bool, m_bFreezePeriod, "C_CSGameRules->m_bFreezePeriod");
	SCHEMA_ADD_FIELD(bool, m_bWarmupPeriod, "C_CSGameRules->m_bWarmupPeriod");
	SCHEMA_ADD_FIELD(float, m_fWarmupPeriodEnd, "C_CSGameRules->m_fWarmupPeriodEnd");
	SCHEMA_ADD_FIELD(int32_t, m_iRoundTime, "C_CSGameRules->m_iRoundTime");
	SCHEMA_ADD_FIELD(float, m_fRoundStartTime, "C_CSGameRules->m_fRoundStartTime");
	SCHEMA_ADD_FIELD(float, m_flRestartRoundTime, "C_CSGameRules->m_flRestartRoundTime");
	SCHEMA_ADD_FIELD(int32_t, m_gamePhase, "C_CSGameRules->m_gamePhase");
	SCHEMA_ADD_FIELD(int32_t, m_totalRoundsPlayed, "C_CSGameRules->m_totalRoundsPlayed");
	SCHEMA_ADD_FIELD(int32_t, m_nRoundsPlayedThisPhase, "C_CSGameRules->m_nRoundsPlayedThisPhase");
	SCHEMA_ADD_FIELD(int32_t, m_nOvertimePlaying, "C_CSGameRules->m_nOvertimePlaying");
	SCHEMA_ADD_FIELD(bool, m_bBombPlanted, "C_CSGameRules->m_bBombPlanted");
	SCHEMA_ADD_FIELD(bool, m_bBombDropped, "C_CSGameRules->m_bBombDropped");
	SCHEMA_ADD_FIELD(int32_t, m_iRoundWinStatus, "C_CSGameRules->m_iRoundWinStatus");
};

// ── C_CSGameRulesProxy ────────────────────────────────────────────────────────
// m_pGameRules @ 0x600  (dump: C_CSGameRulesProxy->m_pGameRules = 0x600)
class c_cs_game_rules_proxy : public c_base_entity
{
public:
	c_cs_game_rules* get_game_rules()
	{
		return m_memory->read_t<c_cs_game_rules*>(
			reinterpret_cast<uintptr_t>(this) + 0x600);
	}
};

// ── C_CSTeam ──────────────────────────────────────────────────────────────────
// m_scoreFirstHalf  @ 0x8C0  (dump: C_CSTeam->m_scoreFirstHalf  = 0x8C0)
// m_scoreSecondHalf @ 0x8C4  (dump: C_CSTeam->m_scoreSecondHalf = 0x8C4)
// m_scoreOvertime   @ 0x8C8  (dump: C_CSTeam->m_scoreOvertime   = 0x8C8)
class c_cs_team : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(int32_t, m_scoreFirstHalf, 0x8C0);
	SCHEMA_ADD_OFFSET(int32_t, m_scoreSecondHalf, 0x8C4);
	SCHEMA_ADD_OFFSET(int32_t, m_scoreOvertime, 0x8C8);

	int32_t m_iScore()
	{
		return m_scoreFirstHalf() + m_scoreSecondHalf() + m_scoreOvertime();
	}
};