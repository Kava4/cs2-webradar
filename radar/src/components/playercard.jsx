import { useState } from "react";
import MaskedIcon from "./maskedicon";
import { playerColors, teamEnum } from "../utilities/utilities";

const hpColor = hp => hp > 60 ? "#3dd68c" : hp > 30 ? "#f0b429" : "#e05252";

/* ─── C4 Carrier badge ──────────────────────────────────────────────────────── */
const C4Badge = () => (
  <>
    <style>{`
      @keyframes c4pulse {
        0%   { box-shadow: 0 0 0 0 rgba(239,68,68,0.9), 0 0 8px rgba(239,68,68,0.6); opacity: 1; }
        50%  { box-shadow: 0 0 0 5px rgba(239,68,68,0), 0 0 16px rgba(239,68,68,0.3); opacity: 0.85; }
        100% { box-shadow: 0 0 0 0 rgba(239,68,68,0), 0 0 8px rgba(239,68,68,0.6); opacity: 1; }
      }
      @keyframes c4glow {
        0%,100% { opacity: 1; transform: scale(1); }
        50%     { opacity: 0.75; transform: scale(0.96); }
      }
    `}</style>
    <div style={{
      display: "flex",
      alignItems: "center",
      gap: 4,
      padding: "3px 6px",
      borderRadius: 4,
      background: "rgba(239,68,68,0.18)",
      border: "1.5px solid #ef4444",
      animation: "c4pulse 1.1s ease-in-out infinite",
    }}>
      <div style={{
        width: 12, height: 12,
        background: "#ef4444",
        WebkitMask: "url('./assets/icons/c4.svg') no-repeat center/contain",
        animation: "c4glow 1.1s ease-in-out infinite",
        flexShrink: 0,
      }} />
      <span style={{
        fontSize: 9,
        fontWeight: 900,
        color: "#ef4444",
        letterSpacing: "0.08em",
        lineHeight: 1,
      }}>C4</span>
    </div>
  </>
);

/* ─── Weapon icon ───────────────────────────────────────────────────────────── */
const WIcon = ({ name, isActive, h = 16 }) => {
  if (!name) return null;
  return (
    <div style={{
      display: "flex", alignItems: "center", justifyContent: "center",
      padding: "3px 15px", borderRadius: 4,
      background: isActive ? "rgba(245,166,35,0.15)" : "rgba(255,255,255,0.04)",
      border: `1px solid ${isActive ? "rgba(245,166,35,0.4)" : "rgba(255,255,255,0.07)"}`,
      
    }}>
      <MaskedIcon path={`./assets/icons/${name}.svg`} height={h} color={isActive ? "bg-amber-400" : "bg-white/45"} />
    </div>
  );
};

const GIcon = ({ name, isActive }) => (
  <div style={{
    display: "flex", alignItems: "center", justifyContent: "center",
    padding: "2px 4px", borderRadius: 3,
    background: isActive ? "rgba(245,166,35,0.12)" : "rgba(255,255,255,0.03)",
    border: `1px solid ${isActive ? "rgba(245,166,35,0.35)" : "rgba(255,255,255,0.06)"}`,
  }}>
    <MaskedIcon path={`./assets/icons/${name}.svg`} height={12} color={isActive ? "bg-amber-400" : "bg-white/40"} />
  </div>
);

const AmmoBar = ({ clip, maxClip, settings }) => {
  if (!maxClip || (settings && !settings.showAmmo)) return null;
  const pct = clip / maxClip;
  const color = pct > 0.5 ? "#3dd68c" : pct > 0.25 ? "#f0b429" : "#e05252";
  const count = Math.min(maxClip, 20);
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 2 }}>
      {Array.from({ length: count }).map((_, i) => (
        <div key={i} style={{
          width: 2, height: i % 5 === 4 ? 7 : 4, borderRadius: 1, flexShrink: 0,
          background: pct > i / count ? color : "rgba(255,255,255,0.07)",
        }} />
      ))}
      <span style={{ fontSize: 16, fontFamily: "monospace", fontWeight: 700, color: "rgba(255,255,255,0.3)", marginLeft: 2, lineHeight: 1 }}>
        {clip ?? 0}
      </span>
    </div>
  );
};

