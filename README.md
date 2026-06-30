# AimSync WebRadar

Live Counter-Strike 2 web radar.

Reads game memory on your PC and streams match data (players, bomb, grenades, round info) to a browser-based radar you can open locally or share on your LAN.

## Project layout

| Folder | Role |
|--------|------|
| [`client/`](client/) | C++ desktop app — memory reader, launcher GUI, in-game overlay (WIP) |
| [`radar/`](radar/) | React radar UI + Node.js WebSocket relay server |

```
cs2.exe
  └─ ReadProcessMemory ─► aimsync_webradar.exe (client)
                              └─ WebSocket :22006
                                    └─ radar UI (browser :5173)
```

## Requirements

- [Node.js](https://nodejs.org/) 18+
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/) with **Desktop development with C++**
- [vcpkg](https://vcpkg.io/) (manifest mode — dependencies install on first build)
- Python 3 + Pillow (`pip install pillow`) — only for `build-aio.ps1` icon generation

## Quick start (development)

### 1. Radar UI

```bash
cd radar
npm install
npm run dev
```

Open `http://localhost:5173`

### 2. Client

Open `client/cs2_webradar.sln` in Visual Studio, build **Release | x64**, run `client/Release/aimsync_webradar.exe`.

Launcher flow: **Start radar** → wait for CS2 → **Open in browser**.

## All-in-one build

Bundles the web server into the single `.exe`:

```powershell
cd client
.\build-aio.ps1
# Rebuild Release|x64 in Visual Studio
```

Output: `client/Release/aimsync_webradar.exe`

## Map assets

All map files live in **`client/data/<map>/`** (`data.json`, `radar.png`, `background.png`).

- **Build** copies `client/data` → `client/Release/data/` (next to the exe)
- **Radar dev** runs `npm run sync:maps` to mirror into `radar/public/data/`
- **AIO build** (`client/build-aio.ps1`) syncs maps into the web bundle before packaging

Runtime copies from bundled `data/` into `%APPDATA%\AimSync\data\` when needed.

## Configuration

Stored in `%APPDATA%\AimSync\`:

| File | Purpose |
|------|---------|
| `config.json` | WebSocket IP, offsets, settings |
| `overlay.json` | Overlay position/size (WIP feature) |
| `data/<map>/` | Cached map assets (synced from `client/data`) |

## Sharing with friends

- **Share with friends** in the launcher copies `your_public_ip:5173`
- Forward **5173/tcp** (HTTP) and **22006/tcp** (WebSocket) on your router

## Overlay (work in progress)

The in-game overlay is experimental. Launcher button is marked **WIP**.

| Key | Action |
|-----|--------|
| F8 | Show / hide overlay |
| DEL / F9 | Settings mode — drag radar/list, toggles, sliders |
| M | Toggle full-map box (settings mode) |

## License

Copyright © AimSync. All rights reserved.
