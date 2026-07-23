"""Authentication helpers for the FujiBlogger admin API.

Credentials are stored in an Apache-format htpasswd file.  Each line is
``username:hash`` where the hash is a bcrypt digest produced by the
``bcrypt`` library (``$2b$`` prefix).  Apache's ``$2y$`` prefix is also
accepted during verification — the two are interchangeable for bcrypt.

Sessions are issued as signed HS256 JWT tokens via ``python-jose``.

Environment variables
---------------------
JWT_SECRET        HMAC signing secret.  If unset a random key is generated
                  at startup — tokens are invalidated on every server
                  restart.  Set this to a stable hex string for persistent
                  sessions.
JWT_EXPIRE_HOURS  Token lifetime in hours (default: 24).
HTPASSWD_PATH     Path to the htpasswd file (default: data/.htpasswd).
API_KEY           Pre-shared admin key for the Apple IIc FujiNet client.
                  The IWM firmware cannot send custom request headers
                  (see CLAUDE.md), so the client cannot supply an
                  ``Authorization: Bearer`` token.  Instead it sends this
                  key as a ``?key=`` query parameter.  If unset, query-key
                  auth is disabled and only Bearer JWTs are accepted.

Setup
-----
Run ``uv run python scripts/set_password.py`` to create or update
credentials.
"""

import os
import secrets
import warnings
from datetime import datetime, timedelta, timezone
from pathlib import Path

import bcrypt as _bcrypt
from fastapi import Depends, HTTPException, Query, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from jose import JWTError, jwt

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

_raw_secret = os.environ.get("JWT_SECRET", "")
if _raw_secret:
    SECRET_KEY: str = _raw_secret
else:
    SECRET_KEY = secrets.token_hex(32)
    warnings.warn(
        "JWT_SECRET env var is not set — using a randomly generated key. "
        "All tokens will be invalidated on server restart. "
        "Set JWT_SECRET to a stable secret for persistent sessions.",
        stacklevel=1,
    )

ALGORITHM = "HS256"
TOKEN_EXPIRE_HOURS = int(os.environ.get("JWT_EXPIRE_HOURS", "24"))
HTPASSWD_PATH = Path(
    os.environ.get("HTPASSWD_PATH", "data/.htpasswd")
)

# Pre-shared key for the Apple IIc client (query-param auth).  Empty == off.
# A key generated from the web admin is persisted to API_KEY_PATH and takes
# precedence over this env var (so it can be rotated without a restart).
API_KEY = os.environ.get("API_KEY", "")
API_KEY_PATH = Path(os.environ.get("API_KEY_PATH", "data/.apikey"))

# auto_error=False so a missing/!malformed Authorization header does NOT raise
# before we get a chance to check the ?key= query param (the Apple IIc client
# can only authenticate via the query string — it cannot send headers).
_bearer = HTTPBearer(auto_error=False)

# ---------------------------------------------------------------------------
# htpasswd helpers
# ---------------------------------------------------------------------------


def _read_htpasswd(path: Path) -> dict[str, str]:
    """Return ``{username: hash}`` from an htpasswd file."""
    users: dict[str, str] = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(":", 1)
        if len(parts) == 2:
            users[parts[0]] = parts[1]
    return users