const Portrait = ({ avatarUrl, model, isDead, accentColor, isRight }) => {
  const [avatarErr, setAvatarErr] = useState(false);
  const [modelErr, setModelErr] = useState(false);
  const showAvatar = avatarUrl && !avatarErr;
  const showModel = !showAvatar && model && !modelErr;

  return (
    <div style={{
      position: "relative", width: 85, flexShrink: 0, alignSelf: "stretch", overflow: "hidden",
      background: `linear-gradient(${isRight ? "720deg" : "90deg"}, ${accentColor}0a, transparent)`,
      borderLeft:  isRight  ? `2px solid ${accentColor}30` : "none",
      borderRight: !isRight ? `2px solid ${accentColor}30` : "none",
    }}>
      {showAvatar && (
        <img src={avatarUrl} alt="" onError={() => setAvatarErr(true)} style={{
          position: "absolute", inset: 0, width: "100%", height: "100%",
          objectFit: "cover", objectPosition: "center top",
          filter: isDead ? "grayscale(1) brightness(0.3)" : "none", opacity: isDead ? 0.5 : 1,
        }} />
      )}
      {showModel && (
        <img src={`./assets/characters/${model}.png`} alt="" onError={() => setModelErr(true)} style={{
          position: "absolute", inset: 0, width: "100%", height: "100%",
          objectFit: "cover", objectPosition: "top center",
          transform: "scale(1.7) translateY(-6%)", transformOrigin: "top center",
          filter: isDead ? "grayscale(1) brightness(0.3)" : "saturate(0.85)", opacity: isDead ? 0.5 : 1,
        }} />
      )}
      {!showAvatar && !showModel && (
        <div style={{ position: "absolute", inset: 0, display: "flex", alignItems: "center", justifyContent: "center" }}>
          <span style={{ fontSize: 20, color: accentColor, opacity: 0.2 }}>?</span>
        </div>
      )}
      <div style={{
        position: "absolute", top: 0, bottom: 0, width: 24,
        [isRight ? "left" : "right"]: 0,
        background: `linear-gradient(${isRight ? "90deg" : "270deg"}, var(--bg-card) 0%, transparent 100%)`,
        pointerEvents: "none",
      }} />
    </div>
  );
};

const HpBar = ({ hp, isDead, settings }) => {
  if (settings && !settings.showHealthBars) return null;
  return (
    <div style={{ position: "absolute", bottom: 0, left: 0, right: 0, height: 2, background: "rgba(0,0,0,0.4)" }}>
      <div style={{ height: "100%", width: `${isDead ? 0 : hp}%`, background: hpColor(hp), transition: "width 0.4s ease" }} />
    </div>
  );
};

