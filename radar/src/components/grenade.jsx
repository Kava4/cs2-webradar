import { useRef } from "react";
import { getRadarPosition, getEntityTranslate } from "../utilities/utilities";

const radiusToPx = (gameUnits, imgWidth, scale) => {
  if (!imgWidth || !scale) return 0;
  return (gameUnits / scale / 1024) * imgWidth * 2;
};

// ─── SMOKE — CS2-style soft cloud (no hard ring, no label) ─────────────────
const SmokeEffect = ({ diameter }) => (
  <>
    <style>{`
      @keyframes smokePulse  { 0%,100%{opacity:.9;transform:scale(1)} 50%{opacity:1;transform:scale(1.02)} }
      @keyframes smokeDrift1 { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(4px,-3px)scale(1.04)} }
      @keyframes smokeDrift2 { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(-3px,4px)scale(1.03)} }
      @keyframes smokeDrift3 { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(2px,2px)scale(1.02)} }
    `}</style>
    <div style={{
      position: "absolute", width: diameter, height: diameter,
      left: "50%", top: "50%",
      transform: "translate(-50%, -50%)",
      pointerEvents: "none", zIndex: 1,
      filter: "blur(0.6px)",
    }}>
      <div style={{
        position: "absolute", inset: "-6%",
        borderRadius: "50%",
        background: `
          radial-gradient(ellipse 58% 52% at 44% 40%, rgba(228,235,242,0.94) 0%, transparent 72%),
          radial-gradient(ellipse 52% 58% at 64% 58%, rgba(188,200,212,0.82) 0%, transparent 74%),
          radial-gradient(ellipse 48% 50% at 28% 66%, rgba(170,184,196,0.76) 0%, transparent 70%),
          radial-gradient(ellipse 62% 62% at 50% 50%, rgba(155,168,180,0.62) 0%, rgba(135,148,160,0.28) 58%, transparent 88%)
        `,
        animation: "smokePulse 5s ease-in-out infinite",
      }} />
      <div style={{
        position: "absolute", width: "46%", height: "46%", top: "14%", left: "22%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(238,244,250,0.82) 0%, transparent 72%)",
        animation: "smokeDrift1 6s ease-in-out infinite",
      }} />
      <div style={{
        position: "absolute", width: "50%", height: "50%", bottom: "8%", right: "10%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(200,212,222,0.72) 0%, transparent 70%)",
        animation: "smokeDrift2 7s ease-in-out infinite",
      }} />
      <div style={{
        position: "absolute", width: "40%", height: "40%", top: "30%", right: "8%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(192,204,214,0.65) 0%, transparent 68%)",
        animation: "smokeDrift3 5.5s ease-in-out infinite",
      }} />
    </div>
  </>
);

