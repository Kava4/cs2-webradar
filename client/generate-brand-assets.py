"""Generate radar icons, favicons, and app.ico from client/logo.png."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
LOGO = ROOT / "logo.png"
RADAR_ICONS = ROOT.parent / "radar" / "public" / "icons"
RADAR_LOGO = ROOT.parent / "radar" / "public" / "logo.png"
APP_ICO = ROOT / "app.ico"

WEB_SIZES = {
    "favicon-16x16.png": 16,
    "favicon-32x32.png": 32,
    "icon-72x72.png": 72,
    "icon-96x96.png": 96,
    "icon-128x128.png": 128,
    "icon-144x144.png": 144,
    "icon-152x152.png": 152,
    "icon-192x192.png": 192,
    "icon-384x384.png": 384,
    "icon-512x512.png": 512,
}

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def main() -> None:
    if not LOGO.exists():
        raise SystemExit(f"Missing logo: {LOGO}")

    img = Image.open(LOGO).convert("RGBA")
    RADAR_ICONS.mkdir(parents=True, exist_ok=True)

    for name, size in WEB_SIZES.items():
        resized = img.resize((size, size), Image.Resampling.LANCZOS)
        resized.save(RADAR_ICONS / name)

    img.resize((512, 512), Image.Resampling.LANCZOS).save(RADAR_LOGO)

    ico_images = [img.resize((s, s), Image.Resampling.LANCZOS) for s in ICO_SIZES]
    ico_images[0].save(APP_ICO, format="ICO", sizes=[(s, s) for s in ICO_SIZES])

    print(f"Wrote icons to {RADAR_ICONS}")
    print(f"Wrote {RADAR_LOGO}")
    print(f"Wrote {APP_ICO}")


if __name__ == "__main__":
    main()
