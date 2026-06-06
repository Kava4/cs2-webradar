import { useState, useEffect } from "react";

const SectionLabel = ({ children }) => (
  <div style={{
    fontSize: 9, fontWeight: 800, letterSpacing: "0.2em",
    color: "var(--text-muted)", textTransform: "uppercase",
    marginBottom: 2, marginTop: 4,
  }}>
    {children}
  </div>
);

const Toggle = ({ label, desc, checked, onChange }) => (
  <label style={{
    display: "flex", alignItems: "center", justifyContent: "space-between",
    padding: "7px 0", cursor: "pointer", gap: 10,
  }}>
    <div style={{ flex: 1, minWidth: 0 }}>
      <div style={{ fontSize: 12, fontWeight: 600, color: "var(--text-secondary)", lineHeight: 1.2 }}>{label}</div>
      {desc && <div style={{ fontSize: 9, color: "var(--text-muted)", marginTop: 2, lineHeight: 1.3 }}>{desc}</div>}
    </div>
    <div style={{ position: "relative", flexShrink: 0 }}>
      <input type="checkbox" checked={checked} onChange={onChange} style={{ position: "absolute", opacity: 0, width: 0, height: 0 }} />
      <div style={{
        width: 34, height: 18, borderRadius: 9,
        background: checked ? "var(--ct-color)" : "var(--bg-border)",
        transition: "background 0.15s", position: "relative",
      }}>
        <div style={{
          position: "absolute", top: 2, left: checked ? 14 : 2,
          width: 14, height: 14, borderRadius: "50%",
          background: checked ? "white" : "rgba(255,255,255,0.3)",
          transition: "left 0.15s", boxShadow: "0 1px 3px rgba(0,0,0,0.4)",
        }} />
      </div>
    </div>
  </label>
);

const Slider = ({ label, value, min, max, step, onChange, color, format }) => {
  const pct = ((value - min) / (max - min)) * 100;
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 7 }}>
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
        <span style={{ fontSize: 12, fontWeight: 600, color: "var(--text-secondary)" }}>{label}</span>
        <span style={{ fontSize: 11, fontFamily: "monospace", fontWeight: 700, color }}>{format ? format(value) : value}</span>
      </div>
      <div style={{ position: "relative", height: 4, borderRadius: 2, background: "var(--bg-border)" }}>
        <div style={{ position: "absolute", left: 0, top: 0, height: "100%", width: `${pct}%`, borderRadius: 2, background: color, transition: "width 0.1s" }} />
        <input type="range" min={min} max={max} step={step} value={value} onChange={onChange}
          style={{ position: "absolute", inset: "-7px 0", width: "100%", opacity: 0, cursor: "pointer" }} />
      </div>
    </div>
  );
};

const Divider = () => (
  <div style={{ height: 1, background: "var(--bg-border-dim)", margin: "8px 0" }} />
);

