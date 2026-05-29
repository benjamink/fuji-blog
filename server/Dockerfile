# ╔═══════════════════════════════════════════════════════════════════╗
# ║  FujiBlogger Server                                               ║
# ║                                                                   ║
# ║  Build:                                                           ║
# ║    docker build -t fujiblogger .                                  ║
# ║                                                                   ║
# ║  Run:                                                             ║
# ║    docker run -d \                                                ║
# ║      -p 8000:8000 \                                               ║
# ║      -v fujiblogger-data:/app/data \                              ║
# ║      -e JWT_SECRET=$(openssl rand -hex 32) \                      ║
# ║      fujiblogger                                                  ║
# ║                                                                   ║
# ║  Set admin password (first run):                                  ║
# ║    docker exec -it <container> \                                  ║
# ║      .venv/bin/python scripts/set_password.py                     ║
# ╚═══════════════════════════════════════════════════════════════════╝


# ─── Stage 1: Build the React / Vite frontend ───────────────────────────────
FROM node:22-alpine AS frontend-builder

WORKDIR /frontend

# Install dependencies first (cached layer)
COPY frontend/package*.json ./
RUN npm ci

# Build — output lands in /frontend/dist
COPY frontend/ ./
RUN npm run build


# ─── Stage 2: Python FastAPI server ─────────────────────────────────────────
FROM python:3.13-slim

# Bring in uv from the official distroless image
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

WORKDIR /app

# ── Python dependencies ──────────────────────────────────────────────────────
# Install runtime deps first so this layer is only invalidated when
# pyproject.toml or uv.lock changes, not on every source edit.
# --no-install-project skips building the project package (which would require
# README.md and a full source tree); we run directly from source instead.
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev --no-install-project

# Copy application source
COPY app/ ./app/

# Make the source tree importable as a top-level package
ENV PYTHONPATH=/app

# ── Frontend ─────────────────────────────────────────────────────────────────
# main.py looks for frontend/dist relative to its own parent directory (/app)
COPY --from=frontend-builder /frontend/dist/ ./frontend/dist/

# ── Helper scripts ───────────────────────────────────────────────────────────
# Includes set_password.py for managing admin credentials and the entrypoint
COPY scripts/ ./scripts/
RUN chmod +x /app/scripts/docker-entrypoint.sh

# ── Data directory ───────────────────────────────────────────────────────────
# /app/data holds blog posts (data/posts/) and credentials (data/.htpasswd).
# Mount this as a named volume so data survives container restarts.
RUN mkdir -p /app/data/posts

# Bake in a default credential (username: admin, password: password).
# Docker copies image content into a named volume on first use when the volume
# is empty, so the container is usable immediately without a setup step.
# Change the password after first login with: docker exec -it <c> .venv/bin/python scripts/set_password.py
RUN .venv/bin/python -c "\
import bcrypt; \
from pathlib import Path; \
h = bcrypt.hashpw(b'password', bcrypt.gensalt()).decode(); \
Path('data/.htpasswd').write_text(f'admin:{h}\n') \
"

# ── Non-root user ─────────────────────────────────────────────────────────────
# The container starts as root so the entrypoint can fix volume ownership,
# then drops to fujiblog before exec'ing the server.
RUN groupadd --system fujiblog \
 && useradd --system --gid fujiblog --no-create-home --shell /bin/false fujiblog \
 && chown -R fujiblog:fujiblog /app

# ── Runtime configuration ─────────────────────────────────────────────────────
# Override these with -e at `docker run` time:
#   JWT_SECRET        HMAC signing secret (random key used if unset — sessions
#                     are invalidated on restart; set for persistent logins)
#   JWT_EXPIRE_HOURS  Token lifetime in hours (default: 24)
#   HTPASSWD_PATH     Credentials file path (default: data/.htpasswd)
#   HOST / PORT       Bind address / port (defaults: 0.0.0.0 / 8000)
ENV HOST=0.0.0.0 \
    PORT=8000

EXPOSE 8000

# /app/data must be a volume to persist posts and credentials
VOLUME ["/app/data"]

# Entrypoint runs as root, chowns /app/data, then drops to fujiblog
ENTRYPOINT ["/app/scripts/docker-entrypoint.sh"]
CMD ["sh", "-c", "exec /app/.venv/bin/uvicorn app.main:app --host \"${HOST}\" --port \"${PORT}\""]
