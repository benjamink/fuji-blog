# FujiBlogger — Server

FastAPI backend and React frontend for FujiBlogger — the FujiNet Apple IIc Markdown Blog.

The server handles all REST API requests from the Apple IIc client and web browsers, persists posts as plain Markdown files, renders Markdown to HTML, and serves the public blog and admin UI as a single React SPA.

---

## Architecture

```text
                        ┌─────────────────────────────────┐
  Apple IIc client ────►│                                 │
  (FujiNet HTTP)        │  FastAPI  (app/main.py)         │
                        │                                 │
  Web browser  ────────►│  /api/*  ──► BlogStorage        ├──► data/posts/*.md
  (Admin UI /           │              BlogRenderer       │     (YAML frontmatter
   Public blog)         │                                 │      + Markdown body)
                        │  /*      ──► React SPA          │
                        │              frontend/dist/     │
                        └─────────────────────────────────┘
```

---

## Directory Layout

```text
server/
├── README.md              ← This file
├── pyproject.toml         ← uv project config and Python dependencies
├── uv.lock                ← Locked dependency versions
│
├── app/
│   ├── main.py            ← FastAPI routes and request handlers
│   ├── models.py          ← BlogPost dataclass (in-memory representation)
│   ├── schemas.py         ← Pydantic request/response models
│   ├── storage.py         ← File-based persistence layer
│   └── blog_renderer.py   ← Markdown → HTML (python-markdown)
│
├── data/
│   ├── posts/             ← One {slug}.md per post (auto-created on first run)
│   └── .gitkeep
│
└── frontend/
    ├── index.html
    ├── package.json
    ├── vite.config.ts
    ├── tsconfig.json
    ├── src/
    │   ├── main.tsx           ← React entry point
    │   ├── App.tsx            ← SPA router (public blog + /admin)
    │   ├── AdminApp.tsx       ← Admin panel root
    │   ├── api.ts             ← Typed API client (axios)
    │   ├── pages/
    │   │   ├── BlogHome.tsx       ← Post list (all published)
    │   │   ├── BlogCategory.tsx   ← Filtered by category
    │   │   ├── BlogPost.tsx       ← Full post view
    │   │   ├── BlogLayout.tsx     ← Header with category nav
    │   │   ├── StatsPage.tsx      ← Stats dashboard
    │   │   └── blog.css
    │   └── components/
    │       ├── PostList.tsx       ← Admin post table
    │       ├── PostEditor.tsx     ← Create/edit with Markdown preview
    │       └── CategorySidebar.tsx
    └── dist/              ← Production build output (not committed)
```

---

## Setup

### Prerequisites

