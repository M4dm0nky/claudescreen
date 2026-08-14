import io
import json
import unittest
import urllib.error

from tools.tokenserver.github_monitor import (
    GitHubMonitor,
    disabled_snapshot,
    normalize_repo,
)


class Clock:
    def __init__(self, value=100.0):
        self.value = value

    def __call__(self):
        return self.value


class Response:
    status = 200

    def __init__(self, payload):
        self.payload = json.dumps(payload).encode()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def read(self, _limit):
        return self.payload


class Opener:
    def __init__(self, payloads):
        self.payloads = list(payloads)
        self.requests = []

    def __call__(self, request, timeout):
        self.requests.append((request, timeout))
        payload = self.payloads.pop(0)
        if isinstance(payload, Exception):
            raise payload
        return Response(payload)


def repo(stars, name="vibepulse", forks=3):
    return {"private": False, "stargazers_count": stars,
            "forks_count": forks, "name": name}


class GitHubMonitorTests(unittest.TestCase):
    def test_repo_validation_is_strict(self):
        self.assertEqual(normalize_repo(" niclas/vibepulse "),
                         "niclas/vibepulse")
        for invalid in ("", "owner", "https://github.com/a/b", "a/b/c",
                        "a b/repo"):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    normalize_repo(invalid)

    def test_first_poll_sets_baseline_without_replaying_old_star(self):
        clock = Clock()
        opener = Opener([repo(41)])
        monitor = GitHubMonitor("niclas/vibepulse", opener=opener,
                                clock=clock, wall_clock=clock)
        self.assertTrue(monitor.poll_once())
        self.assertEqual(monitor.snapshot(), {
            "v": 1, "enabled": True, "repo": "niclas/vibepulse",
            "project": "vibepulse", "stale": False, "stars": 41,
            "forks": 3,
        })
        self.assertEqual(opener.requests[0][1], 8)

    def test_count_increase_publishes_latest_actor_and_new_count(self):
        clock = Clock()
        opener = Opener([
            repo(41),
            repo(42),
            [
                {"starred_at": "2026-08-14T18:01:00Z",
                 "user": {"login": "earlier"}},
                {"starred_at": "2026-08-14T18:02:00Z",
                 "user": {"login": "octocat"}},
            ],
        ])
        monitor = GitHubMonitor("niclas/vibepulse", opener=opener,
                                clock=clock, wall_clock=clock)
        monitor.poll_once()
        clock.value += 120
        monitor.poll_once()
        snap = monitor.snapshot()
        self.assertEqual(snap["stars"], 42)
        self.assertEqual(snap["forks"], 3)
        self.assertEqual(snap["actor"], "octocat")
        self.assertEqual(snap["eventStars"], 42)
        self.assertEqual(snap["eventId"], "2026-08-14T18:02:00Z")
        star_request = opener.requests[-1][0]
        self.assertIn("/stargazers?per_page=100&page=1",
                      star_request.full_url)
        self.assertEqual(star_request.headers["Accept"],
                         "application/vnd.github.star+json")

    def test_count_only_event_survives_stargazer_failure(self):
        clock = Clock()
        opener = Opener([
            repo(7), repo(8), RuntimeError("stargazers unavailable")])
        monitor = GitHubMonitor("niclas/vibepulse", opener=opener,
                                clock=clock, wall_clock=clock)
        monitor.poll_once()
        clock.value += 120
        self.assertTrue(monitor.poll_once())
        snap = monitor.snapshot()
        self.assertFalse(snap["stale"])
        self.assertEqual(snap["eventId"], "count:8")
        self.assertIsNone(snap["actor"])

    def test_naive_stargazer_timestamp_falls_back_to_count_event(self):
        clock = Clock()
        opener = Opener([repo(7), repo(8), [
            {"starred_at": "2026-08-14T18:02:00",
             "user": {"login": "octocat"}},
        ]])
        monitor = GitHubMonitor("a/b", opener=opener, clock=clock,
                                wall_clock=clock)
        monitor.poll_once()
        clock.value += 120
        self.assertTrue(monitor.poll_once())
        snap = monitor.snapshot()
        self.assertEqual(snap["eventId"], "count:8")
        self.assertIsNone(snap["actor"])

    def test_failure_keeps_count_marks_stale_and_backs_off(self):
        clock = Clock()
        rate_error = urllib.error.HTTPError(
            "https://api.github.com/repos/a/b", 403, "limited",
            {"Retry-After": "900"}, io.BytesIO())
        opener = Opener([repo(12), rate_error])
        monitor = GitHubMonitor("a/b", opener=opener, clock=clock,
                                wall_clock=clock, poll_seconds=120)
        monitor.poll_once()
        clock.value += 120
        self.assertFalse(monitor.poll_once())
        snap = monitor.snapshot()
        self.assertEqual(snap["stars"], 12)
        self.assertTrue(snap["stale"])
        self.assertEqual(monitor._next_poll_at, clock.value + 900)

    def test_event_expires_without_erasing_star_count(self):
        clock = Clock()
        opener = Opener([repo(1), repo(2), [
            {"starred_at": "2026-08-14T18:02:00Z",
             "user": {"login": "octocat"}},
        ]])
        monitor = GitHubMonitor("a/b", opener=opener, clock=clock,
                                wall_clock=clock)
        monitor.poll_once()
        clock.value += 120
        monitor.poll_once()
        clock.value += 601
        snap = monitor.snapshot()
        self.assertEqual(snap["stars"], 2)
        self.assertNotIn("eventId", snap)

    def test_private_repository_is_rejected(self):
        clock = Clock()
        opener = Opener([{"private": True, "stargazers_count": 1,
                          "forks_count": 0, "name": "secret"}])
        monitor = GitHubMonitor("a/b", opener=opener, clock=clock,
                                wall_clock=clock)
        self.assertFalse(monitor.poll_once())
        self.assertTrue(monitor.snapshot()["stale"])

    def test_disabled_payload_is_tiny_and_explicit(self):
        self.assertEqual(disabled_snapshot(), {"v": 1, "enabled": False})


if __name__ == "__main__":
    unittest.main()