def _write_htpasswd(path: Path, users: dict[str, str]) -> None:
    """Write ``{username: hash}`` to an htpasswd file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = "".join(f"{u}:{h}\n" for u, h in users.items())
    path.write_text(lines)


# ---------------------------------------------------------------------------
# Password verification
# ---------------------------------------------------------------------------


def verify_password(username: str, password: str) -> bool:
    """Return True if *username*/*password* match an entry in the htpasswd file."""
    if not HTPASSWD_PATH.exists():
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=(
                "Admin credentials not configured. "
                "Run `uv run python scripts/set_password.py` to set a password."
            ),
        )
    users = _read_htpasswd(HTPASSWD_PATH)
    stored = users.get(username)
    if not stored:
        return False
    # Accept both $2b$ (Python bcrypt) and $2y$ (Apache) prefixes.
    stored_bytes = stored.encode()
    if stored_bytes.startswith(b"$2y$"):
        stored_bytes = b"$2b$" + stored_bytes[4:]
    return _bcrypt.checkpw(password.encode(), stored_bytes)


# ---------------------------------------------------------------------------
# Token creation
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# API key store (generated/rotated from the web admin)
# ---------------------------------------------------------------------------


def _read_stored_api_key() -> str:
    """Return the persisted API key, or '' if none has been generated."""
    try:
        return API_KEY_PATH.read_text().strip()
    except FileNotFoundError:
        return ""


def get_active_api_key() -> tuple[str, str]:
    """Return ``(key, source)`` for the currently active client API key.

    A generated key file takes precedence over the ``API_KEY`` env var so the
    admin can rotate the key at runtime.  ``source`` is ``file``, ``env`` or
    ``none``.
    """
    stored = _read_stored_api_key()
    if stored:
        return stored, "file"
    if API_KEY:
        return API_KEY, "env"
    return "", "none"


def persist_api_key(key: str) -> str:
    """Persist a client API key to disk and return the normalized key."""
    key = key.strip().lower()
    if len(key) != 10 or any(c not in '0123456789abcdef' for c in key):
        raise ValueError("API key must be 10 hex characters")
    API_KEY_PATH.parent.mkdir(parents=True, exist_ok=True)
    API_KEY_PATH.write_text(key)
    try:
        API_KEY_PATH.chmod(0o600)
    except OSError:
        pass  # best-effort on filesystems without POSIX permissions
    return key


def generate_api_key() -> str:
    """Generate a new client API key, persist it, and return it.

    10 hex chars (40 bits) — deliberately short so it's easy to type by hand on
    the Apple IIc client. Fine for a trusted-LAN hobby tool; not a high-security
    credential.
    """
    return persist_api_key(secrets.token_hex(5))


def create_access_token(username: str) -> str:
    """Return a signed JWT for *username* valid for TOKEN_EXPIRE_HOURS hours."""
    payload = {
        "sub": username,
        "exp": datetime.now(timezone.utc) + timedelta(hours=TOKEN_EXPIRE_HOURS),
    }
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)


# ---------------------------------------------------------------------------
# FastAPI dependency — protects admin endpoints
# ---------------------------------------------------------------------------


def _verify_bearer(credentials: HTTPAuthorizationCredentials | None) -> str:
    """Validate a Bearer JWT and return the username, or raise 401."""
    exc = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Invalid or expired token",
        headers={"WWW-Authenticate": "Bearer"},
    )
    if credentials is None:
        raise exc
    try:
        payload = jwt.decode(
            credentials.credentials, SECRET_KEY, algorithms=[ALGORITHM]
        )
        sub: str = payload.get("sub", "")
        if not sub:
            raise exc
        return sub
    except JWTError:
        raise exc


def require_admin(
    credentials: HTTPAuthorizationCredentials | None = Depends(_bearer),
) -> str:
    """FastAPI dependency: validate Bearer token and return the username.

    Raises HTTP 401 if the token is missing, malformed, or expired.
    """
    return _verify_bearer(credentials)


def require_admin_or_key(
    credentials: HTTPAuthorizationCredentials | None = Depends(_bearer),
    key: str | None = Query(default=None),
) -> str:
    """FastAPI dependency accepting EITHER a Bearer JWT (web admin) OR a
    matching ``?key=`` query param (Apple IIc client).

    The IWM firmware cannot send request headers, so the client authenticates
    by appending ``?key=<API_KEY>`` to the request URL.  The web frontend keeps
    using ``Authorization: Bearer``.  Raises 401 if neither succeeds.
    """
    active_key, _ = get_active_api_key()
    if active_key and key is not None and secrets.compare_digest(key, active_key):
        return "apikey"
    return _verify_bearer(credentials)
