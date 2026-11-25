#!/usr/bin/env python3
"""End to end tests for the friendship streak server.

Runs a real HTTP server on a random port with a temporary database and drives
it over HTTP, exactly like the mod does.
"""

from __future__ import annotations

import json
import os
import socket
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

os.environ["FSTREAK_ALLOW_DAY_OVERRIDE"] = "1"

import server as streak_server  # noqa: E402


class Client:
    def __init__(self, base: str) -> None:
        self.base = base
        self.token = ""

    def call(self, method: str, path: str, body: dict | None = None) -> tuple[int, dict]:
        data = json.dumps(body or {}).encode()
        request = urllib.request.Request(
            self.base + path,
            data=data if method in ("POST", "DELETE") else None,
            method=method,
            headers={"Content-Type": "application/json"},
        )
        if self.token:
            request.add_header("Authorization", f"Bearer {self.token}")
        try:
            with urllib.request.urlopen(request) as response:
                return response.status, json.loads(response.read() or b"{}")
        except urllib.error.HTTPError as err:
            return err.code, json.loads(err.read() or b"{}")


class ServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        cls.server = streak_server.make_server(
            "127.0.0.1", 0, os.path.join(cls.tmp.name, "test.sqlite3")
        )
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        host, port = cls.server.server_address[:2]
        cls.base = f"http://{host}:{port}"
        cls.store = cls.server.RequestHandlerClass.store

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()
        cls.server.server_close()
        cls.tmp.cleanup()

    def setUp(self) -> None:
        # A fresh database per test keeps the assertions independent.
        self.store._db.executescript("DELETE FROM events; DELETE FROM pairs; DELETE FROM invites;")
        self.store._db.commit()
        self.a = Client(self.base)
        self.b = Client(self.base)
        for client, account, name in ((self.a, 101, "Alpha"), (self.b, 202, "Beta")):
            status, payload = client.call("POST", "/v1/register", {"accountId": account, "username": name})
            self.assertEqual(status, 200, payload)
            client.token = payload["token"]
            self.assertEqual(payload["username"], name)

    def day(self, value: str) -> None:
        os.environ["FSTREAK_TEST_DAY"] = value

    def pair(self) -> dict:
        status, invite = self.a.call("POST", "/v1/pairs/invite")
        self.assertEqual(status, 200, invite)
        status, pair = self.b.call("POST", "/v1/pairs/redeem", {"code": invite["code"]})
        self.assertEqual(status, 200, pair)
        return pair

    def send(self, client: Client, pair_id: str, kind: str = "sent") -> dict:
        status, payload = client.call("POST", f"/v1/pairs/{pair_id}/events", {"kind": kind})
        self.assertEqual(status, 200, payload)
        return payload

    def state(self, client: Client) -> dict:
        status, payload = client.call("GET", "/v1/pairs")
        self.assertEqual(status, 200, payload)
        return payload

    # -- tests -----------------------------------------------------------

    def test_registration_rotates_token(self) -> None:
        status, payload = self.a.call("POST", "/v1/register", {"accountId": 101, "username": "Alpha"})
        self.assertEqual(status, 200)
        self.assertNotEqual(payload["token"], self.a.token)
        self.a.token = payload["token"]

    def test_pairing_requires_mutual_codes(self) -> None:
        status, _ = self.b.call("POST", "/v1/pairs/redeem", {"code": "NOPE00"})
        self.assertEqual(status, 404)

        status, invite = self.a.call("POST", "/v1/pairs/invite")
        self.assertEqual(status, 200)
        status, _ = self.a.call("POST", "/v1/pairs/redeem", {"code": invite["code"]})
        self.assertEqual(status, 400, "self pairing must be rejected")

        status, _ = self.b.call("POST", "/v1/pairs/redeem", {"code": invite["code"]})
        self.assertEqual(status, 200)
        status, _ = self.b.call("POST", "/v1/pairs/redeem", {"code": invite["code"]})
        self.assertEqual(status, 404, "a used code cannot be reused")

    def test_streak_needs_both_sides(self) -> None:
        self.day("2026-01-01")
        pair = self.pair()
        pair_id = pair["id"]

        state = self.send(self.a, pair_id)
        self.assertEqual(state["current"], 0)
        self.assertTrue(state["today"]["youSent"])
        self.assertFalse(state["today"]["friendSent"])

        state = self.send(self.b, pair_id)
        self.assertEqual(state["current"], 1)
        self.assertEqual(state["best"], 1)
        self.assertEqual(state["startDay"], "2026-01-01")
        self.assertTrue(state["today"]["mutual"])

        # Repeated messages on the same day never count twice.
        self.send(self.a, pair_id)
        self.send(self.b, pair_id)
        state = self.send(self.a, pair_id)
        self.assertEqual(state["current"], 1)

    def test_receiving_alone_does_not_count(self) -> None:
        self.day("2026-02-01")
        pair_id = self.pair()["id"]
        self.send(self.a, pair_id, "received")
        state = self.send(self.b, pair_id, "received")
        self.assertEqual(state["current"], 0)
        self.assertFalse(state["today"]["mutual"])

    def test_consecutive_days_and_grace(self) -> None:
        pair_id = self.pair()["id"]

        self.day("2026-03-01")
        self.send(self.a, pair_id)
        self.send(self.b, pair_id)

        self.day("2026-03-02")
        state = self.send(self.a, pair_id)
        self.assertEqual(state["current"], 1, "today is not mutual yet, yesterday still holds")
        state = self.send(self.b, pair_id)
        self.assertEqual(state["current"], 2)
        self.assertEqual(state["startDay"], "2026-03-01")

        self.day("2026-03-03")
        state = self.send(self.a, pair_id)
        self.assertEqual(state["current"], 2)
        self.assertEqual(state["best"], 2)

    def test_missed_day_breaks_and_best_is_kept(self) -> None:
        pair_id = self.pair()["id"]
        for day in ("2026-04-01", "2026-04-02", "2026-04-03"):
            self.day(day)
            self.send(self.a, pair_id)
            self.send(self.b, pair_id)

        self.day("2026-04-04")  # nobody writes
        state = self.state(self.a)["pairs"][0]
        self.assertEqual(state["current"], 3, "today is still undecided")

        self.day("2026-04-05")
        state = self.state(self.a)["pairs"][0]
        self.assertEqual(state["current"], 0)
        self.assertEqual(state["best"], 3)
        self.assertIsNone(state["startDay"])

        state = self.send(self.a, pair_id)
        state = self.send(self.b, pair_id)
        self.assertEqual(state["current"], 1)
        self.assertEqual(state["best"], 3)

    def test_history_and_viewer_perspective(self) -> None:
        self.day("2026-05-01")
        pair_id = self.pair()["id"]
        self.send(self.a, pair_id)
        state = self.send(self.b, pair_id)

        self.assertEqual(len(state["days"]), 14)
        self.assertEqual(state["days"][-1]["day"], "2026-05-01")
        self.assertTrue(state["days"][-1]["mutual"])
        self.assertEqual(state["you"]["accountId"], 202)
        self.assertEqual(state["friend"]["accountId"], 101)
        self.assertEqual(state["friend"]["username"], "Alpha")

        other = self.state(self.a)["pairs"][0]
        self.assertEqual(other["you"]["accountId"], 101)
        self.assertEqual(other["friend"]["accountId"], 202)

    def test_auth_and_membership(self) -> None:
        pair_id = self.pair()["id"]

        stranger = Client(self.base)
        status, payload = stranger.call(
            "POST", "/v1/register", {"accountId": 303, "username": "Gamma"}
        )
        self.assertEqual(status, 200)
        stranger.token = payload["token"]
        status, _ = stranger.call("POST", f"/v1/pairs/{pair_id}/events", {"kind": "sent"})
        self.assertEqual(status, 403)

        bad = Client(self.base)
        bad.token = "not-a-real-token"
        status, _ = bad.call("GET", "/v1/pairs")
        self.assertEqual(status, 401)

        status, _ = bad.call("POST", "/v1/pairs/invite")
        self.assertEqual(status, 401)

    def test_unlink_removes_pair_for_both(self) -> None:
        pair_id = self.pair()["id"]
        status, _ = self.a.call("DELETE", f"/v1/pairs/{pair_id}")
        self.assertEqual(status, 200)
        self.assertEqual(self.state(self.a)["pairs"], [])
        self.assertEqual(self.state(self.b)["pairs"], [])

    def test_health(self) -> None:
        status, payload = Client(self.base).call("GET", "/v1/health")
        self.assertEqual(status, 200)
        self.assertTrue(payload["ok"])

    def test_wire_contract_matches_mod(self) -> None:
        """Every key the mod parses in PairState::fromServer must be present."""
        self.day("2026-05-01")
        pair = self.pair()
        self.send(self.a, pair["id"])
        self.send(self.b, pair["id"])

        status, payload = self.a.call("GET", "/v1/pairs")
        self.assertEqual(status, 200, payload)
        self.assertIn("pairs", payload)
        state = payload["pairs"][0]

        for key in ("id", "current", "best", "startDay", "lastStreakDay", "serverTime"):
            self.assertIn(key, state)
        self.assertIsInstance(state["serverTime"], int)
        self.assertIsInstance(state["current"], int)
        self.assertIsInstance(state["best"], int)

        for side, account in (("you", 101), ("friend", 202)):
            self.assertEqual(set(state[side]), {"accountId", "username"})
            self.assertEqual(state[side]["accountId"], account)

        self.assertEqual(set(state["today"]), {"day", "youSent", "friendSent", "mutual"})
        self.assertEqual(state["today"]["day"], "2026-05-01")
        self.assertTrue(state["today"]["mutual"])

        self.assertEqual(len(state["days"]), 14)
        for entry in state["days"]:
            self.assertEqual(set(entry), {"day", "mutual", "youSent", "friendSent"})
            self.assertIsInstance(entry["mutual"], bool)
            self.assertIsInstance(entry["youSent"], bool)
            self.assertIsInstance(entry["friendSent"], bool)
        self.assertEqual(state["days"][-1]["day"], "2026-05-01")


if __name__ == "__main__":
    unittest.main(verbosity=2)