const SettingsButton = ({ settings, onSettingsChange }) => {
  const [isOpen, setIsOpen] = useState(false);
  const [isMobile, setIsMobile] = useState(false);

  useEffect(() => {
    const check = () => setIsMobile(window.innerWidth < 640);
    check();
    window.addEventListener("resize", check);
    return () => window.removeEventListener("resize", check);
  }, []);

  const upd = (key, val) => onSettingsChange({ ...settings, [key]: val });

  return (
    <div style={{ position: "relative", zIndex: 50 }}>
      {/* Trigger */}
      <button
        onClick={() => setIsOpen(o => !o)}
        style={{
          display: "flex", alignItems: "center", gap: 6, padding: "6px 12px",
          borderRadius: 6, fontFamily: "inherit", fontSize: 12, fontWeight: 700,
          letterSpacing: "0.04em", cursor: "pointer", transition: "all 0.15s",
          background: isOpen ? "var(--bg-card)" : "transparent",
          border: `1px solid ${isOpen ? "var(--bg-border)" : "var(--bg-border-dim)"}`,
          color: isOpen ? "var(--text-primary)" : "var(--text-secondary)",
        }}
        onMouseEnter={e => { e.currentTarget.style.borderColor = "var(--bg-border)"; e.currentTarget.style.color = "var(--text-primary)"; }}
        onMouseLeave={e => { if (!isOpen) { e.currentTarget.style.borderColor = "var(--bg-border-dim)"; e.currentTarget.style.color = "var(--text-secondary)"; }}}
      >
        <svg class="w-3 h-3 text-gray-800 dark:text-white" aria-hidden="true" xmlns="http://www.w3.org/2000/svg" width="24" height="24" fill="none" viewBox="0 0 24 24">
        <path stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 13v-2a1 1 0 0 0-1-1h-.757l-.707-1.707.535-.536a1 1 0 0 0 0-1.414l-1.414-1.414a1 1 0 0 0-1.414 0l-.536.535L14 4.757V4a1 1 0 0 0-1-1h-2a1 1 0 0 0-1 1v.757l-1.707.707-.536-.535a1 1 0 0 0-1.414 0L4.929 6.343a1 1 0 0 0 0 1.414l.536.536L4.757 10H4a1 1 0 0 0-1 1v2a1 1 0 0 0 1 1h.757l.707 1.707-.535.536a1 1 0 0 0 0 1.414l1.414 1.414a1 1 0 0 0 1.414 0l.536-.535 1.707.707V20a1 1 0 0 0 1 1h2a1 1 0 0 0 1-1v-.757l1.707-.708.536.536a1 1 0 0 0 1.414 0l1.414-1.414a1 1 0 0 0 0-1.414l-.535-.536.707-1.707H20a1 1 0 0 0 1-1Z"/>
        <path stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z"/>
        </svg>

        <span style={{ display: isMobile ? "none" : "inline" }}>Settings</span>
      </button>

      {isOpen && (
        <>
          {isMobile && (
            <div onClick={() => setIsOpen(false)} style={{ position: "fixed", inset: 0, background: "rgba(0,0,0,0.65)", zIndex: 40, backdropFilter: "blur(4px)" }} />
          )}
          <div style={{
            position: isMobile ? "fixed" : "absolute",
            ...(isMobile
              ? { bottom: 0, left: 0, right: 0, borderRadius: "12px 12px 0 0", maxHeight: "90vh", overflowY: "auto" }
              : { right: 0, top: "calc(100% + 6px)", width: 300, borderRadius: 8, maxHeight: "85vh", overflowY: "auto" }
            ),
            background: "var(--bg-panel)",
            border: "1px solid var(--bg-border)",
            boxShadow: "0 20px 60px rgba(0,0,0,0.75)",
            padding: "16px 18px 20px",
            zIndex: 50,
          }}>
            {/* Header */}
            <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 16 }}>
              <span style={{ fontSize: 10, fontWeight: 800, letterSpacing: "0.2em", color: "var(--text-secondary)", textTransform: "uppercase" }}>
                Radar Settings
              </span>
              <button onClick={() => setIsOpen(false)} style={{ background: "none", border: "none", color: "var(--text-muted)", cursor: "pointer", padding: 2, lineHeight: 1 }}>
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M18 6L6 18M6 6l12 12"/></svg>
              </button>
            </div>

            {/* ── DISPLAY ── */}
            <SectionLabel>Display</SectionLabel>
            <Divider />
            <div style={{ display: "flex", flexDirection: "column" }}>
              <Toggle label="Ally Names"    checked={settings.showAllNames   ?? true}  onChange={e => upd("showAllNames",    e.target.checked)} />
              <Toggle label="Enemy Names"   checked={settings.showEnemyNames ?? false} onChange={e => upd("showEnemyNames",  e.target.checked)} />
              <Toggle label="View Cones"    checked={settings.showViewCones  ?? true}  onChange={e => upd("showViewCones",   e.target.checked)} desc="FOV cone per player" />
              <Toggle label="Health Bars"   checked={settings.showHealthBars ?? true}  onChange={e => upd("showHealthBars",  e.target.checked)} desc="HP bar at bottom of card" />
              <Toggle label="Ammo Counter"  checked={settings.showAmmo       ?? true}  onChange={e => upd("showAmmo",        e.target.checked)} desc="Ammo pips on player cards" />
              <Toggle label="Armor"         checked={settings.showArmor      ?? true}  onChange={e => upd("showArmor",       e.target.checked)} desc="Armor value on cards" />
              <Toggle label="Ping"          checked={settings.showPing       ?? true}  onChange={e => upd("showPing",        e.target.checked)} desc="Latency on player cards" />
              <Toggle label="Money"         checked={settings.showMoney      ?? true}  onChange={e => upd("showMoney",       e.target.checked)} desc="$ economy on player cards" />
              <Toggle label="Flash Effect"  checked={settings.showFlashEffect?? true}  onChange={e => upd("showFlashEffect", e.target.checked)} desc="White overlay when flashed" />
            </div>

            <Divider />

            {/* ── PLAYER CARDS (NEW) ── */}
            <SectionLabel>Player Cards</SectionLabel>
            <Divider />
            <div style={{ display: "flex", flexDirection: "column" }}>
              <Toggle
                label="Show Scoreboard"
                desc="Score / timer / bomb timer bar"
                checked={settings.showScoreboard !== false}
                onChange={e => upd("showScoreboard", e.target.checked)}
              />
              <Toggle
                label="Show T Cards"
                desc="Terrorist player cards panel"
                checked={settings.showTCards !== false}
                onChange={e => upd("showTCards", e.target.checked)}
              />
              <Toggle
                label="Show CT Cards"
                desc="Counter-Terrorist player cards panel"
                checked={settings.showCTCards !== false}
                onChange={e => upd("showCTCards", e.target.checked)}
              />
            </div>

            <Divider />

            {/* ── MAP ── */}
            <SectionLabel>Map</SectionLabel>
            <Divider />
            <div style={{ display: "flex", flexDirection: "column" }}>
              <Toggle label="Grenades"       checked={settings.showGrenades       ?? true} onChange={e => upd("showGrenades",       e.target.checked)} desc="Flash, HE, decoy projectiles" />
              <Toggle label="Smoke Overlay"  checked={settings.showSmoke          ?? true} onChange={e => upd("showSmoke",          e.target.checked)} desc="Deployed smoke cloud area" />
              <Toggle label="Fire Overlay"   checked={settings.showFire           ?? true} onChange={e => upd("showFire",           e.target.checked)} desc="Molotov / incendiary area" />
              <Toggle label="Grenade Labels" checked={settings.showGrenadeLabels  ?? true} onChange={e => upd("showGrenadeLabels",  e.target.checked)} desc="SMK / HE / FLB type labels" />
              <Toggle label="Bomb Timer"     checked={settings.showBombTimer      ?? true} onChange={e => upd("showBombTimer",      e.target.checked)} desc="Countdown when bomb planted" />
              <Toggle label="Kill Feed"      checked={settings.showKillFeed       ?? true} onChange={e => upd("showKillFeed",       e.target.checked)} desc="Kill events corner" />
            </div>

            <Divider />

            {/* ── SIZES ── */}
            <SectionLabel>Sizes</SectionLabel>
            <Divider />
            <div style={{ display: "flex", flexDirection: "column", gap: 16, paddingTop: 4 }}>
              <Slider label="Player Size" value={settings.dotSize ?? 1} min={0.5} max={2.5} step={0.1}
                onChange={e => upd("dotSize", parseFloat(e.target.value))}
                color="var(--t-color)" format={v => `${v.toFixed(1)}×`} />
              <Slider label="Bomb Size" value={settings.bombSize ?? 1} min={0.1} max={2} step={0.1}
                onChange={e => upd("bombSize", parseFloat(e.target.value))}
                color="var(--hp-low)" format={v => `${v.toFixed(1)}×`} />
              <Slider label="View Cone Length" value={settings.coneLength ?? 1} min={0.5} max={3} step={0.1}
                onChange={e => upd("coneLength", parseFloat(e.target.value))}
                color="var(--ct-color)" format={v => `${v.toFixed(1)}×`} />
            </div>

            {isMobile && (
              <button onClick={() => setIsOpen(false)} style={{
                display: "block", width: "100%", marginTop: 20, padding: "10px",
                borderRadius: 6, background: "var(--ct-color)", border: "none",
                color: "white", fontSize: 12, fontWeight: 800, letterSpacing: "0.1em",
                textTransform: "uppercase", cursor: "pointer", fontFamily: "inherit",
              }}>
                Done
              </button>
            )}
          </div>
        </>
      )}
    </div>
  );
};

export default SettingsButton;