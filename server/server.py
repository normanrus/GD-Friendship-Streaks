#!/usr/bin/env python3
"""GD Friendship Streaks sync server.

Standard library only: no dependencies, no paid services, no third party accounts.
State lives in a single SQLite file, so backing up the server is copying one file.

Run:
    python3 server.py --host 0.0.0.0 --port 8080 --db streaks.sqlite3

Streak rules (the server is the only authority, clients only report events):

    aSent(day)  - player A sent at least one direct message to B that day
    bSent(day)  - player B sent at least one direct message to A that day
    mutual(day) - aSent(day) and bSent(day)

    current = length of the run of mutual days that reaches today (or yesterday,
    while today is still undecided). A day that ends without a mutual day breaks
    the run; best keeps the record.

The day is the UTC day of the server clock. Clients cannot choose the day, so a
modified client cannot backdate or fast forward a streak.
"""

from __future__ import annotations

import argparse
import json
import os
import secrets
import sqlite3
import threading
import time
from datetime import date, datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Optional

SCHEMA = """
CREATE TABLE IF NOT EXISTS accounts (
    account_id INTEGER PRIMARY KEY,
    username   TEXT NOT NULL,
    token      TEXT NOT NULL UNIQUE,
    created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS pairs (
    id         TEXT PRIMARY KEY,
    a_account  INTEGER NOT NULL,
    a_user     TEXT NOT NULL,
    b_account  INTEGER NOT NULL,
    b_user     TEXT NOT NULL,
    created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS pairs_unique
    ON pairs (min(a_account, b_account), max(a_account, b_account));
CREATE TABLE IF NOT EXISTS invites (
    code       TEXT PRIMARY KEY,
    account_id INTEGER NOT NULL,
    username   TEXT NOT NULL,
    expires_at INTEGER NOT NULL,
    used       INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS events (
    pair_id    TEXT NOT NULL,
    account_id INTEGER NOT NULL,
    day        TEXT NOT NULL,
    kind       TEXT NOT NULL,
    ts         INTEGER NOT NULL,
    PRIMARY KEY (pair_id, account_id, day, kind)
);
"""

INVITE_TTL = 24 * 3600
HISTORY_DAYS = 14
RATE_LIMIT_PER_MINUTE = 120


class ApiError(Exception):
    def __init__(self, status: int, code: str, message: str = "") -> None:
        super().__init__(message or code)
        self.status = status
        self.code = code
        self.message = message or code


