import { useState, useEffect } from "react";

export const RANK_NAMES = [
  "Unranked",
  "Silver I","Silver II","Silver III","Silver IV",
  "Silver Elite","Silver Elite Master",
  "Gold Nova I","Gold Nova II","Gold Nova III","Gold Nova Master",
  "Master Guardian I","Master Guardian II","Master Guardian Elite",
  "Distinguished Master Guardian",
  "Legendary Eagle","Legendary Eagle Master",
  "Supreme Master First Class","The Global Elite",
];

export const RANK_COLORS = [
  "#444",
  "#8a8aa0","#8a8aa0","#8a8aa0","#8a8aa0","#9898b0","#a0a0be",
  "#b8932a","#b8932a","#c4a030","#c4a030",
  "#4a8ec4","#4a8ec4","#58a0d0","#50aad8",
  "#b85858","#c46666","#d04444","#d8c030",
];

const T_COLOR  = "#f5a623";
const CT_COLOR = "#4a9eff";
const BOMB_CLR = "#e05252";

const fmt = (s) => {
  if (s == null || s < 0) return "0:00";
  const m = Math.floor(s / 60);
  const sec = Math.floor(s % 60);
  return `${m}:${sec.toString().padStart(2, "0")}`;
};

/* ── Bomb centre ─────────────────────────────────────────────────────────────── */
const BombCentre = ({ bombData, roundNum, compact }) => {
  const blow    = bombData?.m_blow_time   ?? 0;
  const defuse  = bombData?.m_defuse_time ?? 0;
  const defusing = bombData?.m_is_defusing ?? false;
  const defused  = bombData?.m_is_defused  ?? false;

  const pct = Math.max(0, Math.min(1, blow / 40));
  const defPct = defusing && defuse > 0
    ? Math.max(0, Math.min(1, defuse / (bombData?.m_has_kit ? 5 : 10)))
    : 0;

  const [blink, setBlink] = useState(false);
  useEffect(() => {
    if (defused) { setBlink(false); return; }
    const id = setInterval(() => setBlink(b => !b), blow < 10 ? 240 : 480);
    return () => clearInterval(id);
  }, [blow < 10, defused]);

  const timeColor = defused ? "#3dd68c"
    : blow < 5  ? (blink ? "#fca5a5" : BOMB_CLR)
    : blow < 10 ? (blink ? "#f87171" : "#fca5a5")
    : "#fca5a5";

  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      justifyContent: "center",
      padding: compact ? "0 12px" : "0 20px",
      gap: 2,
      background: "rgba(40,8,8,0.9)",
      borderLeft: "1px solid rgba(224,82,82,0.2)",
      borderRight: "1px solid rgba(224,82,82,0.2)",
      minWidth: compact ? 100 : 130,
      position: "relative",
      overflow: "hidden",
    }}>
      {/* Blow timer bar — bottom */}
      <div style={{ position: "absolute", bottom: 0, left: 0, right: 0, height: 2, background: "rgba(0,0,0,0.5)" }}>
        <div style={{
          height: "100%",
          width: `${pct * 100}%`,
          background: BOMB_CLR,
          boxShadow: `0 0 6px ${BOMB_CLR}80`,
          transition: "width 0.1s linear",
        }} />
      </div>

      {/* Defuse bar — top */}
      {defusing && (
        <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: 2, background: "rgba(0,0,0,0.5)" }}>
          <div style={{
            height: "100%",
            width: `${(1 - defPct) * 100}%`,
            background: CT_COLOR,
            boxShadow: `0 0 6px ${CT_COLOR}80`,
            transition: "width 0.1s linear",
          }} />
        </div>
      )}

      {/* Round label */}
      <span style={{
        fontSize: 8,
        fontWeight: 700,
        letterSpacing: "0.2em",
        color: "rgba(255,255,255,0.2)",
        lineHeight: 1,
        textTransform: "uppercase",
      }}>
        {roundNum > 0 ? `RND ${roundNum}` : "—"}
      </span>

      {/* Bomb icon + time */}
      <div style={{ display: "flex", alignItems: "center", gap: 5 }}>
        <div style={{
          width: 12,
          height: 12,
          background: defused ? "#3dd68c" : BOMB_CLR,
          WebkitMask: "url('./assets/icons/c4_sml.png') no-repeat center/contain",
          flexShrink: 0,
        }} />
        <span style={{
          fontSize: compact ? 18 : 24,
          fontWeight: 900,
          fontFamily: "monospace",
          lineHeight: 1,
          color: timeColor,
          textShadow: `0 0 16px ${timeColor}80`,
          letterSpacing: "-0.02em",
          transition: "color 0.08s",
        }}>
          {defused ? "SAFE" : `${blow.toFixed(1)}s`}
        </span>
      </div>

      {/* Status label */}
      <span style={{
        fontSize: 7,
        fontWeight: 900,
        letterSpacing: "0.18em",
        lineHeight: 1,
        color: defusing ? CT_COLOR : "#f87171",
        animation: "phasePulse 0.65s ease-in-out infinite alternate",
        minHeight: 9,
        textTransform: "uppercase",
      }}>
        {defused ? "" : defusing ? `DEFUSE ${defuse.toFixed(1)}s` : "PLANTED"}
      </span>

      <style>{`
        @keyframes phasePulse { 0% { opacity:0.65; } 100% { opacity:1; } }
      `}</style>
    </div>
  );
};

