import { useEffect, useState } from "react";
import QRCode from "qrcode";
import { getShareTargets, RADAR_HTTP_PORT } from "../brand";

const triggerStyle = (active) => ({
  display: "flex",
  alignItems: "center",
  gap: 6,
  padding: "6px 12px",
  borderRadius: 6,
  background: active ? "var(--bg-card)" : "transparent",
  border: `1px solid ${active ? "var(--bg-border)" : "var(--bg-border-dim)"}`,
  color: active ? "var(--text-primary)" : "var(--text-secondary)",
  fontSize: 12,
  fontWeight: 700,
  cursor: "pointer",
  fontFamily: "inherit",
  letterSpacing: "0.04em",
  transition: "all 0.15s",
});

const copyText = async (text) => {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(text);
    return;
  }
  const ta = document.createElement("textarea");
  ta.value = text;
  ta.style.position = "fixed";
  ta.style.opacity = "0";
  document.body.appendChild(ta);
  ta.select();
  document.execCommand("copy");
  ta.remove();
};

const ShareRadarButton = ({ publicIP, label = "Share Radar", compact = false }) => {
  const [open, setOpen] = useState(false);
  const [copied, setCopied] = useState(false);
  const [qrUrl, setQrUrl] = useState("");
  const [isMobile, setIsMobile] = useState(false);

  const { publicUrl, lanUrl, primaryUrl, copyText: addressCopy } = getShareTargets(publicIP);

  useEffect(() => {
    const check = () => setIsMobile(window.innerWidth < 640);
    check();
    window.addEventListener("resize", check);
    return () => window.removeEventListener("resize", check);
  }, []);

  useEffect(() => {
    if (!open || !primaryUrl) {
      setQrUrl("");
      return;
    }
    let cancelled = false;
    QRCode.toDataURL(primaryUrl, {
      width: 220,
      margin: 1,
      color: { dark: "#07141e", light: "#ffffff" },
    })
      .then((url) => { if (!cancelled) setQrUrl(url); })
      .catch(() => { if (!cancelled) setQrUrl(""); });
    return () => { cancelled = true; };
  }, [open, primaryUrl]);

  useEffect(() => {
    if (!open) return;
    const onKey = (e) => { if (e.key === "Escape") setOpen(false); };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [open]);

  const handleCopy = async () => {
    if (!addressCopy) return;
    try {
      await copyText(addressCopy);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch {
      /* ignore */
    }
  };

  if (!primaryUrl) return null;

  return (
    <>
      <button
        type="button"
        onClick={() => setOpen(true)}
        style={triggerStyle(open)}
        onMouseEnter={(e) => {
          if (!open) {
            e.currentTarget.style.borderColor = "var(--bg-border)";
            e.currentTarget.style.color = "var(--text-primary)";
          }
        }}
        onMouseLeave={(e) => {
          if (!open) {
            e.currentTarget.style.borderColor = "var(--bg-border-dim)";
            e.currentTarget.style.color = "var(--text-secondary)";
          }
        }}
      >
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <rect x="9" y="9" width="13" height="13" rx="2" />
          <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" />
        </svg>
        {!compact && label}
      </button>

      {open && (
        <>
          <div
            onClick={() => setOpen(false)}
            style={{
              position: "fixed",
              inset: 0,
              background: "rgba(0,0,0,0.65)",
              zIndex: 500,
              backdropFilter: "blur(4px)",
            }}
          />
          <div
            role="dialog"
            aria-modal="true"
            aria-label="Share radar"
            style={{
              position: "fixed",
              zIndex: 501,
              ...(isMobile
                ? { left: 0, right: 0, bottom: 0, borderRadius: "16px 16px 0 0", maxHeight: "92vh", overflowY: "auto" }
                : { left: "50%", top: "50%", transform: "translate(-50%, -50%)", width: "min(92vw, 380px)", borderRadius: 12 }),
              background: "var(--bg-panel)",
              border: "1px solid var(--bg-border)",
              boxShadow: "0 24px 80px rgba(0,0,0,0.75)",
              padding: isMobile ? "20px 18px 28px" : "22px 22px 24px",
            }}
          >
            <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 16 }}>
              <div>
                <div style={{ fontSize: 14, fontWeight: 800, color: "var(--text-primary)", letterSpacing: "0.03em" }}>
                  Share Radar
                </div>
                <div style={{ fontSize: 11, color: "var(--text-muted)", marginTop: 4, lineHeight: 1.45 }}>
                  Scan the QR code on your phone or copy the address below.
                </div>
              </div>
              <button
                type="button"
                onClick={() => setOpen(false)}
                aria-label="Close"
                style={{
                  width: 32, height: 32, borderRadius: 8, border: "1px solid var(--bg-border-dim)",
                  background: "transparent", color: "var(--text-secondary)", cursor: "pointer",
                  display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0,
                }}
              >
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <path d="M18 6L6 18M6 6l12 12" />
                </svg>
              </button>
            </div>

            <div style={{
              display: "flex", flexDirection: "column", alignItems: "center", gap: 14,
              padding: "16px 12px", borderRadius: 10,
              background: "rgba(255,255,255,0.03)", border: "1px solid var(--bg-border-dim)",
            }}>
              {qrUrl ? (
                <img src={qrUrl} alt="Radar link QR code" width={220} height={220} style={{ borderRadius: 8, display: "block" }} />
              ) : (
                <div style={{ width: 220, height: 220, borderRadius: 8, background: "rgba(255,255,255,0.06)" }} />
              )}
              <div style={{ width: "100%", textAlign: "center" }}>
                <div style={{ fontSize: 10, fontWeight: 700, letterSpacing: "0.14em", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: 6 }}>
                  {publicUrl && primaryUrl === publicUrl ? "Public address" : "Local address"}
                </div>
                <div style={{
                  fontSize: 13, fontWeight: 700, color: "var(--text-primary)",
                  wordBreak: "break-all", lineHeight: 1.4, fontFamily: "ui-monospace, monospace",
                }}>
                  {addressCopy}
                </div>
              </div>
            </div>

            {lanUrl && publicUrl && lanUrl !== publicUrl && (
              <div style={{ marginTop: 12, fontSize: 11, color: "var(--text-muted)", lineHeight: 1.5 }}>
                Same WiFi: <span style={{ color: "var(--text-secondary)", fontFamily: "ui-monospace, monospace" }}>{lanUrl.replace(/^https?:\/\//, "")}</span>
              </div>
            )}

            <button
              type="button"
              onClick={handleCopy}
              style={{
                marginTop: 16, width: "100%", padding: "12px 14px", borderRadius: 8,
                border: `1px solid ${copied ? "var(--hp-high)" : "var(--bg-border)"}`,
                background: copied ? "rgba(34,197,94,0.12)" : "var(--bg-card)",
                color: copied ? "var(--hp-high)" : "var(--text-primary)",
                fontSize: 13, fontWeight: 700, cursor: "pointer", fontFamily: "inherit",
                display: "flex", alignItems: "center", justifyContent: "center", gap: 8,
              }}
            >
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                {copied
                  ? <path d="M20 6L9 17l-5-5" />
                  : <><rect x="9" y="9" width="13" height="13" rx="2" /><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" /></>}
              </svg>
              {copied ? "Copied!" : "Copy address"}
            </button>

            <p style={{ margin: "14px 0 0", fontSize: 10, color: "var(--text-muted)", lineHeight: 1.55, textAlign: "center" }}>
              Forward ports {RADAR_HTTP_PORT} (HTTP) and 22006 (WebSocket) on your router for access outside your network.
            </p>
          </div>
        </>
      )}
    </>
  );
};

export default ShareRadarButton;