class Store:
    def __init__(self, path: str) -> None:
        self._lock = threading.Lock()
        self._db = sqlite3.connect(path, check_same_thread=False)
        self._db.row_factory = sqlite3.Row
        self._db.executescript(SCHEMA)
        self._db.commit()

    # -- helpers ---------------------------------------------------------

    def _now(self) -> int:
        return int(time.time())

    def today(self) -> str:
        override = os.environ.get("FSTREAK_TEST_DAY")
        if override and os.environ.get("FSTREAK_ALLOW_DAY_OVERRIDE") == "1":
            return override
        return datetime.now(timezone.utc).strftime("%Y-%m-%d")

    def account_by_token(self, token: str) -> sqlite3.Row:
        row = self._db.execute(
            "SELECT * FROM accounts WHERE token = ?", (token,)
        ).fetchone()
        if row is None:
            raise ApiError(401, "bad_token")
        return row

    # -- accounts --------------------------------------------------------

    def register(self, account_id: int, username: str) -> dict[str, Any]:
        if account_id <= 0:
            raise ApiError(400, "bad_account")
        username = (username or "").strip()[:64] or f"player{account_id}"
        token = secrets.token_urlsafe(32)
        with self._lock:
            # Re-registering rotates the token: clearing mod data on the same
            # account must not lock the player out, and a rotated token makes
            # an old leaked one useless.
            self._db.execute(
                "INSERT INTO accounts (account_id, username, token, created_at)"
                " VALUES (?, ?, ?, ?)"
                " ON CONFLICT(account_id) DO UPDATE SET username = ?, token = ?",
                (account_id, username, token, self._now(), username, token),
            )
            self._db.commit()
        return {"accountId": account_id, "username": username, "token": token}

    # -- pairing ---------------------------------------------------------

    def create_invite(self, account: sqlite3.Row) -> dict[str, Any]:
        code = "".join(secrets.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(6))
        expires = self._now() + INVITE_TTL
        with self._lock:
            self._db.execute(
                "INSERT INTO invites (code, account_id, username, expires_at) VALUES (?, ?, ?, ?)",
                (code, account["account_id"], account["username"], expires),
            )
            self._db.commit()
        return {"code": code, "expiresAt": expires}

    def redeem_invite(self, account: sqlite3.Row, code: str) -> dict[str, Any]:
        code = (code or "").strip().upper()
        with self._lock:
            invite = self._db.execute(
                "SELECT * FROM invites WHERE code = ?", (code,)
            ).fetchone()
            if invite is None or invite["used"]:
                raise ApiError(404, "invite_not_found")
            if invite["expires_at"] < self._now():
                raise ApiError(410, "invite_expired")
            if invite["account_id"] == account["account_id"]:
                raise ApiError(400, "self_pair")
            existing = self._db.execute(
                "SELECT id FROM pairs WHERE (a_account = ? AND b_account = ?)"
                " OR (a_account = ? AND b_account = ?)",
                (
                    invite["account_id"],
                    account["account_id"],
                    account["account_id"],
                    invite["account_id"],
                ),
            ).fetchone()
            if existing is not None:
                raise ApiError(409, "already_paired")
            pair_id = secrets.token_hex(8)
            self._db.execute(
                "INSERT INTO pairs (id, a_account, a_user, b_account, b_user, created_at)"
                " VALUES (?, ?, ?, ?, ?, ?)",
                (
                    pair_id,
                    invite["account_id"],
                    invite["username"],
                    account["account_id"],
                    account["username"],
                    self._now(),
                ),
            )
            self._db.execute("UPDATE invites SET used = 1 WHERE code = ?", (code,))
            self._db.commit()
        return self.pair_state(pair_id, account["account_id"])

    def unlink(self, account: sqlite3.Row, pair_id: str) -> dict[str, Any]:
        with self._lock:
            pair = self._pair(pair_id)
            self._require_member(pair, account["account_id"])
            self._db.execute("DELETE FROM pairs WHERE id = ?", (pair_id,))
            self._db.execute("DELETE FROM events WHERE pair_id = ?", (pair_id,))
            self._db.commit()
        return {"ok": True}

    # -- events ----------------------------------------------------------

    def record_event(self, account: sqlite3.Row, pair_id: str, kind: str) -> dict[str, Any]:
        if kind not in ("sent", "received"):
            raise ApiError(400, "bad_kind")
        with self._lock:
            pair = self._pair(pair_id)
            self._require_member(pair, account["account_id"])
            day = self.today()
            # Idempotent: one row per account, day and kind. Repeating the same
            # report (retries, multiple messages) never counts twice.
            self._db.execute(
                "INSERT OR IGNORE INTO events (pair_id, account_id, day, kind, ts)"
                " VALUES (?, ?, ?, ?, ?)",
                (pair_id, account["account_id"], day, kind, self._now()),
            )
            self._db.commit()
        return self.pair_state(pair_id, account["account_id"])

    # -- reads -----------------------------------------------------------

    def _pair(self, pair_id: str) -> sqlite3.Row:
        pair = self._db.execute("SELECT * FROM pairs WHERE id = ?", (pair_id,)).fetchone()
        if pair is None:
            raise ApiError(404, "pair_not_found")
        return pair

    def _require_member(self, pair: sqlite3.Row, account_id: int) -> None:
        if account_id not in (pair["a_account"], pair["b_account"]):
            raise ApiError(403, "not_a_member")

    def list_pairs(self, account: sqlite3.Row) -> dict[str, Any]:
        rows = self._db.execute(
            "SELECT * FROM pairs WHERE a_account = ? OR b_account = ? ORDER BY created_at",
            (account["account_id"], account["account_id"]),
        ).fetchall()
        return {
            "pairs": [self.pair_state(row["id"], account["account_id"]) for row in rows],
            "today": self.today(),
            "serverTime": self._now(),
        }

    def pair_state(self, pair_id: str, viewer_account: int) -> dict[str, Any]:
        pair = self._pair(pair_id)
        self._require_member(pair, viewer_account)

        rows = self._db.execute(
            "SELECT account_id, day, kind FROM events WHERE pair_id = ? ORDER BY day",
            (pair_id,),
        ).fetchall()

        sent: dict[str, set[int]] = {}
        for row in rows:
            if row["kind"] == "sent":
                sent.setdefault(row["day"], set()).add(row["account_id"])

        a, b = pair["a_account"], pair["b_account"]
        mutual_days = sorted(day for day, who in sent.items() if a in who and b in who)
        mutual = set(mutual_days)

        today = self.today()
        streak = compute_streak(mutual_days, today)
        history = recent_days(today, HISTORY_DAYS)

        viewer_is_a = viewer_account == a
        today_sent = sent.get(today, set())

        return {
            "id": pair["id"],
            "viewer": viewer_account,
            "you": {
                "accountId": viewer_account,
                "username": pair["a_user"] if viewer_is_a else pair["b_user"],
            },
            "friend": {
                "accountId": b if viewer_is_a else a,
                "username": pair["b_user"] if viewer_is_a else pair["a_user"],
            },
            "current": streak["current"],
            "best": streak["best"],
            "startDay": streak["startDay"],
            "lastStreakDay": streak["lastStreakDay"],
            "today": {
                "day": today,
                "youSent": viewer_account in today_sent,
                "friendSent": (b if viewer_is_a else a) in today_sent,
                "mutual": today in mutual,
            },
            "days": [
                {
                    "day": day,
                    "mutual": day in mutual,
                    "youSent": viewer_account in sent.get(day, set()),
                    "friendSent": (b if viewer_is_a else a) in sent.get(day, set()),
                }
                for day in history
            ],
            "serverTime": self._now(),
        }