/* ── Normal centre ───────────────────────────────────────────────────────────── */
const NormalCentre = ({ time, roundNum, isWarmup, isFreeze, isBomb, shouldBlink, blink, compact }) => {
  const timeColor = shouldBlink ? (blink ? "#f87171" : "#fca5a5") : "var(--text-primary)";

  let phaseLabel = null;
  if (isWarmup)      phaseLabel = { text: "WARMUP", color: "#f0b429" };
  else if (isFreeze) phaseLabel = { text: "BUY",    color: CT_COLOR };
  else if (isBomb)   phaseLabel = { text: "BOMB",   color: "#f87171" };

  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      justifyContent: "center",
      padding: compact ? "0 14px" : "0 24px",
      gap: 2,
      background: "var(--bg-panel)",
      minWidth: compact ? 88 : 120,
    }}>
      <span style={{
        fontSize: 8,
        fontWeight: 700,
        letterSpacing: "0.2em",
        color: "rgba(255,255,255,0.2)",
        lineHeight: 1,
        textTransform: "uppercase",
      }}>
        {isWarmup ? "WARMUP" : roundNum > 0 ? `RND ${roundNum}` : "—"}
      </span>

      <span style={{
        fontSize: compact ? 20 : 28,
        fontWeight: 900,
        fontFamily: "monospace",
        lineHeight: 1,
        color: timeColor,
        letterSpacing: "-0.02em",
        transition: "color 0.08s",
      }}>
        {fmt(time)}
      </span>

      {phaseLabel && !isWarmup && (
        <span style={{
          fontSize: 7,
          fontWeight: 900,
          letterSpacing: "0.18em",
          color: phaseLabel.color,
          lineHeight: 1,
          animation: isBomb ? "phasePulse 0.65s ease-in-out infinite alternate" : "none",
          minHeight: 9,
          textTransform: "uppercase",
        }}>
          {phaseLabel.text}
        </span>
      )}

      <style>{`
        @keyframes phasePulse { 0% { opacity:0.65; } 100% { opacity:1; } }
      `}</style>
    </div>
  );
};

/* ── Score side ──────────────────────────────────────────────────────────────── */
const ScoreSide = ({ score, color, isLocal, side, compact }) => (
  <div style={{
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    justifyContent: "center",
    padding: compact ? "4px 0" : "8px 0",
    width: compact ? 44 : 56,
    background: isLocal ? `${color}18` : "transparent",
    borderRight: side === "t" ? `1px solid ${color}18` : "none",
    borderLeft: side === "ct" ? `1px solid ${color}18` : "none",
  }}>
    {/* Team label */}
    <span style={{
      fontSize: 7,
      fontWeight: 800,
      letterSpacing: "0.14em",
      color: `${color}70`,
      textTransform: "uppercase",
      lineHeight: 1,
      marginBottom: 3,
    }}>
      {side === "t" ? "T" : "CT"}
    </span>

    <span style={{
      fontSize: compact ? 22 : 28,
      fontWeight: 900,
      fontFamily: "monospace",
      lineHeight: 1,
      color: color,
      opacity: isLocal ? 1 : 0.65,
      textShadow: isLocal ? `0 0 18px ${color}60` : "none",
    }}>
      {score}
    </span>
  </div>
);

/* ── Scoreboard ──────────────────────────────────────────────────────────────── */
const Scoreboard = ({ roundInfo, localTeam, bombData, compact = false }) => {
  const rd = roundInfo ?? {};

  const time     = rd.m_time_remaining   ?? 0;
  const isWarmup = rd.m_is_warmup        ?? false;
  const isFreeze = rd.m_is_freeze_period ?? false;
  const isBomb   = rd.m_bomb_planted     ?? false;
  const roundNum = rd.m_round_number     ?? 0;
  const tScore   = rd.m_t_score?.m_score  ?? 0;
  const ctScore  = rd.m_ct_score?.m_score ?? 0;

  const [blink, setBlink] = useState(false);
  const shouldBlink = !isWarmup && !isFreeze && time > 0 && time <= 15 && !isBomb;

  useEffect(() => {
    if (!shouldBlink) { setBlink(false); return; }
    const id = setInterval(() => setBlink(b => !b), 500);
    return () => clearInterval(id);
  }, [shouldBlink]);

  const bombActive = isBomb && bombData?.m_blow_time > 0.5 && !bombData?.m_is_defused;

  return (
    <div style={{
      display: "flex",
      alignItems: "stretch",
      borderRadius: 8,
      overflow: "hidden",
      border: bombActive
        ? "1px solid rgba(224,82,82,0.3)"
        : "1px solid var(--bg-border)",
      background: "var(--bg-panel)",
      boxShadow: bombActive ? "0 0 24px rgba(224,82,82,0.2)" : "0 4px 20px rgba(0,0,0,0.5)",
      transition: "box-shadow 0.3s, border-color 0.3s",
    }}>
      <ScoreSide score={tScore}  color={T_COLOR}  isLocal={localTeam === 2} side="t" compact={compact} />

      {bombActive
        ? <BombCentre bombData={bombData} roundNum={roundNum} compact={compact} />
        : <NormalCentre
            time={time} roundNum={roundNum}
            isWarmup={isWarmup} isFreeze={isFreeze} isBomb={isBomb}
            shouldBlink={shouldBlink} blink={blink} compact={compact}
          />
      }

      <ScoreSide score={ctScore} color={CT_COLOR} isLocal={localTeam === 3} side="ct" compact={compact} />
    </div>
  );
};

export default Scoreboard;