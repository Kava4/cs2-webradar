import { useEffect, useRef, useState } from "react";

const WeaponIcon = ({ weapon, size = 12 }) =>
  weapon ? (
    <div style={{
      width: size * 2.4,
      height: size,
      background: "rgba(255,255,255,0.4)",
      WebkitMask: `url('./assets/icons/${weapon}.svg') no-repeat center / contain`,
      flexShrink: 0,
    }} />
  ) : null;

const T_COLOR  = "#f5a623";
const CT_COLOR = "#4a9eff";

const FeedEntry = ({ entry }) => {
  const killerColor = entry.killerTeam === 2 ? T_COLOR : CT_COLOR;
  const victimColor = entry.victimTeam  === 2 ? T_COLOR : CT_COLOR;

  return (
    <div style={{
      display: "flex",
      alignItems: "center",
      gap: 6,
      padding: "5px 8px",
      borderRadius: 4,
      background: "rgba(8,11,15,0.82)",
      border: "1px solid rgba(255,255,255,0.06)",
      backdropFilter: "blur(8px)",
      animation: "feedIn 0.2s ease-out",
    }}>
      <span style={{
        fontSize: 11,
        fontWeight: 800,
        color: killerColor,
        maxWidth: 80,
        overflow: "hidden",
        textOverflow: "ellipsis",
        whiteSpace: "nowrap",
      }}>
        {entry.killerName}
      </span>

      <WeaponIcon weapon={entry.weapon} size={10} />

      {entry.headshot && (
        <span style={{
          fontSize: 8,
          fontWeight: 800,
          color: "#f0b429",
          letterSpacing: "0.04em",
        }}>
          HS
        </span>
      )}

      <span style={{
        fontSize: 11,
        fontWeight: 800,
        color: victimColor,
        opacity: 0.45,
        maxWidth: 80,
        overflow: "hidden",
        textOverflow: "ellipsis",
        whiteSpace: "nowrap",
        textDecoration: "line-through",
      }}>
        {entry.victimName}
      </span>
    </div>
  );
};

const MAX_ENTRIES = 5;
const ENTRY_TTL   = 6000;

const KillFeed = ({ playerArray }) => {
  const [entries, setEntries] = useState([]);
  const prevRef = useRef({});

  useEffect(() => {
    if (!playerArray || playerArray.length === 0) return;
    const prev = prevRef.current;
    const newEntries = [];

    playerArray.forEach((p) => {
      const prevP = prev[p.m_idx];
      if (!prevP) return;
      if (p.m_kills > prevP.m_kills) {
        const delta = p.m_kills - prevP.m_kills;
        for (let i = 0; i < delta; i++) {
          newEntries.push({
            id: `${Date.now()}-${p.m_idx}-k${i}`,
            killerName: p.m_name,
            killerTeam: p.m_team,
            victimName: "?",
            victimTeam: p.m_team === 2 ? 3 : 2,
            weapon: p.m_weapons?.m_active ?? null,
            headshot: false,
            ts: Date.now(),
          });
        }
      }
      if (p.m_deaths > prevP.m_deaths) {
        const match = newEntries.findLast?.(e => e.victimName === "?" && e.victimTeam === p.m_team) ?? null;
        if (match) { match.victimName = p.m_name; match.victimTeam = p.m_team; }
      }
    });

    playerArray.forEach(p => {
      prev[p.m_idx] = { kills: p.m_kills ?? 0, deaths: p.m_deaths ?? 0, name: p.m_name, team: p.m_team };
    });
    prevRef.current = prev;

    if (newEntries.length === 0) return;
    setEntries(old => [...newEntries, ...old].slice(0, MAX_ENTRIES));
  }, [playerArray]);

  useEffect(() => {
    const id = setInterval(() => {
      const now = Date.now();
      setEntries(old => old.filter(e => now - e.ts < ENTRY_TTL));
    }, 1000);
    return () => clearInterval(id);
  }, []);

  if (entries.length === 0) return null;

  return (
    <>
      <style>{`
        @keyframes feedIn {
          from { opacity:0; transform:translateX(10px) scale(0.97); }
          to   { opacity:1; transform:translateX(0)    scale(1);    }
        }
      `}</style>
      <div style={{
        display: "flex",
        flexDirection: "column",
        gap: 3,
        pointerEvents: "none",
        userSelect: "none",
        minWidth: 200,
        maxWidth: 280,
      }}>
        {entries.map(e => <FeedEntry key={e.id} entry={e} />)}
      </div>
    </>
  );
};

export default KillFeed;