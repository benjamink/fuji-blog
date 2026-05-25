# Fuji Blog Server

FastAPI server for the FujiNet Apple IIc Markdown Blog project.

This service provides the REST API used by the Apple IIc client and the React admin frontend. It stores posts as JSON files under `server/data/` and renders Markdown to HTML.

Quick start:

```bash
cd server
uv sync
uv run uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

For interactive API docs visit `http://localhost:8000/docs` once the server is running.

License: MIT