def day_ordinal(day: str) -> int:
    return date.fromisoformat(day).toordinal()


def compute_streak(mutual_days: list[str], today: str) -> dict[str, Any]:
    """Current run, record and the first day of the current run."""
    best = 0
    run = 0
    previous: Optional[int] = None
    best_end: Optional[int] = None
    for day in mutual_days:
        ordinal = day_ordinal(day)
        if previous is not None and ordinal == previous + 1:
            run += 1
        else:
            run = 1
        previous = ordinal
        if run > best:
            best = run
            best_end = ordinal

    current = 0
    start: Optional[int] = None
    last: Optional[int] = None
    if mutual_days:
        today_ordinal = day_ordinal(today)
        mutual_set = {day_ordinal(d) for d in mutual_days}
        # The run may end today, or yesterday while today is still undecided.
        anchor = today_ordinal if today_ordinal in mutual_set else today_ordinal - 1
        if anchor in mutual_set:
            cursor = anchor
            while cursor in mutual_set:
                current += 1
                cursor -= 1
            start = cursor + 1
            last = anchor

    return {
        "current": current,
        "best": best,
        "startDay": date.fromordinal(start).isoformat() if start else None,
        "lastStreakDay": date.fromordinal(last).isoformat() if last else None,
        "bestEnd": date.fromordinal(best_end).isoformat() if best_end else None,
    }


def recent_days(today: str, count: int) -> list[str]:
    end = day_ordinal(today)
    return [date.fromordinal(end - offset).isoformat() for offset in range(count - 1, -1, -1)]


class RateLimiter:
    def __init__(self, limit: int) -> None:
        self._limit = limit
        self._lock = threading.Lock()
        self._buckets: dict[str, list[float]] = {}

    def allow(self, key: str) -> bool:
        now = time.monotonic()
        with self._lock:
            bucket = [t for t in self._buckets.get(key, []) if now - t < 60.0]
            if len(bucket) >= self._limit:
                self._buckets[key] = bucket
                return False
            bucket.append(now)
            self._buckets[key] = bucket
            return True


