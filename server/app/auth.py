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
from fastapi import Depends, HTTPException, status
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

_bearer = HTTPBearer()

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


def require_admin(
    credentials: HTTPAuthorizationCredentials = Depends(_bearer),
) -> str:
    """FastAPI dependency: validate Bearer token and return the username.

    Raises HTTP 401 if the token is missing, malformed, or expired.
    """
    exc = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Invalid or expired token",
        headers={"WWW-Authenticate": "Bearer"},
    )
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
