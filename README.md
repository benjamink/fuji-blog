# FujiBlogger

Write and publish blog posts from a real Apple II with [FujiNet](https://github.com/FujiNetWIFI/fujinet-lib) hardware. Compose Markdown on your retro machine, sync over Wi-Fi to a modern Python server, and serve beautifully rendered HTML to the web.

```text
Apple II + FujiNet   ──────PUT/GET──────►  FastAPI Server  ──────►  Public Blog
    (CC65 client)                           (Python + uv)            (React SPA)
```

---

## Features

### Apple II Client

- Compose Markdown posts in an 80-column full-screen editor
- Browse, create, edit, delete, and publish posts from a classic menu UI
- Organize posts with a single category per post
- View blog statistics: post counts, category breakdown, monthly histogram
- Configurable server URL — no rebuild required
- Automatic 40/80-column detection; FujiNet version and SSID shown on startup

### Python FastAPI Server

- REST API for full post lifecycle (create, read, update, delete, publish)
- Posts stored as plain-text Markdown files with YAML frontmatter — easy to edit, version-control, and back up
- Markdown rendered to HTML on the fly (via `python-markdown`)
- Category listing with post counts
- Blog statistics endpoint
- Interactive Swagger docs at `/docs`

### React Web Frontend (served by FastAPI)

- **Public blog** — clean reading experience at `/`; browse by category; full post view at `/post/{slug}`
- **Stats page** — post counts, category bar chart, monthly histogram at `/stats`
- **Admin UI** — create, edit, delete, and publish posts from the browser at `/admin`; live Markdown preview

---

## Repository Layout

```text
fujiblogger/   (repo: fuji-blog)
├── README.md               ← You are here
├── CLAUDE.md               ← Deep technical notes (FujiNet quirks, API spec)
├── .gitignore
│
├── client/                 ← Apple II client (CC65 + MekkoGX)
│   ├── Makefile            ← PRODUCT=fujiblog (ProDOS binary), FUJINET_LIB=4.10.0
│   ├── mekkogx/            ← MekkoGX cross-platform build framework (submodule)
│   └── src/
│       ├── main.c          ← Main loop, menu system, all screen functions
│       ├── network.c       ← FujiNet HTTP transport layer
│       ├── network.h
│       ├── api.c           ← JSON request building and response parsing
│       ├── api.h
│       ├── editor.c        ← Line-based Markdown editor
│       └── ui.c            ← Shared UI helpers
│
└── server/                 ← FastAPI backend + React frontend
    ├── README.md           ← Server-specific docs
    ├── pyproject.toml      ← Python dependencies (managed by uv)
    ├── uv.lock
    ├── app/
    │   ├── main.py         ← FastAPI routes
    │   ├── models.py       ← BlogPost dataclass
    │   ├── schemas.py      ← Pydantic request/response models
    │   ├── storage.py      ← File-based persistence (reads/writes .md files)
    │   └── blog_renderer.py ← Markdown → HTML
    ├── data/
    │   └── posts/          ← One {slug}.md file per post (auto-created)
    └── frontend/           ← React + Vite SPA
        ├── src/
        │   ├── App.tsx
        │   ├── api.ts
        │   ├── AdminApp.tsx
        │   ├── pages/      ← BlogHome, BlogPost, BlogCategory, StatsPage, BlogLayout
        │   └── components/ ← PostList, PostEditor, CategorySidebar
        ├── package.json
        └── dist/           ← Built output (served by FastAPI; not committed)
```

---

## Quick Start

### 1 — Start the Server

```bash
cd server
uv sync                                               # install Python deps
uv run uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

URLs once running:

| URL                              | Description                    |
| -------------------------------- | ------------------------------ |
| `http://localhost:8000/`         | Public blog                    |
| `http://localhost:8000/stats`    | Stats page                     |
| `http://localhost:8000/admin`    | Admin UI                       |
| `http://localhost:8000/docs`     | Interactive API docs (Swagger) |
| `http://localhost:8000/api/*`    | REST API                       |

### 2 — Build the React Frontend (optional for dev)

The `dist/` directory is not committed. Build it once before using the web UI:

```bash
cd server/frontend
npm install
npm run build          # output → server/frontend/dist/
```

In development you can also run `npm run dev` for hot-reload (proxies API calls to the FastAPI server).

### 3 — Build and Deploy the Apple II Client

**Prerequisites:** cc65 toolchain installed and on your `PATH`.

```bash
cd client
make apple2
```

Output disk image: `client/build/apple2/r2r/fujiblog.po`

Executable name on disk: `FUJIBLOG.SYSTEM` (ProDOS — 15 chars, the filesystem maximum;
`FUJIBLOGGER.SYSTEM` would be 18 chars and exceed the limit, so the binary keeps the
shorter name while the program displays "FujiBlogger" in its UI.)

Transfer to your Apple II via FujiNet's file browser or your preferred disk-transfer tool. Boot the disk and the client starts automatically.

---

## Apple II Client

### First Run

On boot the client:

1. Detects 40- or 80-column mode and switches to 80 columns if available
2. Queries FujiNet for adapter config (firmware version, SSID, MAC)
3. Drops to the main menu

If FujiNet is not detected, the client prints an error and halts.

### Main Menu

```text
=== MAIN MENU ===

1. List Posts
2. New Post
3. Edit Post
4. Toggle Publish
5. Delete Post
6. Stats
7. Network Status
8. Configuration
Q. Quit
```

### Workflows

#### Create a post

1. `2. New Post` → enter a title
2. Enter a category (optional, press Enter to skip)
3. Write Markdown in the full-screen editor; `Ctrl-S` or `Esc` to save; `Ctrl-Q` to discard
4. Post is saved as a **draft** (`published: false`) — publish it separately

#### Publish or unpublish a post

- `4. Toggle Publish` → pick a post from the list → the published flag is flipped and synced immediately

#### Edit a post

- `3. Edit Post` → pick a post → modify title, category, or body → save

#### Delete a post

- `5. Delete Post` → pick a post → confirm with `Y` at the "PERMANENTLY DELETE?" prompt

#### View stats

- `6. Stats` — two screens:
  - Screen 1: total posts, total categories, per-category bar chart
  - Screen 2: monthly post histogram for the current year (Jan–Dec)

#### Configure server URL

- `8. Configuration` → enter the server IP and port (e.g. `http://192.168.1.50:8000`)
- The default is `http://192.168.15.35:8001` (set at compile time in `main.c`)

#### Diagnose connectivity

- `7. Network Status` → shows FujiNet version, SSID, MAC
  - Press `T` → Test Server submenu:
    - `G` — GET `/api/ping` (tests basic TCP reach)
    - `P` — PUT `/api/posts` with a minimal payload (tests the full write path)

---

## Server API Reference

Full interactive docs are available at `/docs` when the server is running. Key endpoints:

### Posts

| Method   | Path                        | Description                                              |
| -------- | --------------------------- | -------------------------------------------------------- |
| `GET`    | `/api/posts`                | All posts (published + drafts)                           |
| `GET`    | `/api/posts/published`      | Published posts only                                     |
| `GET`    | `/api/posts/summaries`      | Compact list (no body fields) — sized for FujiNet        |
| `GET`    | `/api/posts/{id}`           | Single post by ID                                        |
| `GET`    | `/api/posts/{id}/markdown`  | Title + category + raw Markdown only                     |
| `GET`    | `/api/posts/slug/{slug}`    | Post by URL slug (public blog)                           |
| `POST`   | `/api/posts`                | Create post (standard JSON + `Content-Type` header)      |
| `PUT`    | `/api/posts`                | Create post (FujiNet workaround — no `Content-Type`)     |
| `PUT`    | `/api/posts/{id}`           | Update post (FujiNet workaround — no `Content-Type`)     |
| `PATCH`  | `/api/posts/{id}/publish`   | Toggle published state                                   |
| `PUT`    | `/api/posts/{id}/publish`   | Toggle published state (FujiNet workaround)              |
| `DELETE` | `/api/posts/{id}`           | Delete a post                                            |
| `PUT`    | `/api/posts/{id}/delete`    | Delete a post (FujiNet workaround)                       |

### Categories and Stats

| Method | Path              | Description                                                          |
| ------ | ----------------- | -------------------------------------------------------------------- |
| `GET`  | `/api/categories` | All categories with post counts (`?published_only=true` for public)  |
| `GET`  | `/api/stats`      | Blog statistics (counts, histogram, avg file size)                   |

### Utilities

| Method | Path          | Description                                            |
| ------ | ------------- | ------------------------------------------------------ |
| `GET`  | `/api/ping`   | Lightweight connectivity check — returns `{"ok":true}` |
| `POST` | `/api/render` | Render Markdown to HTML                                |

### Post JSON model

```json
{
  "id":            "80f7e5e7-e73c-495e-9b92-620d049d4e3d",
  "title":         "My First Post",
  "slug":          "my-first-post",
  "markdown_body": "# Heading\n\nContent...",
  "html_body":     "<h1>Heading</h1><p>Content...</p>",
  "category":      "apple2",
  "published":     false,
  "created_at":    "2026-05-22T12:00:00",
  "updated_at":    "2026-05-22T12:00:00"
}
```

Write endpoints return the compact `BlogPostSummary` shape (omits `markdown_body` and `html_body`) so responses fit within FujiNet's receive buffer.

---

## Post Storage Format

Every post is a plain Markdown file at `server/data/posts/{slug}.md`:

```markdown
---
id: 80f7e5e7-e73c-495e-9b92-620d049d4e3d
title: My First Post
slug: my-first-post
category: apple2
published: false
created_at: '2026-05-22T12:00:00'
updated_at: '2026-05-22T12:00:00'
---

# My First Post

Post content in Markdown here.
```

You can create or edit posts directly in any text editor — restart the server (or let `--reload` pick it up) and the changes are live. The `id` field must be a valid UUID.

---

## Environment Variables

| Variable   | Default       | Description                         |
| ---------- | ------------- | ----------------------------------- |
| `DATA_DIR` | `server/data` | Root directory for `posts/` folder  |
| `HOST`     | `0.0.0.0`     | Bind address for uvicorn            |
| `PORT`     | `8000`        | Listen port                         |

---

## Testing the Server

```bash
# Start server
cd server
uv run uvicorn app.main:app --reload

# Ping
curl http://localhost:8000/api/ping

# Create a post (standard browser/curl)
curl -X POST http://localhost:8000/api/posts \
  -H "Content-Type: application/json" \
  -d '{"title":"Hello","markdown_body":"# Hello\n\nWorld.","category":"test","published":false}'

# Create a post (FujiNet-style PUT, no Content-Type)
curl -X PUT http://localhost:8000/api/posts \
  -d '{"title":"Hello","markdown_body":"# Hello\n\nWorld.","category":"test","published":false}'

# List all posts
curl http://localhost:8000/api/posts

# Stats
curl http://localhost:8000/api/stats
```

---

## Troubleshooting

### Server Issues

| Symptom | Fix |
| ------- | --- |
| `ModuleNotFoundError: No module named 'fastapi'` | Run `cd server && uv sync` |
| `Address already in use` | Use `--port 9000` or kill the process on port 8000 |
| Frontend shows "Frontend not built" | Run `npm run build` in `server/frontend/` |

### Client Build Issues

| Symptom | Fix |
| ------- | --- |
| `cc65: command not found` | Install the cc65 toolchain and add it to `PATH` |
| `BSS overflows memory area 'BSS'` | Reduce static buffer sizes in `client/src/main.c` or move data to RODATA |
| `FUJINET_LIB not found` | Set `FUJINET_LIB = 4.10.0` (or latest) in `client/Makefile` |

### Apple II + FujiNet Connectivity Issues

| Symptom | Fix |
| ------- | --- |
| "FujiNet NOT DETECTED" | Check FujiNet is powered on, seated correctly, and running firmware ≥ 4.10.0 |
| `7. Network Status → T → G` fails | Verify server IP/port in `8. Configuration`; confirm the machine can reach the server |
| `7. Network Status → T → P` fails | Check server logs for the incoming PUT; ensure write endpoints return `BlogPostSummary` |
| "Relocation/Configuration Error" on boot | ProDOS filename must be ≤ 15 chars, A–Z/0–9/period only. The binary ships as `FUJIBLOG.SYSTEM` (15 chars). |
| Garbage characters on screen | ANSI escape codes were added without `#ifdef __CC65__` guards — remove them |

---

## FujiNet IWM Firmware Notes

The IWM (SmartPort) firmware variant used on the Apple IIc has a known bug: `network_http_set_channel_mode()` silently delivers **mode 0** regardless of the requested mode. As a result, `network_http_post()` body writes are discarded by the firmware.

**Workaround** used throughout this client: open with `OPEN_MODE_HTTP_PUT`, write the JSON body with `network_write()`, then call `network_json_parse()`. Server endpoints that receive data from the client all accept raw JSON without requiring a `Content-Type` header.

A secondary quirk: after `network_json_parse()` reads the response, calling `network_close()` re-flushes the PUT body buffer — sending a second identical request. The server handles this with a short-lived deduplication cache (30-second TTL keyed on request fingerprint) so duplicate flushes are silently absorbed.

See [`CLAUDE.md`](CLAUDE.md) for the complete technical description.

---

## System Requirements

### Server Requirements

- Python 3.10+
- [`uv`](https://docs.astral.sh/uv/getting-started/installation/) package manager

### Frontend Requirements (development only)

- Node.js 18+
- npm

### Apple II Client Build Requirements

- cc65 toolchain (`cc65`, `ca65`, `ld65`, `cl65`)
- FujiNet-lib 4.10.0 (fetched automatically by MekkoGX)

### Apple II Client Runtime Requirements

- Apple IIc or IIc Plus
- FujiNet IWM device, firmware 4.10.0+
- Wi-Fi network with access to the server

---

## License

MIT — see individual source files for details.

---

*Happy blogging from your Apple II!* 🍎
