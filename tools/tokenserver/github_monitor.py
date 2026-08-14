"""Small, failure-isolated monitor for one public GitHub repository.

The display never talks to GitHub directly. This service keeps the public API's
TLS, rate limiting, backoff and response shape on the computer that is already
running VibePulse's tokenserver, then publishes one flat LAN payload.
"""

from __future__ import annotations

import json
import logging
import re
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone


log = logging.getLogger("tokenserver.github")

POLL_SECONDS = 120.0
EVENT_TTL_SECONDS = 10 * 60.0
FAILURE_BACKOFF_SECONDS = 10 * 60.0
STALE_AFTER_SECONDS = 5 * 60.0
_REPO_RE = re.compile(r"^[A-Za-z0-9_.-]{1,100}/[A-Za-z0-9_.-]{1,100}$")


def normalize_repo(value: str) -> str:
    """Validate and return an owner/repository public-repo identifier."""
    value = (value or "").strip()
    if not _REPO_RE.fullmatch(value):
        raise ValueError("GitHub repository must be owner/repository")
    return value


class GitHubMonitor:
    """Poll one public repository without ever raising into tokenserver."""

    def __init__(self, repo, *, opener=None, clock=None, wall_clock=None,
                 poll_seconds=POLL_SECONDS):
        self.repo = normalize_repo(repo)
        self._opener = opener or urllib.request.urlopen
        self._clock = clock or time.monotonic
        self._wall_clock = wall_clock or time.time
        self.poll_seconds = float(poll_seconds)
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None
        self._stars = None
        self._forks = None
        self._project = self.repo.split("/", 1)[1]
        self._event = None
        self._event_seen_at = None
        self._last_success_at = None
        self._last_error = None
        self._next_poll_at = 0.0

    @staticmethod
    def _headers(stargazers=False):
        return {
            "Accept": ("application/vnd.github.star+json" if stargazers
                       else "application/vnd.github+json"),
            "User-Agent": "VibePulse-public-repo-monitor/1",
            "X-GitHub-Api-Version": "2022-11-28",
        }

    def _fetch_json(self, url, *, stargazers=False):
        request = urllib.request.Request(
            url, headers=self._headers(stargazers=stargazers))
        with self._opener(request, timeout=8) as response:
            if getattr(response, "status", 200) != 200:
                raise RuntimeError(f"GitHub returned HTTP {response.status}")
            raw = response.read(256 * 1024 + 1)
            if len(raw) > 256 * 1024:
                raise ValueError("GitHub response exceeded 256 KiB")
        return json.loads(raw)

    @staticmethod
    def _parse_star_time(value):
        if not isinstance(value, str):
            return None
        try:
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
            return parsed if parsed.tzinfo is not None else None
        except ValueError:
            return None

    def _latest_stargazer(self, count):
        page = max(1, (count - 1) // 100 + 1)
        owner, repo = (urllib.parse.quote(part, safe="")
                       for part in self.repo.split("/", 1))
        url = (f"https://api.github.com/repos/{owner}/{repo}/stargazers"
               f"?per_page=100&page={page}")
        payload = self._fetch_json(url, stargazers=True)
        if not isinstance(payload, list):
            raise ValueError("GitHub stargazers response was not a list")

        candidates = []
        for item in payload:
            if not isinstance(item, dict):
                continue
            user = item.get("user")
            login = user.get("login") if isinstance(user, dict) else None
            starred_at = self._parse_star_time(item.get("starred_at"))
            if isinstance(login, str) and login and starred_at is not None:
                candidates.append((starred_at, login))
        if not candidates:
            return None, None
        starred_at, login = max(candidates, key=lambda pair: pair[0])
        return login, starred_at.astimezone(timezone.utc).isoformat().replace(
            "+00:00", "Z")

    @staticmethod
    def _retry_delay(error, wall_now):
        if not isinstance(error, urllib.error.HTTPError):
            return FAILURE_BACKOFF_SECONDS
        if error.code not in (403, 429):
            return FAILURE_BACKOFF_SECONDS
        headers = error.headers or {}
        try:
            retry_after = float(headers.get("Retry-After", 0))
        except (TypeError, ValueError):
            retry_after = 0
        try:
            reset_after = float(headers.get("X-RateLimit-Reset", 0)) - wall_now
        except (TypeError, ValueError):
            reset_after = 0
        return max(FAILURE_BACKOFF_SECONDS, retry_after, reset_after, 0)

    def poll_once(self):
        """Perform one metadata poll. All failures become cached state."""
        now = self._clock()
        owner, repo = (urllib.parse.quote(part, safe="")
                       for part in self.repo.split("/", 1))
        url = f"https://api.github.com/repos/{owner}/{repo}"
        try:
            payload = self._fetch_json(url)
            if not isinstance(payload, dict):
                raise ValueError("GitHub repository response was not an object")
            if payload.get("private") is not False:
                raise ValueError("configured GitHub repository is not public")
            stars = payload.get("stargazers_count")
            forks = payload.get("forks_count")
            project = payload.get("name")
            if (isinstance(stars, bool) or not isinstance(stars, int) or
                    stars < 0 or stars > 2_147_483_647):
                raise ValueError("invalid GitHub star count")
            if (isinstance(forks, bool) or not isinstance(forks, int) or
                    forks < 0 or forks > 2_147_483_647):
                raise ValueError("invalid GitHub fork count")
            if not isinstance(project, str) or not project or len(project) > 100:
                raise ValueError("invalid GitHub repository name")

            with self._lock:
                previous = self._stars

            event = None
            if previous is not None and stars > previous:
                actor = None
                starred_at = None
                try:
                    actor, starred_at = self._latest_stargazer(stars)
                except Exception as exc:
                    # The count is still authoritative. A missing actor must
                    # never make the whole GitHub feed stale or retry-storm.
                    log.warning("kunde inte läsa senaste stargazer för %s: %s",
                                self.repo, exc)
                event = {
                    "eventId": starred_at or f"count:{stars}",
                    "actor": actor,
                    "eventStars": stars,
                }

            with self._lock:
                self._stars = stars
                self._forks = forks
                self._project = project
                self._last_success_at = now
                self._last_error = None
                self._next_poll_at = now + self.poll_seconds
                if event is not None:
                    self._event = event
                    self._event_seen_at = now
            return True
        except Exception as exc:
            delay = self._retry_delay(exc, self._wall_clock())
            with self._lock:
                self._last_error = f"{type(exc).__name__}: {exc}"
                self._next_poll_at = now + delay
            log.warning("GitHub-pollning misslyckades för %s; ny chans om %ds: %s",
                        self.repo, int(delay), exc)
            return False

    def snapshot(self):
        now = self._clock()
        with self._lock:
            stars = self._stars
            forks = self._forks
            last_success = self._last_success_at
            event = dict(self._event) if self._event is not None else None
            event_seen_at = self._event_seen_at
            last_error = self._last_error
            project = self._project

        result = {
            "v": 1,
            "enabled": True,
            "repo": self.repo,
            "project": project,
            "stale": (last_success is None or last_error is not None or
                      now - last_success > STALE_AFTER_SECONDS),
        }
        if stars is not None:
            result["stars"] = stars
        if forks is not None:
            result["forks"] = forks
        if (event is not None and event_seen_at is not None and
                now - event_seen_at <= EVENT_TTL_SECONDS):
            result.update(event)
        return result

    def run(self):
        while not self._stop.is_set():
            with self._lock:
                delay = max(0.0, self._next_poll_at - self._clock())
            if self._stop.wait(delay):
                break
            self.poll_once()

    def start(self):
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._thread = threading.Thread(
            target=self.run, name="github-monitor", daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=max(1.0, min(self.poll_seconds, 5.0)))


def disabled_snapshot():
    return {"v": 1, "enabled": False}