const InfoPanel = ({ playerData, isRight, settings }) => {
  const isDead  = playerData.m_is_dead;
  const hp      = playerData.m_health  ?? 0;
  const kills   = playerData.m_kills   ?? 0;
  const deaths  = playerData.m_deaths  ?? 0;
  const assists = playerData.m_assists ?? 0;
  const money   = playerData.m_money   ?? 0;
  const weapons = playerData.m_weapons ?? {};
  const activeW = weapons.m_active;
  const utils   = weapons.m_utilities ?? [];
  const hasBomb = !isDead && playerData.m_team === teamEnum.terrorist && playerData.m_has_bomb;

  return (
    <div style={{
      flex: 1, display: "flex", flexDirection: "column",
      padding: "2px 10px 50px", gap: 4, minWidth: 0,
      alignItems: isRight ? "flex-end" : "flex-start", position: "relative",
    }}>
      {/* Flash overlay */}
      {playerData.m_flash_alpha > 0 && settings?.showFlashEffect !== false && (
        <div style={{
          position: "absolute", inset: 0,
          background: "rgba(255,255,255,0.18)",
          opacity: Math.min(playerData.m_flash_alpha, 1),
          pointerEvents: "none",
        }} />
      )}

      {/* Row 1: Name + HP badge */}
      <div style={{
        display: "flex", alignItems: "center", justifyContent: "space-between",
        width: "100%", gap: 6,
        flexDirection: isRight ? "row-reverse" : "row",
      }}>
        <span style={{
          fontSize: 12, fontWeight: 800, lineHeight: 1,
          color: isDead ? "rgba(255,255,255,0.22)" : "var(--text-primary)",
          overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap",
          flex: 1, textAlign: isRight ? "right" : "left",
        }}>
          {playerData.m_name}
        </span>
        <div style={{
          display: "flex", alignItems: "center", justifyContent: "center",
          minWidth: 28, padding: "2px 4px", borderRadius: 3, flexShrink: 0,
          background: isDead ? "rgba(255,255,255,0.03)" : `${hpColor(hp)}14`,
          border: `1px solid ${isDead ? "rgba(255,255,255,0.06)" : `${hpColor(hp)}35`}`,
        }}>
          <span style={{ fontSize: 11, fontWeight: 800, fontFamily: "monospace", lineHeight: 1, color: isDead ? "rgba(255,255,255,0.2)" : hpColor(hp) }}>
            {isDead ? "✕" : hp}
          </span>
        </div>
      </div>

      {/* Row 2: Money | KDA | Armor | Ping */}
      <div style={{ display: "flex", alignItems: "center", gap: 7, flexDirection: isRight ? "row-reverse" : "row" }}>
        {settings?.showMoney !== false && (
          <span style={{ fontSize: 14, fontFamily: "monospace", fontWeight: 700, lineHeight: 1, color: isDead ? "rgba(61,214,140,0.25)" : "var(--money)" }}>
            ${money.toLocaleString()}
          </span>
        )}
        <span style={{ fontSize: 14, fontFamily: "monospace", fontWeight: 700, lineHeight: 1 }}>
          <span style={{ color: "#3dd68c" }}>{kills}</span>
          <span style={{ color: "rgba(255,255,255,0.12)" }}>/</span>
          <span style={{ color: "#e05252" }}>{deaths}</span>
          <span style={{ color: "rgba(255,255,255,0.12)" }}>/</span>
          <span style={{ color: "#60a5fa" }}>{assists}</span>
        </span>
        {!isDead && playerData.m_armor > 0 && settings?.showArmor !== false && (
          <div style={{ display: "flex", alignItems: "center", gap: 2 }}>
            <MaskedIcon path={`./assets/icons/${playerData.m_has_helmet ? "kevlar_helmet" : "kevlar"}.svg`} height={9} color="bg-blue-400/50" />
            <span style={{ fontSize: 16, fontFamily: "monospace", fontWeight: 700, color: "rgba(96,165,250,0.6)", lineHeight: 1 }}>
              {playerData.m_armor}
            </span>
          </div>
        )}
        {settings?.showPing !== false && playerData.m_iPing > 0 && playerData.m_iPing < 999 && (
          <span style={{
            fontSize: 16, fontFamily: "monospace", fontWeight: 700, lineHeight: 1,
            color: playerData.m_iPing < 40 ? "rgba(61,214,140,0.6)" : playerData.m_iPing < 60 ? "rgba(240,180,41,0.6)" : "rgba(224,82,82,0.6)",
          }}>
            {playerData.m_iPing}ms
          </span>
        )}
      </div>

      {/* Row 3: Weapons + C4 badge */}
      <div style={{ display: "flex", alignItems: "center", gap: 4, flexDirection: isRight ? "row-reverse" : "row" }}>
        {weapons.m_primary   && <WIcon name={weapons.m_primary}   isActive={activeW === weapons.m_primary}   h={16} />}
        {weapons.m_secondary && <WIcon name={weapons.m_secondary} isActive={activeW === weapons.m_secondary} h={12} />}

        {/* ── C4 CARRIER — very visible ── */}
        {hasBomb && <C4Badge />}

        {!isDead && playerData.m_team === teamEnum.counterTerrorist && playerData.m_has_defuser && (
          <div style={{ padding: "3px 4px", borderRadius: 3, background: "rgba(74,158,255,0.1)", border: "1px solid rgba(74,158,255,0.3)", display: "flex", alignItems: "center" }}>
            <MaskedIcon path="./assets/icons/defuser.svg" height={12} color="bg-blue-400/80" />
          </div>
        )}
      </div>

      {/* Row 4: Grenades + Ammo */}
      {!isDead && (utils.length > 0 || weapons.m_ammo_max_clip > 0) && (
        <div style={{ display: "flex", alignItems: "center", gap: 3, flexDirection: isRight ? "row-reverse" : "row" }}>
          {utils.map((u, i) => <GIcon key={i} name={u} isActive={activeW === u} />)}
          <AmmoBar clip={weapons.m_ammo_clip} maxClip={weapons.m_ammo_max_clip} settings={settings} />
        </div>
      )}
    </div>
  );
};

const PlayerCard = ({ playerData, isOnRightSide, settings }) => {
  const isDead = playerData.m_is_dead;
  const isT    = playerData.m_team === 2;
  const hp     = playerData.m_health ?? 0;
  const accentHex = isT ? "#f5a623" : "#4a9eff";

  const portrait = <Portrait avatarUrl={playerData.m_avatar_url || null} model={playerData.m_model_name || null} isDead={isDead} accentColor={accentHex} isRight={isOnRightSide} />;
  const info     = <InfoPanel playerData={playerData} isRight={isOnRightSide} settings={settings} />;

  return (
    <li style={{
      position: "relative", display: "flex", alignItems: "stretch",
      overflow: "hidden", listStyle: "none", borderRadius: 6, minHeight: 74,
      background: isDead
        ? "var(--bg-panel)"
        : isT
        ? "linear-gradient(90deg, rgba(245,166,35,0.07) 0%, var(--bg-card) 100%)"
        : "linear-gradient(270deg, rgba(74,158,255,0.07) 0%, var(--bg-card) 100%)",
      border: `1px solid ${isDead ? "var(--bg-border-dim)" : isT ? "var(--t-border)" : "var(--ct-border)"}`,
      opacity: isDead ? 0.4 : 1,
      transition: "opacity 0.35s",
    }}>
      {isOnRightSide ? <>{info}{portrait}</> : <>{portrait}{info}</>}
      <HpBar hp={hp} isDead={isDead} settings={settings} />
    </li>
  );
};

export default PlayerCard;