- Python 3.10+
- [`uv`](https://docs.astral.sh/uv/getting-started/installation/) — fast Python package and project manager
- Node.js 18+ and npm (only needed to build or develop the frontend)

### Install Python dependencies

```bash
cd server
uv sync
```

### Run the development server

```bash
uv run uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

`--reload` watches `app/` for changes and restarts automatically.

### Build the React frontend

```bash
cd frontend
npm install
npm run build          # writes to frontend/dist/
```

FastAPI serves `frontend/dist/` on every non-API path. If `dist/` does not exist, the catch-all returns a JSON message directing you to build it.

### Frontend development (hot-reload)

```bash
cd frontend
npm run dev            # Vite dev server on :5173, proxies /api/* to :8000
```

---

## Running in Production

```bash
# Single process (fine for a personal/home server)
uv run uvicorn app.main:app --host 0.0.0.0 --port 8000 --workers 1

# Or with gunicorn for multi-worker
uv run gunicorn app.main:app -k uvicorn.workers.UvicornWorker \
    --bind 0.0.0.0:8000 --workers 2
```

Set `DATA_DIR` if you want posts stored outside the repo:

```bash
export DATA_DIR=/var/data/fujiblogger
uv run uvicorn app.main:app --host 0.0.0.0 --port 8000
```

---

## Admin Authentication

The admin panel (`/admin`) and all write API endpoints are protected by JWT-based HTTP Bearer authentication.  Credentials are stored in an [Apache htpasswd](https://httpd.apache.org/docs/current/programs/htpasswd.html)-format file using bcrypt hashes.

### Default credentials

| Username | Password   |
| -------- | ---------- |
| `admin`  | `password` |

> **Change the default password before exposing the server to the internet.**

### Setting the password — Docker

```bash
# Interactive prompt inside the running container
docker exec -it fujiblogger .venv/bin/python scripts/set_password.py
```

Restart the container after updating the password so the server reloads `data/.htpasswd`:

```bash
docker restart fujiblogger
```

### Setting the password — local / bare-metal

```bash
cd server
uv run python scripts/set_password.py
```

The script prompts for a username (default: `admin`) and password, writes a bcrypt hash to `data/.htpasswd`, and prints the recommended startup command with a stable `JWT_SECRET`.

### Keeping sessions alive across restarts

By default, `JWT_SECRET` is regenerated on every startup, which invalidates all existing tokens.  Set it once and store it somewhere safe:

```bash
export JWT_SECRET=$(openssl rand -hex 32)
uv run uvicorn app.main:app --host 0.0.0.0 --port 8000
```

For Docker, pass it via the environment file (see `fujiblogger.service`):

```bash
echo "JWT_SECRET=$(openssl rand -hex 32)" >> /etc/fujiblogger/env
```

---

## Environment Variables

| Variable          | Default              | Description                                                     |
| ----------------- | -------------------- | --------------------------------------------------------------- |
| `DATA_DIR`        | `<repo>/server/data` | Root directory; `posts/` and `.htpasswd` are created inside     |
| `HOST`            | `0.0.0.0`            | Passed to uvicorn `--host`                                      |
| `PORT`            | `8000`               | Passed to uvicorn `--port`                                      |
| `JWT_SECRET`      | *(random on start)*  | HMAC signing secret for JWTs; set to a stable value in prod     |
| `JWT_EXPIRE_HOURS`| `24`                 | Token lifetime in hours                                         |
| `HTPASSWD_PATH`   | `data/.htpasswd`     | Path to the bcrypt htpasswd credentials file                    |

---

## API Reference

Full interactive docs (Swagger UI) are available at `http://localhost:8000/docs` when the server is running. ReDoc is at `/redoc`.

### Data models

#### BlogPost (full response)

```json
{
  "id":            "80f7e5e7-e73c-495e-9b92-620d049d4e3d",
  "title":         "My Post",
  "slug":          "my-post",
  "markdown_body": "# My Post\n\nContent...",
  "html_body":     "<h1>My Post</h1><p>Content...</p>",
  "category":      "apple2",
  "published":     false,
  "created_at":    "2026-05-22T12:00:00",
  "updated_at":    "2026-05-22T12:00:00"
}
```

#### BlogPostSummary (write-endpoint response)

Same shape but without `markdown_body` and `html_body`. Used by all write endpoints to keep responses small enough for FujiNet's receive buffer.

#### StatsResponse

```json
{
  "total_posts":      12,
  "total_categories": 4,
  "avg_bytes":        1024,
  "year":             2026,
  "categories": [
    { "name": "apple2", "count": 7 },
    { "name": "fujinet", "count": 3 }
  ],
  "posts_per_month": [1, 0, 2, 1, 3, 2, 1, 0, 0, 1, 0, 1]
}
```

### Endpoints

#### Posts

| Method   | Path                       | Auth  | Description                                             |
| -------- | -------------------------- | ----- | ------------------------------------------------------- |
| `GET`    | `/api/posts`               | —     | All posts (published + drafts)                          |
| `GET`    | `/api/posts/published`     | —     | Published posts only                                    |
| `GET`    | `/api/posts/summaries`     | —     | Compact list without body fields                        |
| `GET`    | `/api/posts/{id}`          | —     | Single post by UUID                                     |
| `GET`    | `/api/posts/{id}/markdown` | —     | Title + category + raw Markdown (no HTML)               |
| `GET`    | `/api/posts/slug/{slug}`   | —     | Published post by URL slug                              |
| `POST`   | `/api/posts`               | —     | Create post (requires `Content-Type: application/json`) |
| `PUT`    | `/api/posts`               | —     | Create post (no `Content-Type` needed — FujiNet)        |
| `PUT`    | `/api/posts/{id}`          | —     | Update post (no `Content-Type` needed — FujiNet)        |
| `PATCH`  | `/api/posts/{id}/publish`  | —     | Set `published` flag                                    |
| `PUT`    | `/api/posts/{id}/publish`  | —     | Set `published` flag (FujiNet workaround)               |
| `DELETE` | `/api/posts/{id}`          | —     | Delete post                                             |
| `PUT`    | `/api/posts/{id}/delete`   | —     | Delete post (FujiNet workaround)                        |

`GET /api/posts` and `GET /api/posts/published` both accept optional query params:

- `?published_only=true` — filter to published posts
- `?category=apple2` — filter by category name

#### Categories and Stats

| Method | Path              | Description                                             |
| ------ | ----------------- | ------------------------------------------------------- |
| `GET`  | `/api/categories` | Category names + post counts, sorted by count desc      |
| `GET`  | `/api/stats`      | Full stats: counts, per-category breakdown, histogram   |

`/api/categories` accepts `?published_only=true` to count only published posts (used by the public blog sidebar).

#### Utilities

| Method | Path          | Description                                        |
| ------ | ------------- | -------------------------------------------------- |
| `GET`  | `/api/ping`   | Returns `{"ok": true}` — connectivity check        |
| `POST` | `/api/render` | Body: `{"markdown_body":"..."}` → `{"html":"..."}` |

### Query Examples

```bash
# All posts
curl http://localhost:8000/api/posts

# Published posts in category "apple2"
curl "http://localhost:8000/api/posts/published?category=apple2"

# Compact list (no body fields) — for bandwidth-limited clients
curl http://localhost:8000/api/posts/summaries

# Categories with counts (published only)
curl "http://localhost:8000/api/categories?published_only=true"

# Stats
curl http://localhost:8000/api/stats

# Create (standard)
curl -X POST http://localhost:8000/api/posts \
  -H "Content-Type: application/json" \
  -d '{"title":"Hello","markdown_body":"# Hello\n\nWorld.","category":"test","published":false}'

# Create (FujiNet-style — no Content-Type)
curl -X PUT http://localhost:8000/api/posts \
  -d '{"title":"Hello","markdown_body":"# Hello\n\nWorld.","category":"test","published":false}'

# Toggle published
curl -X PATCH http://localhost:8000/api/posts/{id}/publish \
  -H "Content-Type: application/json" \
  -d '{"published": true}'

# Delete
curl -X DELETE http://localhost:8000/api/posts/{id}

# Render Markdown
curl -X POST http://localhost:8000/api/render \
  -H "Content-Type: application/json" \
  -d '{"markdown_body":"# Hello\n\n**World**"}'
```

---

## Post Storage Format

Each post is stored as `data/posts/{slug}.md` — a plain Markdown file with YAML frontmatter:

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

Key rules:

- **`id`** must be a valid UUID v4. Generate one with `python3 -c "import uuid; print(uuid.uuid4())"`.
- **`slug`** is the filename without `.md`. Rename the file if you change the slug.
- **`category`** is a single string, not a list.
- **`html_body`** is never stored — it is rendered on-the-fly from `markdown_body`.
- You can edit or create `.md` files directly in `data/posts/`. With `--reload`, the server picks up changes immediately.

### Slug collision handling

If two posts end up with the same slug (e.g., both titled "Test"), the second gets a counter suffix: `test.md`, `test-2.md`, etc.

### Title renames

When a post title is updated via the API, `storage.py` renames the `.md` file to match the new slug automatically.

### Automatic migrations

On startup `storage.py` runs these one-time migrations if needed:

1. **JSON → Markdown** — converts a legacy `data/posts.json` to individual `.md` files
2. **UUID filenames → slug filenames** — renames any `{uuid}.md` files from the initial migration

---

## FujiNet Compatibility

The Apple IIc FujiNet IWM firmware has two quirks that affect the API design:

### 1. POST body is discarded (channel-mode bug)

`network_http_set_channel_mode()` always delivers mode 0 (DATA mode), so `network_http_post()` body writes never reach the firmware's `postData` buffer. The client works around this by using `OPEN_MODE_HTTP_PUT` + `network_write()` for every write operation.

Every mutating endpoint therefore has a twin `PUT` route that accepts raw JSON without requiring a `Content-Type: application/json` header:

| Standard endpoint                 | FujiNet PUT twin                  |
| --------------------------------- | --------------------------------- |
| `POST /api/posts`                 | `PUT /api/posts`                  |
| `PUT /api/posts/{id}`             | *(same method, same workaround)*  |
| `PATCH /api/posts/{id}/publish`   | `PUT /api/posts/{id}/publish`     |
| `DELETE /api/posts/{id}`          | `PUT /api/posts/{id}/delete`      |

### 2. Double-flush on network_close (dedup cache)

After `network_json_parse()` reads the server response, the firmware re-flushes the PUT body buffer when `network_close()` is called — sending a second identical request.

The server absorbs this silently with two in-memory TTL caches (30-second window):

- **Create dedup** — keyed on SHA-1 of `(title, markdown_body)`. Duplicate creates return the already-created post.
- **Delete dedup** — keyed on `post_id`. Duplicate deletes return `{"id": post_id}` even after the post is gone.

These caches are in-memory and reset on server restart, which is fine since the TTL is only 30 seconds.

---

## Frontend

The React SPA is served by FastAPI's catch-all route. React Router handles all client-side navigation.

### Routes

| Path            | Component        | Description                          |
| --------------- | ---------------- | ------------------------------------ |
| `/`             | `BlogHome`       | All published posts                  |
| `/post/{slug}`  | `BlogPost`       | Full post view (rendered HTML)       |
| `/{category}`   | `BlogCategory`   | Posts filtered by category           |
| `/stats`        | `StatsPage`      | Stats dashboard                      |
| `/admin`        | `AdminApp`       | Post management (create/edit/delete) |

`/stats` and `/admin` are registered before `/:category` so they are not matched as category names.

### Admin UI

The admin panel at `/admin` provides:

- Post list with publish state indicators
- Inline create and edit form with split Markdown/preview panes
- One-click publish/unpublish toggle
- Delete with confirmation

### Stats page

The stats page at `/stats` shows:

- Summary cards: total posts, total categories, average post size
- Horizontal bar chart of posts per category
- Monthly histogram (green bars) for the current calendar year

---

## Development

### Add a new API endpoint

1. Define request/response models in `app/schemas.py`
2. Add the route handler in `app/main.py`
3. Update `app/storage.py` if new persistence logic is needed
4. Test with `curl` or the Swagger UI at `/docs`
5. Update `frontend/src/api.ts` and add any new UI components

### Add a new frontend page

1. Create `frontend/src/pages/MyPage.tsx`
2. Import and add a `<Route>` in `frontend/src/App.tsx`
3. Add navigation in `frontend/src/pages/BlogLayout.tsx` if it should appear in the header
4. Run `npm run dev` to test, then `npm run build` before deploying

### Dependency management

```bash
# Add a Python package
cd server
uv add some-package

# Add a frontend package
cd server/frontend
npm install some-package
```

---

## Troubleshooting

| Symptom | Cause | Fix |
| ------- | ----- | --- |
| `ModuleNotFoundError` on startup | Dependencies not installed | `cd server && uv sync` |
| `Address already in use` | Port 8000 is taken | `--port 9001` or kill the existing process |
| `404` on all `/admin` routes after hard reload | `dist/` is stale or missing | `npm run build` in `frontend/` |
| Post edits via text editor not appearing | Server cache | Restart uvicorn (or wait for `--reload` to detect the file change) |
| `PUT /api/posts` returns 422 | Missing required fields | Body must include `title` and `markdown_body` |
| `PUT /api/posts/{id}/delete` returns 404 | Post already deleted; dedup TTL expired | Expected — the delete succeeded on the first request |
| `/admin` login fails with 401 | Wrong credentials or default password still set | Run `scripts/set_password.py` to set a new password |
| Logged out after server restart | `JWT_SECRET` was not set (random key regenerated) | Set `JWT_SECRET` to a stable value (see above) |
| Large response causes FujiNet parse error | Response body too big | Ensure write endpoints return `BlogPostSummary`, not `BlogPostResponse` |

---

## License

MIT
