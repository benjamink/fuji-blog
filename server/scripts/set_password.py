#!/usr/bin/env python3
"""Set or update the FujiBlogger admin password.

Creates/updates  data/.htpasswd  (or the path in HTPASSWD_PATH env var)
using bcrypt hashing compatible with Apache htpasswd format.

Usage (from the server/ directory):
    uv run python scripts/set_password.py
"""

import getpass
import os
import sys
from pathlib import Path

import bcrypt

HTPASSWD_PATH = Path(os.environ.get("HTPASSWD_PATH", "data/.htpasswd"))


def _read_users(path: Path) -> dict[str, str]:
    """Return {username: hash} from an existing htpasswd file."""
    users: dict[str, str] = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(":", 1)
        if len(parts) == 2:
            users[parts[0]] = parts[1]
    return users


def _write_users(path: Path, users: dict[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(f"{u}:{h}\n" for u, h in users.items()))


def main() -> None:
    users = _read_users(HTPASSWD_PATH) if HTPASSWD_PATH.exists() else {}

    username = input("Username [admin]: ").strip() or "admin"
    is_new = username not in users

    while True:
        password = getpass.getpass("Password: ")
        if not password:
            print("Password cannot be empty.", file=sys.stderr)
            continue
        confirm = getpass.getpass("Confirm password: ")
        if password != confirm:
            print("Passwords do not match.", file=sys.stderr)
            continue
        break

    hashed = bcrypt.hashpw(password.encode(), bcrypt.gensalt())
    users[username] = hashed.decode()
    _write_users(HTPASSWD_PATH, users)

    action = "Created" if is_new else "Updated"
    print(f"{action} credentials for '{username}' in {HTPASSWD_PATH}")
    print()
    print("Start the server with a persistent JWT secret:")
    print("  export JWT_SECRET=$(openssl rand -hex 32)")
    print("  uv run uvicorn app.main:app --host 0.0.0.0 --port 8000")


if __name__ == "__main__":
    main()