class Handler(BaseHTTPRequestHandler):
    store: Store
    limiter: RateLimiter
    server_version = "FriendshipStreaks/1"

    def log_message(self, fmt: str, *args: Any) -> None:  # keep stdout quiet
        if os.environ.get("FSTREAK_VERBOSE") == "1":
            super().log_message(fmt, *args)

    # -- plumbing --------------------------------------------------------

    def _body(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        try:
            parsed = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise ApiError(400, "bad_json")
        if not isinstance(parsed, dict):
            raise ApiError(400, "bad_json")
        return parsed

    def _token(self, body: dict[str, Any]) -> str:
        header = self.headers.get("Authorization") or ""
        if header.lower().startswith("bearer "):
            return header[7:].strip()
        return str(body.get("token") or "")

    def _send(self, status: int, payload: dict[str, Any]) -> None:
        data = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _dispatch(self, method: str) -> None:
        try:
            path = self.path.split("?", 1)[0].rstrip("/") or "/"
            body = self._body() if method in ("POST", "PATCH", "DELETE") else {}
            if not self.limiter.allow(self.client_address[0]):
                raise ApiError(429, "rate_limited")
            status, payload = self._route(method, path, body)
            self._send(status, payload)
        except ApiError as err:
            self._send(err.status, {"error": err.code, "message": err.message})
        except Exception as err:  # noqa: BLE001 - never leak a stack trace
            self._send(500, {"error": "internal", "message": str(err)})

    def _route(self, method: str, path: str, body: dict[str, Any]) -> tuple[int, dict[str, Any]]:
        if method == "GET" and path == "/v1/health":
            return 200, {"ok": True, "serverTime": int(time.time()), "today": self.store.today()}

        if method == "POST" and path == "/v1/register":
            result = self.store.register(
                int(body.get("accountId") or 0), str(body.get("username") or "")
            )
            return 200, result

        if method == "POST" and path == "/v1/pairs/invite":
            account = self.store.account_by_token(self._token(body))
            return 200, self.store.create_invite(account)

        if method == "POST" and path == "/v1/pairs/redeem":
            account = self.store.account_by_token(self._token(body))
            return 200, self.store.redeem_invite(account, str(body.get("code") or ""))

        if method == "GET" and path == "/v1/pairs":
            token = self.headers.get("Authorization", "")
            token = token[7:].strip() if token.lower().startswith("bearer ") else ""
            if not token:
                raise ApiError(401, "bad_token")
            account = self.store.account_by_token(token)
            return 200, self.store.list_pairs(account)

        parts = path.strip("/").split("/")
        if len(parts) == 4 and parts[0] == "v1" and parts[1] == "pairs" and parts[3] == "events":
            if method != "POST":
                raise ApiError(405, "method_not_allowed")
            account = self.store.account_by_token(self._token(body))
            return 200, self.store.record_event(account, parts[2], str(body.get("kind") or ""))

        if len(parts) == 3 and parts[0] == "v1" and parts[1] == "pairs":
            if method == "DELETE":
                account = self.store.account_by_token(self._token(body))
                return 200, self.store.unlink(account, parts[2])
            if method == "GET":
                token = self.headers.get("Authorization", "")
                token = token[7:].strip() if token.lower().startswith("bearer ") else ""
                account = self.store.account_by_token(token)
                return 200, self.store.pair_state(parts[2], account["account_id"])

        raise ApiError(404, "not_found")

    def do_GET(self) -> None:  # noqa: N802 - http.server naming
        self._dispatch("GET")

    def do_POST(self) -> None:  # noqa: N802
        self._dispatch("POST")

    def do_DELETE(self) -> None:  # noqa: N802
        self._dispatch("DELETE")


def make_server(host: str, port: int, db_path: str) -> ThreadingHTTPServer:
    handler = type("BoundHandler", (Handler,), {})
    handler.store = Store(db_path)
    handler.limiter = RateLimiter(RATE_LIMIT_PER_MINUTE)
    server = ThreadingHTTPServer((host, port), handler)
    server.daemon_threads = True
    return server


def main() -> None:
    parser = argparse.ArgumentParser(description="GD Friendship Streaks sync server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", 8080)))
    parser.add_argument("--db", default=os.environ.get("FSTREAK_DB", "streaks.sqlite3"))
    args = parser.parse_args()

    server = make_server(args.host, args.port, args.db)
    print(f"friendship streaks server listening on {args.host}:{args.port} (db: {args.db})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
