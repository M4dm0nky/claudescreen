"""Röktestets eget facit: varje kamsteg mot riggade servrar och filer.

Ingen testklass rör nätet utom via en lokal kannad HTTP-server på port 0,
och ingen rör hemkatalogen — allt tillstånd bor i tempkataloger.
"""

import contextlib
import io
import json
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

if __package__:
    from . import smoke
else:
    import smoke


HEALTHY_ROOT = {
    "service": "torget-tokenserver",
    "rev": "abc1234",
    "startedAt": "2026-08-13T20:00:00+02:00",
    "claudeProbe": "usage_http_200 + ok",
    "ratelimitHeaders": [],
    "unknownRateLimitBuckets": [],
}
HEALTHY_TOKENS = {"v": 1, "dayTokens": 1, "claudeWeekStale": False,
                  "codexWeekStale": False}


@contextlib.contextmanager
def canned_server(payloads, status=200):
    """Lokal HTTP-server på port 0 som svarar med riggade JSON-kroppar."""

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            body = json.dumps(payloads.get(self.path,
                                           {"error": "not found"})).encode()
            self.send_response(status if self.path in payloads else 404)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt, *args):
            pass

    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{srv.server_address[1]}"
    finally:
        srv.shutdown()
        srv.server_close()


def levels(results):
    return [level for level, _ in results]


class ServerCheckTests(unittest.TestCase):
    def test_healthy_server_with_matching_rev_is_all_ok(self):
        with canned_server({"/": HEALTHY_ROOT}) as base:
            results = smoke.check_server(base, checkout_rev="abc1234")
        self.assertEqual(levels(results), [smoke.OK, smoke.OK])

    def test_unreachable_server_is_fail(self):
        results = smoke.check_server("http://127.0.0.1:9")  # discard-porten
        self.assertEqual(levels(results), [smoke.FAIL])

    def test_rev_mismatch_warns_about_stale_worktree(self):
        with canned_server({"/": HEALTHY_ROOT}) as base:
            results = smoke.check_server(base, checkout_rev="fff9999")
        self.assertIn(smoke.VARN, levels(results))
        self.assertIn("WorkingDirectory", results[0][1])

    def test_probe_status_other_than_ok_warns_with_runbook_pointer(self):
        root = dict(HEALTHY_ROOT, claudeProbe="usage_http_401")
        with canned_server({"/": root}) as base:
            results = smoke.check_server(base, checkout_rev="abc1234")
        warn = [text for level, text in results if level == smoke.VARN]
        self.assertEqual(len(warn), 1)
        self.assertIn("usage_http_401", warn[0])
        self.assertIn("agent-setup", warn[0])

    def test_unknown_buckets_warn(self):
        root = dict(HEALTHY_ROOT, unknownRateLimitBuckets=["7d_haiku"])
        with canned_server({"/": root}) as base:
            results = smoke.check_server(base, checkout_rev="abc1234")
        self.assertIn(smoke.VARN, levels(results))

    def test_wrong_service_on_port_is_fail(self):
        with canned_server({"/": {"service": "annan"}}) as base:
            results = smoke.check_server(base, checkout_rev="abc1234")
        self.assertEqual(levels(results), [smoke.FAIL])


class EndpointCheckTests(unittest.TestCase):
    def test_three_healthy_endpoints_are_ok(self):
        payloads = {"/api/tokens": HEALTHY_TOKENS,
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1, "stale": False}}
        with canned_server(payloads) as base:
            results = smoke.check_endpoints(base)
        self.assertEqual(levels(results), [smoke.OK] * 3)

    def test_error_form_is_fail_even_on_http_500(self):
        payloads = {"/api/tokens": {"error": "internal server error"},
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1}}
        with canned_server(payloads, status=500) as base:
            results = smoke.check_endpoints(base)
        self.assertEqual(levels(results)[0], smoke.FAIL)
        self.assertIn("internal server error", results[0][1])

    def test_stale_flags_warn(self):
        payloads = {"/api/tokens": dict(HEALTHY_TOKENS,
                                        claudeWeekStale=True),
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1, "stale": True}}
        with canned_server(payloads) as base:
            results = smoke.check_endpoints(base)
        self.assertEqual(levels(results),
                         [smoke.VARN, smoke.OK, smoke.VARN])


