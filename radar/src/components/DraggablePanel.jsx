import { useRef, useState, useEffect, useCallback } from "react";

/**
 * DraggablePanel
 * Wraps any content in a panel that can be:
 *  - Dragged by its header bar
 *  - Resized from the bottom-right corner handle
 *  - Collapsed by double-clicking the header
 *  - Fully hidden via the `visible` prop
 *
 * Props:
 *  title       string   — header label
 *  defaultPos  {x, y}   — initial position in px (default: {x:0, y:0})
 *  defaultSize {w, h}   — initial size in px (default: {w:300, h:400})
 *  minSize     {w, h}   — minimum resize limit
 *  visible     bool     — hide/show the whole panel
 *  storageKey  string   — localStorage key to persist position+size (optional)
 *  children    node
 *  style       object   — extra styles for the outer wrapper
 *  headerRight node     — extra content rendered in the header right side
 */
const DraggablePanel = ({
  title,
  defaultPos   = { x: 20, y: 20 },
  defaultSize  = { w: 300, h: 400 },
  minSize      = { w: 180, h: 120 },
  visible      = true,
  storageKey   = null,
  children,
  style        = {},
  headerRight  = null,
}) => {
  // ── Restore saved state from localStorage if a key is given ──────────────
  const saved = storageKey ? (() => {
    try { return JSON.parse(localStorage.getItem(storageKey)); } catch { return null; }
  })() : null;

  const [pos,       setPos]       = useState(saved?.pos  ?? defaultPos);
  const [size,      setSize]      = useState(saved?.size ?? defaultSize);
  const [collapsed, setCollapsed] = useState(false);

  // Persist changes
  useEffect(() => {
    if (!storageKey) return;
    localStorage.setItem(storageKey, JSON.stringify({ pos, size }));
  }, [pos, size, storageKey]);

  const panelRef  = useRef();
  const dragState = useRef(null);  // { startMouseX, startMouseY, startPosX, startPosY }
  const resState  = useRef(null);  // { startMouseX, startMouseY, startW, startH }

  // ── Drag (move) ───────────────────────────────────────────────────────────
  const onHeaderMouseDown = useCallback((e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    dragState.current = {
      startMouseX: e.clientX,
      startMouseY: e.clientY,
      startPosX:   pos.x,
      startPosY:   pos.y,
    };

    const onMove = (ev) => {
      const d = dragState.current;
      if (!d) return;
      setPos({
        x: d.startPosX + (ev.clientX - d.startMouseX),
        y: d.startPosY + (ev.clientY - d.startMouseY),
      });
    };
    const onUp = () => {
      dragState.current = null;
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup",   onUp);
    };
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup",   onUp);
  }, [pos]);

  // ── Resize (bottom-right handle) ─────────────────────────────────────────
  const onResizeMouseDown = useCallback((e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();
    resState.current = {
      startMouseX: e.clientX,
      startMouseY: e.clientY,
      startW:      size.w,
      startH:      size.h,
    };

    const onMove = (ev) => {
      const r = resState.current;
      if (!r) return;
      setSize({
        w: Math.max(minSize.w, r.startW + (ev.clientX - r.startMouseX)),
        h: Math.max(minSize.h, r.startH + (ev.clientY - r.startMouseY)),
      });
    };
    const onUp = () => {
      resState.current = null;
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup",   onUp);
    };
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup",   onUp);
  }, [size, minSize]);

  if (!visible) return null;

  const bodyH = collapsed ? 0 : size.h - 32; // 32px = header height

  return (
    <div
      ref={panelRef}
      style={{
        position:    "fixed",
        left:        pos.x,
        top:         pos.y,
        width:       size.w,
        zIndex:      200,
        userSelect:  "none",
        background:  "rgba(8,11,15,0.88)",
        border:      "1px solid rgba(30,39,54,0.9)",
        borderRadius: 8,
        boxShadow:   "0 8px 32px rgba(0,0,0,0.7)",
        backdropFilter: "blur(6px)",
        overflow:    "hidden",
        ...style,
      }}
    >
      {/* ── Header ── */}
      <div
        onMouseDown={onHeaderMouseDown}
        onDoubleClick={() => setCollapsed(c => !c)}
        style={{
          height:      32,
          display:     "flex",
          alignItems:  "center",
          justifyContent: "space-between",
          padding:     "0 10px",
          cursor:      "grab",
          background:  "rgba(13,17,23,0.95)",
          borderBottom: collapsed ? "none" : "1px solid rgba(30,39,54,0.7)",
          flexShrink:  0,
        }}
      >
        <span style={{
          fontSize:      10,
          fontWeight:    800,
          letterSpacing: "0.18em",
          textTransform: "uppercase",
          color:         "rgba(90,106,126,0.9)",
          lineHeight:    1,
        }}>
          {title}
        </span>
        <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
          {headerRight}
          {/* Collapse toggle */}
          <button
            onMouseDown={e => e.stopPropagation()}
            onClick={() => setCollapsed(c => !c)}
            style={{
              background: "none", border: "none", cursor: "pointer",
              color: "rgba(90,106,126,0.7)", fontSize: 12, lineHeight: 1,
              padding: "0 2px",
            }}
            title={collapsed ? "Expand" : "Collapse"}
          >
            {collapsed ? "▲" : "▼"}
          </button>
        </div>
      </div>

      {/* ── Body ── */}
      <div style={{
        height:   bodyH,
        overflow: "hidden auto",
        transition: "height 0.15s ease",
      }}>
        {children}
      </div>

      {/* ── Resize handle ── */}
      {!collapsed && (
        <div
          onMouseDown={onResizeMouseDown}
          style={{
            position: "absolute", bottom: 0, right: 0,
            width: 14, height: 14,
            cursor: "nwse-resize",
            zIndex: 10,
            background:
              "linear-gradient(135deg, transparent 50%, rgba(90,106,126,0.4) 50%)",
          }}
        />
      )}
    </div>
  );
};

export default DraggablePanel;