// ─── FIRE — realistic layered flame area ─────────────────────────────────────
const FireEffect = ({ diameter }) => {
  const r = diameter / 2;
  return (
    <>
      <style>{`
        @keyframes fireGlow     { from{opacity:.75;transform:scale(0.97)} to{opacity:1;transform:scale(1.03)} }
        @keyframes fireFlicker  { from{transform:scale(0.91)rotate(-3deg);opacity:.78} to{transform:scale(1.07)rotate(3deg);opacity:1} }
        @keyframes flameTongue1 { from{transform:rotate(-10deg)scaleY(0.85)} to{transform:rotate(7deg)scaleY(1.15)} }
        @keyframes flameTongue2 { from{transform:rotate(8deg)scaleY(0.88)} to{transform:rotate(-6deg)scaleY(1.12)} }
        @keyframes flameTongue3 { from{transform:rotate(-5deg)scaleY(0.80)} to{transform:rotate(5deg)scaleY(1.20)} }
        @keyframes ember        { 0%{transform:translateY(0)scale(1);opacity:1} 80%{transform:translateY(-22px)scale(0.4);opacity:.5} 100%{transform:translateY(-28px)scale(0);opacity:0} }
        @keyframes fireRing     { 0%{transform:translate(-50%,-50%)scale(1);opacity:.6} 100%{transform:translate(-50%,-50%)scale(1.5);opacity:0} }
      `}</style>
      <div style={{
        position: "absolute", width: diameter, height: diameter,
        left: "50%", top: "50%",
        transform: "translate(-50%, -50%)",
        borderRadius: "50%", overflow: "visible", zIndex: 1,
      }}>
        {/* Expanding ring pulse */}
        <div style={{
          position: "absolute", width: diameter, height: diameter,
          left: "50%", top: "50%",
          borderRadius: "50%",
          border: "2px solid rgba(255,140,0,0.6)",
          animation: "fireRing 1.2s ease-out infinite",
        }} />
        {/* Outer glow */}
        <div style={{
          position: "absolute", inset: 0, borderRadius: "50%",
          background: "radial-gradient(circle, rgba(255,130,0,0.55) 0%, rgba(220,65,0,0.35) 45%, rgba(180,30,0,0.15) 70%, transparent 90%)",
          boxShadow: `0 0 ${r * 0.8}px ${r * 0.4}px rgba(255,100,0,0.35), 0 0 ${r * 1.5}px rgba(255,50,0,0.15)`,
          animation: "fireGlow 1.2s ease-in-out infinite alternate",
        }} />
        {/* Flame core */}
        <div style={{
          position: "absolute", inset: "10%", borderRadius: "50%",
          background: "radial-gradient(ellipse at 50% 65%, rgba(255,220,60,0.90) 0%, rgba(255,140,0,0.75) 30%, rgba(220,60,0,0.50) 60%, transparent 100%)",
          animation: "fireFlicker 0.8s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 1 */}
        <div style={{
          position: "absolute", width: "32%", height: "55%", bottom: "22%", left: "18%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,220,50,0.90), rgba(255,130,0,0.55), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue1 1s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 2 */}
        <div style={{
          position: "absolute", width: "30%", height: "48%", bottom: "22%", right: "16%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,200,40,0.85), rgba(255,110,0,0.45), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue2 1.25s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 3 center */}
        <div style={{
          position: "absolute", width: "24%", height: "62%", bottom: "22%", left: "38%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,240,90,0.95), rgba(255,160,20,0.60), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue3 0.75s ease-in-out infinite alternate",
        }} />
        {/* Ground char circle */}
        <div style={{
          position: "absolute", inset: "20%", borderRadius: "50%",
          background: "radial-gradient(circle, rgba(30,10,0,0.45) 0%, transparent 70%)",
        }} />
        {/* Embers */}
        {[
          { left: "28%", delay: "0s",   dur: "1.6s" },
          { left: "52%", delay: "0.3s", dur: "1.9s" },
          { left: "42%", delay: "0.6s", dur: "1.4s" },
          { left: "63%", delay: "1.0s", dur: "2.2s" },
          { left: "35%", delay: "1.4s", dur: "1.7s" },
        ].map((e, i) => (
          <div key={i} style={{
            position: "absolute", width: 3, height: 3, borderRadius: "50%",
            background: "rgba(255,230,80,0.95)", bottom: "28%", left: e.left,
            boxShadow: "0 0 5px 2px rgba(255,170,0,0.7)",
            animation: `ember ${e.dur} ${e.delay} ease-out infinite`,
          }} />
        ))}
        {/* FIRE label */}
        <div style={{
          position: "absolute", top: "50%", left: "50%",
          transform: "translate(-50%, -50%)",
          fontSize: Math.max(8, diameter * 0.11),
          fontWeight: 900,
          color: "rgba(255,240,180,0.95)",
          textShadow: "0 0 6px rgba(255,100,0,1), 0 1px 3px rgba(0,0,0,0.8)",
          letterSpacing: "0.06em",
          lineHeight: 1,
          pointerEvents: "none",
          zIndex: 3,
        }}>FIRE</div>
      </div>
    </>
  );
};

// ─── Config per type ──────────────────────────────────────────────────────────
const CFG = {
  smoke:      { dotColor: "#94a3b8", border: "rgba(148,163,184,0.7)", auraRadius: 144, label: "SMK" },
  flashbang:  { dotColor: "#fef08a", border: "rgba(254,240,138,0.8)", auraRadius: 0,   label: "FLB" },
  he:         { dotColor: "#fb923c", border: "rgba(251,146,60,0.8)",  auraRadius: 0,   label: "HE"  },
  molotov:    { dotColor: "#ef4444", border: "rgba(239,68,68,0.8)",   auraRadius: 150, label: "MOL" },
  incendiary: { dotColor: "#ef4444", border: "rgba(239,68,68,0.8)",   auraRadius: 150, label: "INC" },
  decoy:      { dotColor: "#a78bfa", border: "rgba(167,139,250,0.8)", auraRadius: 0,   label: "DCY" },
  inferno:    { dotColor: "#f97316", border: "rgba(249,115,22,0.9)",  auraRadius: 150, label: "FIRE"},
};

const Grenade = ({ grenadeData, mapData, mapWidth = 0, mapHeight = 0, settings }) => {
  const dotRef = useRef();

  if (!grenadeData || !mapWidth || !mapHeight || !mapData) return null;
  if (settings && !settings.showGrenades) return null;

  const type = grenadeData.type;
  const cfg  = CFG[type];
  if (!cfg) return null;

  const radarPos = getRadarPosition(mapData, { x: grenadeData.x, y: grenadeData.y });
  if (!radarPos || (radarPos.x <= 0 && radarPos.y <= 0)) return null;

  const dotSize = Math.max(12, mapWidth * 0.013);
  const { x: tx, y: ty } = getEntityTranslate(mapWidth, mapHeight, radarPos, dotSize, dotSize);

  const diamPx = radiusToPx(cfg.auraRadius, mapWidth, mapData.scale);

  const smokeDeployed = type === "smoke"   && grenadeData.m_did_smoke_effect;
  const showFire      = type === "inferno" && (settings?.showFire !== false);
  const showSmoke     = smokeDeployed      && (settings?.showSmoke !== false);
  const showDot       = !smokeDeployed;
  const isDecoyActive = type === "decoy"   && grenadeData.m_is_active;
  const showLabels    = settings?.showGrenadeLabels !== false;

  return (
    <div
      ref={dotRef}
      className="absolute left-0 top-0"
      style={{ transform: `translate(${tx}px, ${ty}px)`, zIndex: 120, pointerEvents: "none", width: dotSize, height: dotSize }}
    >
      {showSmoke && diamPx > 0 && <SmokeEffect diameter={diamPx} />}
      {showFire  && diamPx > 0 && <FireEffect  diameter={diamPx} />}

      {/* Decoy ping ring */}
      {isDecoyActive && (
        <>
          <style>{`@keyframes decoyPing{0%{transform:translate(-50%,-50%)scale(1);opacity:.8}100%{transform:translate(-50%,-50%)scale(2.5);opacity:0}}`}</style>
          <div style={{
            position: "absolute", width: 32, height: 32, left: "50%", top: "50%",
            transform: "translate(-50%, -50%)", borderRadius: "50%",
            border: `2px solid ${cfg.border}`,
            animation: "decoyPing 1.2s ease-out infinite",
          }} />
        </>
      )}

      {/* Projectile dot */}
      {showDot && (
        <div style={{
          width: dotSize, height: dotSize, borderRadius: "50%",
          background: cfg.dotColor, border: `2px solid ${cfg.border}`,
          boxShadow: `0 0 8px ${cfg.dotColor}, 0 0 3px rgba(0,0,0,0.9)`,
          position: "relative", zIndex: 2,
        }} />
      )}

      {/* Label */}
      {showDot && showLabels && (
        <div style={{
          position: "absolute", top: 16, left: "50%", transform: "translateX(-50%)",
          fontSize: 8, fontWeight: 900, color: cfg.dotColor,
          textShadow: "0 1px 4px rgba(0,0,0,1)", whiteSpace: "nowrap",
          letterSpacing: "0.04em", lineHeight: 1,
        }}>
          {cfg.label}
        </div>
      )}
    </div>
  );
};

export default Grenade;