class LogFileCheckTests(unittest.TestCase):
    def test_missing_file_is_a_warning_not_a_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            results = smoke.check_log_file(Path(tmp) / "finns-inte.log")
        self.assertEqual(levels(results), [smoke.VARN])

    def test_small_clean_file_is_ok(self):
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / "t.log"
            f.write_text("2026-08-13 INFO serverar http://0.0.0.0:8737\n")
            results = smoke.check_log_file(f)
        self.assertEqual(levels(results), [smoke.OK])

    def test_tracebacks_and_respawn_churn_warn(self):
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / "t.log"
            f.write_text(
                "Traceback (most recent call last)\n boom\n" +
                "serverar http://0.0.0.0:8737\n" * 12)
            results = smoke.check_log_file(f)
        self.assertEqual(levels(results),
                         [smoke.OK, smoke.VARN, smoke.VARN])
        self.assertIn("traceback", results[1][1])
        self.assertIn("respawn", results[2][1])


class StateFileCheckTests(unittest.TestCase):
    def _write_state(self, tmp, **overrides):
        for name in smoke.STATE_FILES:
            (Path(tmp) / name).write_text(overrides.get(name, "{}"))

    def test_valid_files_are_ok_and_fresh_history_stays_quiet(self):
        with tempfile.TemporaryDirectory() as tmp:
            self._write_state(tmp)
            results = smoke.check_state_files(tmp, now_ts=time.time())
        self.assertEqual(levels(results), [smoke.OK] * 3)

    def test_corrupt_state_file_is_fail_with_forensics_pointer(self):
        with tempfile.TemporaryDirectory() as tmp:
            self._write_state(tmp, **{"max-tracker.json": "{trasig"})
            results = smoke.check_state_files(tmp, now_ts=time.time())
        fails = [text for level, text in results if level == smoke.FAIL]
        self.assertEqual(len(fails), 1)
        self.assertIn("KORRUPT", fails[0])
        self.assertIn("forensik", fails[0])

    def test_missing_files_and_missing_dir_warn(self):
        with tempfile.TemporaryDirectory() as tmp:
            results = smoke.check_state_files(Path(tmp) / "finns-inte")
            self.assertEqual(levels(results), [smoke.VARN])
            results = smoke.check_state_files(tmp)
            self.assertEqual(levels(results), [smoke.VARN] * 3)

    def test_stale_usage_history_warns(self):
        with tempfile.TemporaryDirectory() as tmp:
            self._write_state(tmp)
            results = smoke.check_state_files(
                tmp, now_ts=time.time() + 25 * 3600)
        self.assertEqual(
            levels(results), [smoke.OK, smoke.VARN, smoke.OK, smoke.OK])


class RunExitCodeTests(unittest.TestCase):
    def _run(self, payloads, status=200):
        with tempfile.TemporaryDirectory() as tmp:
            log_file = Path(tmp) / "t.log"
            log_file.write_text("serverar http://0.0.0.0:8737\n")
            state = Path(tmp) / "state"
            state.mkdir()
            for name in smoke.STATE_FILES:
                (state / name).write_text("{}")
            out = io.StringIO()
            with canned_server(payloads, status=status) as base:
                code = smoke.run(base, log_path=log_file, state_dir=state,
                                 checkout_rev="abc1234", out=out)
            return code, out.getvalue()

    def test_healthy_chain_exits_0(self):
        payloads = {"/": HEALTHY_ROOT,
                    "/api/tokens": HEALTHY_TOKENS,
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1, "stale": False}}
        code, text = self._run(payloads)
        self.assertEqual(code, 0, text)
        self.assertIn("röktest:", text)
        self.assertNotIn("[FAIL]", text)

    def test_warnings_exit_1(self):
        payloads = {"/": dict(HEALTHY_ROOT, claudeProbe="not_run"),
                    "/api/tokens": HEALTHY_TOKENS,
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1, "stale": False}}
        code, text = self._run(payloads)
        self.assertEqual(code, 1, text)

    def test_error_form_exits_2(self):
        payloads = {"/": HEALTHY_ROOT,
                    "/api/tokens": {"error": "internal server error"},
                    "/api/agent-status": {"v": 1},
                    "/api/max-tracker": {"v": 1, "stale": False}}
        code, text = self._run(payloads)
        self.assertEqual(code, 2, text)

    def test_unreachable_server_skips_endpoint_checks_and_exits_2(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = io.StringIO()
            code = smoke.run("http://127.0.0.1:9",
                             log_path=Path(tmp) / "t.log",
                             state_dir=Path(tmp) / "state",
                             checkout_rev="abc1234", out=out)
        self.assertEqual(code, 2)
        self.assertNotIn("/api/tokens", out.getvalue())


if __name__ == "__main__":
    unittest.main()
