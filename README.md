# FujiNet Apple IIc Markdown Blog

A client/server application that lets you write and publish blog posts from an Apple IIc with FujiNet hardware. Write Markdown on your retro machine, sync to a modern server, and serve beautifully rendered HTML to the web.

## Features

- **Apple IIc Client**: Compose Markdown posts directly on your Apple IIc
  - 40-column and 80-column mode support
  - Browse, create, edit, and delete posts
  - Toggle publish state with a single keystroke
  - Organize posts by categories
  - Real-time sync with server over FujiNet network

- **Python FastAPI Server**: Modern REST API backend
  - CRUD operations for blog posts
  - Markdown to HTML rendering with syntax highlighting
  - File-based JSON storage (easy to backup and version control)
  - Swagger API documentation

- **React Admin UI**: Web-based post management
  - Dashboard with list of all posts
  - Create, edit, and delete posts from browser
  - Live Markdown preview
  - Publish/draft state management
  - Category management

- **Public Blog**: Serve rendered posts to the web
  - RESTful API for fetching published posts
  - Ready for static site generation or custom frontend

## System Requirements

### Server (Linux, macOS, Windows with WSL)
- Python 3.10 or later
- `uv` package manager ([install here](https://docs.astral.sh/uv/getting-started/installation/))
- Node.js 16+ for React frontend development (optional, only if building frontend)

### Apple IIc Client
- Apple IIc or compatible machine
- FujiNet hardware (v4.10.0+)
- CC65 toolchain (for building from source)

## Quick Start

### Server Setup

1. **Install dependencies** using `uv`:
   ```bash
   cd server
   uv sync
   ```

2. **Run the FastAPI server**:
   ```bash
   uv run uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
   ```
   
   The server will start on `http://localhost:8000`:
   - API endpoints: `http://localhost:8000/api/*`
   - Admin UI: `http://localhost:8000/`
   - API docs: `http://localhost:8000/docs`

3. **(Optional) Build the React frontend for production**:
   ```bash
   cd server/frontend
   npm install
   npm run build
   ```
   Built files go to `server/frontend/dist/` and are automatically served by FastAPI.

### Apple IIc Client Build

1. **Build the client**:
   ```bash
   cd client
   make apple2
   ```
   
   The built executable will be in `client/build/apple2/r2r/fuji_blog`.

2. **Transfer to Apple IIc**:
   - Use FujiNet's file transfer features or
   - Create a bootable disk from the executable
   - See [FujiNet documentation](https://github.com/FujiNetWIFI/fujinet-lib) for transfer methods

3. **Run the client**:
   - Boot the Apple IIc and run the executable
   - The client auto-detects FujiNet hardware and your screen mode (40/80 column)
   - Configure the server URL in the settings menu if not using default (`http://192.168.1.100:8000`)

## API Reference

All API endpoints are documented with examples below. For interactive documentation, visit `http://localhost:8000/docs` when the server is running.

### Blog Post Model

```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "title": "My First Blog Post",
  "slug": "my-first-blog-post",
  "markdown_body": "# Hello\n\nThis is my first post.",
  "html_body": "<h1>Hello</h1>\n<p>This is my first post.</p>",
  "categories": ["tutorial", "hello-world"],
  "published": false,
  "created_at": "2026-05-19T12:00:00Z",
  "updated_at": "2026-05-19T12:00:00Z"
}
```

### Endpoints

#### Create a Post
```
POST /api/posts
Content-Type: application/json

{
  "title": "My Post Title",
  "markdown_body": "# Hello\n\nContent here...",
  "categories": ["tag1", "tag2"],
  "published": false
}
```

Returns: Created post with auto-generated ID and timestamps.

#### List All Posts (Admin)
```
GET /api/posts
```

Returns: Array of all posts (published and drafts), sorted by creation date (newest first).

#### List Published Posts Only (Public)
```
GET /api/posts/published
```

Returns: Array of published posts only.

#### Get Single Post
```
GET /api/posts/{id}
```

Returns: A single post by ID, or 404 if not found.

#### Update a Post
```
PUT /api/posts/{id}
Content-Type: application/json

{
  "title": "Updated Title",
  "markdown_body": "Updated content...",
  "categories": ["updated-tag"],
  "published": false
}
```

Returns: Updated post.

#### Toggle Publish State
```
PATCH /api/posts/{id}/publish
Content-Type: application/json

{
  "published": true
}
```

Returns: Updated post with new published state.

#### Delete a Post
```
DELETE /api/posts/{id}
```

Returns: 204 No Content on success.

#### Render Markdown to HTML
```
POST /api/render
Content-Type: application/json

{
  "markdown_body": "# Heading\n\nParagraph text."
}
```

Returns:
```json
{
  "html": "<h1>Heading</h1>\n<p>Paragraph text.</p>"
}
```

## Development

### Project Structure

```
fuji-blog/
├── README.md              # This file
├── CLAUDE.md              # Detailed technical documentation
├── client/                # Apple IIc client (CC65)
│   ├── Makefile
│   ├── src/
│   │   ├── main.c         # Menu system and UI
│   │   ├── network.c      # FujiNet HTTP communication
│   │   ├── api.c          # JSON parsing and request building
│   │   └── editor.c       # Text editor for post composition
│   └── mekkogx/           # MekkoGX cross-platform build framework
│
└── server/                # FastAPI backend + React frontend
    ├── pyproject.toml     # Python dependencies (uv)
    ├── uv.lock            # Locked versions
    ├── app/
    │   ├── main.py        # FastAPI routes
    │   ├── models.py      # BlogPost data model
    │   ├── schemas.py     # Pydantic validation
    │   ├── storage.py     # JSON persistence
    │   └── blog_renderer.py # Markdown rendering
    ├── data/
    │   └── posts.json     # Blog data (auto-created)
    └── frontend/          # React admin UI
        ├── src/
        ├── package.json
        └── dist/          # Built static assets
```

### Adding Server Features

1. Define new Pydantic models in `server/app/schemas.py`
2. Add route handlers in `server/app/main.py`
3. Update persistence logic in `server/app/storage.py` if needed
4. Test with `curl` or the Swagger UI at `/docs`
5. Update React components in `server/frontend/src/` to consume new endpoints

### Adding Client Features

1. Create new functions in appropriate `.c` file or add to existing:
   - `src/main.c` — Menu and UI
   - `src/network.c` — FujiNet HTTP calls
   - `src/api.c` — JSON parsing
   - `src/editor.c` — Text editing
   - `src/ui.c` — Utility functions
2. Update `main.c` menu system if adding new menu options
3. Recompile: `cd client && make apple2`
4. Test on Apple IIc or emulator

## Testing

### Test the Server Manually

```bash
# Start the server
cd server
uv run uvicorn app.main:app --reload

# In another terminal, create a post
curl -X POST http://localhost:8000/api/posts \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Test Post",
    "markdown_body": "# Test\n\nThis is a test.",
    "categories": ["test"],
    "published": false
  }'

# List all posts
curl http://localhost:8000/api/posts

# List published posts only
curl http://localhost:8000/api/posts/published

# Get API documentation
curl http://localhost:8000/docs  # View in browser
```

### Test the Client Build

```bash
cd client
make apple2
# Check for executable at: client/build/apple2/r2r/fuji_blog
```

### End-to-End Testing

1. Start server: `cd server && uv run uvicorn app.main:app --reload`
2. Create test posts via curl or Swagger UI
3. Run client on Apple IIc (or emulator)
4. Verify list, create, edit, delete, and publish toggle work
5. Check that changes persist on server

## Environment Variables

### Server

- `DATA_DIR`: Path to store `posts.json` (default: `server/data/`)
- `HOST`: Server host (default: `0.0.0.0`)
- `PORT`: Server port (default: `8000`)

Set these before running the server:
```bash
export DATA_DIR=/custom/path
uv run uvicorn app.main:app --host 0.0.0.0 --port 9000
```

### Client

- Server URL is configurable in the Apple IIc client menu
- Default: `http://192.168.1.100:8000`
- Can be changed at runtime or hardcoded in `client/src/main.c` before building

## Troubleshooting

### Server won't start

```
ModuleNotFoundError: No module named 'fastapi'
```

Solution: Run `cd server && uv sync` to install dependencies.

```
[Errno 48] Address already in use
```

Solution: Change port with `--port 9000` or kill process on port 8000.

### Client build fails

```
cc65: Unknown option '-...'
```

Solution: Ensure cc65 is installed and in your PATH:
```bash
which cc65
cc65 --version
```

```
FUJINET_LIB not found
```

Solution: Update `FUJINET_LIB` version in `client/Makefile`. The current version is `4.10.0`. See [FujiNet releases](https://github.com/FujiNetWIFI/fujinet-lib/releases) for available versions.

### Client can't connect to server

1. **Verify server is running**: Open `http://localhost:8000` in browser
2. **Check network**: Use Apple IIc's network status menu to verify FujiNet is online
3. **Verify server URL**: Confirm IP and port are correct in client settings
4. **Check firewall**: Allow port 8000 on your network
5. **Ping server**: From your development machine, verify the IP is reachable

### FujiNet hardware not detected

- Ensure FujiNet is powered on and connected to Apple IIc
- Check FujiNet firmware version matches library requirements (4.10.0+)
- Try resetting FujiNet device
- Consult [FujiNet hardware documentation](https://github.com/FujiNetWIFI/fujinet-lib/wiki)

## Contributing

We welcome contributions! Here's how to help:

### Reporting Bugs

1. Describe the issue clearly (expected vs actual behavior)
2. Include steps to reproduce
3. Note your platform, Python version, and Apple IIc model
4. Attach relevant error messages

### Submitting Code

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make changes:
   - **Server**: Follow FastAPI conventions, add type hints, include docstrings
   - **Client**: Use standard C conventions, keep line length ≤80 columns for Apple II
   - **Frontend**: Use TypeScript, follow React best practices
4. Test thoroughly (see Testing section)
5. Submit a pull request with:
   - Clear description of changes
   - Rationale for changes
   - Testing performed
   - Any new dependencies (with versions)

### Areas for Contribution

- **Documentation**: Improve guides, add examples, translate README
- **Client Features**: Text editor improvements, better menus, column mode fixes
- **Server Features**: Add pagination, search, tagging improvements
- **Frontend**: Better UI, dark mode, mobile responsiveness
- **Testing**: Unit tests, integration tests, edge case testing
- **Build System**: Improved Makefile, CI/CD setup

## License

[Add your license here]

## Support & Contact

For questions, issues, or discussions:

- Open an issue on GitHub
- Check existing discussions for similar questions
- See [CLAUDE.md](CLAUDE.md) for detailed technical documentation

---

**Happy blogging from your Apple IIc!** 🍎
