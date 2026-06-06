import { useEffect, useRef, useState } from "react";
import { CRYPTO_WALLETS, SUPPORT_LINKS } from "../brand";

const menuItemStyle = {
  display: "flex",
  alignItems: "center",
  gap: 10,
  width: "100%",
  padding: "10px 14px",
  fontSize: 12,
  fontWeight: 600,
  color: "var(--text-secondary)",
  textDecoration: "none",
  background: "transparent",
  border: "none",
  cursor: "pointer",
  fontFamily: "inherit",
  textAlign: "left",
  transition: "background 0.12s, color 0.12s",
};

const SupportButton = () => {
  const [open, setOpen] = useState(false);
  const [cryptoOpen, setCryptoOpen] = useState(false);
  const [copiedId, setCopiedId] = useState(null);
  const rootRef = useRef(null);

  useEffect(() => {
    const onDocClick = (e) => {
      if (rootRef.current && !rootRef.current.contains(e.target)) {
        setOpen(false);
        setCryptoOpen(false);
      }
    };
    document.addEventListener("mousedown", onDocClick);
    return () => document.removeEventListener("mousedown", onDocClick);
  }, []);

  const copyWallet = async (id, address) => {
    try {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(address);
      } else {
        const ta = document.createElement("textarea");
        ta.value = address;
        ta.style.position = "fixed";
        ta.style.opacity = "0";
        document.body.appendChild(ta);
        ta.select();
        document.execCommand("copy");
        ta.remove();
      }
      setCopiedId(id);
      setTimeout(() => setCopiedId(null), 2000);
    } catch {
      /* ignore */
    }
  };

  return (
    <div ref={rootRef} style={{ position: "relative" }}>
      <button
        type="button"
        onClick={() => {
          setOpen((v) => !v);
          if (open) setCryptoOpen(false);
        }}
        style={{
          display: "flex",
          alignItems: "center",
          gap: 6,
          padding: "6px 12px",
          borderRadius: 6,
          background: open ? "rgba(255,255,255,0.06)" : "transparent",
          border: `1px solid ${open ? "var(--bg-border)" : "var(--bg-border-dim)"}`,
          color: open ? "var(--text-primary)" : "var(--text-secondary)",
          fontSize: 12,
          fontWeight: 700,
          cursor: "pointer",
          fontFamily: "inherit",
          letterSpacing: "0.04em",
          transition: "all 0.15s",
        }}
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
        <svg width="13" height="13" viewBox="0 0 24 24" fill="#fbbf24">
          <path d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z" />
        </svg>
        Support
        <svg
          width="10"
          height="10"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          style={{
            opacity: 0.45,
            transform: open ? "rotate(180deg)" : "rotate(0deg)",
            transition: "transform 0.15s",
          }}
        >
          <path d="M19 9l-7 7-7-7" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
      </button>

      {open && (
        <div
          style={{
            position: "absolute",
            right: 0,
            top: "calc(100% + 6px)",
            width: 196,
            borderRadius: 10,
            background: "var(--bg-panel)",
            border: "1px solid var(--bg-border-dim)",
            boxShadow: "0 12px 32px rgba(0,0,0,0.45)",
            padding: "4px 0",
            zIndex: 200,
          }}
        >
          <a
            href={SUPPORT_LINKS.kofi}
            target="_blank"
            rel="noopener noreferrer"
            style={{ ...menuItemStyle, borderBottom: "1px solid var(--bg-border-dim)" }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = "rgba(255,255,255,0.04)";
              e.currentTarget.style.color = "var(--text-primary)";
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = "transparent";
              e.currentTarget.style.color = "var(--text-secondary)";
            }}
          >
            <svg width="14" height="14" viewBox="0 0 24 24" fill="#F16061">
              <path d="M23.881 8.948c-.773-4.085-4.859-4.593-4.859-4.593H.723c-.604 0-.679.798-.679.798s-.082 7.324-.022 11.822c.164 2.424 2.586 2.672 2.586 2.672s8.267-.023 11.966-.049c2.438-.426 2.683-2.566 2.658-3.734 4.352.24 7.422-2.831 6.649-6.916zm-11.062 3.511c-1.246 1.453-4.011 3.976-4.011 3.976s-.121.119-.31.023c-.076-.057-.108-.09-.108-.09-.443-.441-3.368-3.049-4.034-3.954-.709-.965-1.041-2.7-.091-3.71.951-1.01 3.005-1.086 4.363.407 0 0 1.565-1.782 3.468-.963 1.904.82 1.832 3.011.723 4.311zm6.173.478c-.928.116-1.682.028-1.682.028V7.284h1.77s1.971.551 1.971 2.638c0 1.913-.985 2.667-2.059 3.015z" />
            </svg>
            Support on Ko-fi
          </a>

          <a
            href={SUPPORT_LINKS.paypal}
            target="_blank"
            rel="noopener noreferrer"
            style={menuItemStyle}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = "rgba(255,255,255,0.04)";
              e.currentTarget.style.color = "var(--text-primary)";
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = "transparent";
              e.currentTarget.style.color = "var(--text-secondary)";
            }}
          >
            <svg width="14" height="14" viewBox="0 0 24 24" fill="#00457C">
              <path d="M20.067 8.178c-.246 4.352-3.411 6.556-7.345 6.556H9.336L7.96 21.037H2.603L5.61 4.545h7.32c3.483 0 5.488 1.432 5.488 3.633zm-4.72.015c0-1.258-.785-1.776-2.146-1.776h-2.18l-1.123 6.136h1.222c2.147 0 4.227-1.082 4.227-4.36z" />
            </svg>
            Donate via PayPal
          </a>

          <button
            type="button"
            onClick={(e) => {
              e.stopPropagation();
              setCryptoOpen((v) => !v);
            }}
            style={{
              ...menuItemStyle,
              borderTop: "1px solid var(--bg-border-dim)",
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = "rgba(255,255,255,0.04)";
              e.currentTarget.style.color = "var(--text-primary)";
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = "transparent";
              e.currentTarget.style.color = "var(--text-secondary)";
            }}
          >
            <svg width="14" height="14" viewBox="0 0 24 24" fill="#4ade80">
              <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 16c-3.31 0-6-2.69-6-6s2.69-6 6-6 6 2.69 6 6-2.69 6-6 6zm-1-9h2v2h-2V9zm0 3h2v3h-2v-3z" />
            </svg>
            <span style={{ flex: 1 }}>Crypto Donate</span>
            <svg
              width="10"
              height="10"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              style={{
                opacity: 0.45,
                transform: cryptoOpen ? "rotate(180deg)" : "rotate(0deg)",
                transition: "transform 0.15s",
              }}
            >
              <path d="M19 9l-7 7-7-7" strokeLinecap="round" strokeLinejoin="round" />
            </svg>
          </button>

          {cryptoOpen && CRYPTO_WALLETS.map((wallet) => (
            <button
              key={wallet.id}
              type="button"
              onClick={() => copyWallet(wallet.id, wallet.address)}
              style={{ ...menuItemStyle, paddingLeft: 28, fontSize: 11 }}
              onMouseEnter={(e) => {
                e.currentTarget.style.background = "rgba(255,255,255,0.04)";
                e.currentTarget.style.color = "var(--text-primary)";
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.background = "transparent";
                e.currentTarget.style.color = "var(--text-secondary)";
              }}
            >
              {copiedId === wallet.id ? `Copied ${wallet.label}!` : `Copy ${wallet.label}`}
            </button>
          ))}
        </div>
      )}
    </div>
  );
};

export default SupportButton;
