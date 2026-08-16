#!/usr/bin/env python3
"""The panel, before there is a panel.

Polls the tokenserver exactly the way the ESP32 does — one GET per second —
draws the pending interaction at the same 480 x 480 proportions, and answers
with the same signed POST. It exists so the whole loop (Claude asks → the
shelf shows it → you tap → Claude continues) can be proved on the Mac alone,
before a single line of firmware is written.

    python3 tools/fake-panel.py                 # localhost
    python3 tools/fake-panel.py --host mac.local

Keys: [a] approve   [d] deny   [l] leave it (the terminal asks)
      [p] panic stop — deny everything parked   [q] quit

The server must be running with --interactions, and the device key must be
readable (VIBEPULSE_DEVICE_KEY, ~/.vibepulse-device-key, or
TK_VIBEPULSE_DEVICE_KEY in secrets.h).
"""
import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "tokenserver"))

from interactions import read_device_key, sign_answer  # noqa: E402

WIDTH = 52
ORANGE = "\033[38;5;173m"
GREEN = "\033[38;5;72m"
DIM = "\033[38;5;244m"
BOLD = "\033[1m"
OFF = "\033[0m"


def rule(char="─"):
    return "╭" + char * WIDTH + "╮"


def line(text="", colour="", pad=2):
    plain = text
    room = WIDTH - pad * 2
    if len(plain) > room:
        plain = plain[:room - 1] + "…"
    body = " " * pad + colour + plain + (OFF if colour else "")
    body += " " * (WIDTH - pad - len(plain) - pad)
    return "│" + body + " " * pad + "│"


def draw(pending):
    """Render the pending interaction the way the panel would."""
    print("\033[2J\033[H", end="")
    print(rule())
    if pending is None:
        print(line())
        print(line("VIBEPULSE", DIM + BOLD))
        print(line())
        print(line("no one needs you right now", DIM))
        print(line())
        for _ in range(6):
            print(line())
        print("╰" + "─" * WIDTH + "╯")
        print(f"\n  {DIM}waiting… [q] quit{OFF}")
        return

    kind = pending.get("kind")
    project = pending.get("project") or "?"
    seconds = max(0, pending.get("expires_in_ms", 0) // 1000)
    can_approve = bool(pending.get("can_approve"))
    # v2 semantics, mirroring the device screens exactly:
    # question -> APPROVE / LEAVE IT; approval -> APPROVE / DENY / LEAVE IT;
    # private (no title) -> tap-anywhere = LEAVE IT, nothing else offered.
    private = pending.get("title") is None
    offer_deny = kind == "approval" and not private

    # the countdown ring, one text line worth of it
    hold = pending.get("hold_ms") or pending.get("expires_in_ms", 0)
    frac = (pending.get("expires_in_ms", 0) / hold) if hold else 1.0
    filled = max(0, min(20, round(frac * 20)))
    ring_bar = "█" * filled + "░" * (20 - filled)

    print(line())
    print(line(f"CLAUDE NEEDS YOU · {project}", ORANGE + BOLD))
    print(line(f"{ring_bar}  {seconds}s", DIM))
    print(line())

    prompt = pending.get("prompt")
    title = pending.get("title")
    subtitle = pending.get("subtitle")

    if private:
        print(line("SOMETHING IS WAITING", BOLD))
        print(line("Details stay on the Mac", DIM))
        print(line())
        print(line("TAP TO ANSWER AT YOUR DESK", DIM))
        print(line("(any key sends it to the terminal)", DIM))
        print("╰" + "─" * WIDTH + "╯")
        print(f"\n  {DIM}[any] to terminal [p]anic [q]uit{OFF}")
        return

    if prompt:
        print(line(prompt, DIM))
    if kind == "question" and pending.get("marked"):
        print(line("CLAUDE RECOMMENDS", ORANGE))
    print(line(title, BOLD))
    if subtitle:
        print(line(subtitle, DIM))
    print(line())

    if can_approve:
        actions = "[a] APPROVE    [l] LEAVE IT"
        if offer_deny:
            actions = "[a] APPROVE    [d] DENY    [l] LEAVE IT"
        print(line(actions, GREEN))
    else:
        print(line("[d] DENY    [l] LEAVE IT" if offer_deny
                   else "[l] LEAVE IT", DIM))
        print(line("approve at the terminal for this one", DIM))
    total = pending.get("options_total")
    if kind == "question" and total and total > 1:
        extra = total - 1
        plural = "S" if extra > 1 else ""
        print(line(f"{extra} MORE OPTION{plural} IN TERMINAL", DIM))
    print("╰" + "─" * WIDTH + "╯")
    print(f"\n  {DIM}[a]pprove{' [d]eny' if offer_deny else ''} "
          f"[l]eave [p]anic [q]uit{OFF}")


def get_json(url, timeout=5):
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return json.loads(response.read())


def post_json(url, payload, timeout=5):
    request = urllib.request.Request(
        url, data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, json.loads(response.read())
    except urllib.error.HTTPError as error:
        try:
            return error.code, json.loads(error.read())
        except Exception:
            return error.code, {}


def read_key_press():
    """One keypress, no Enter — the closest a terminal gets to a touch."""
    try:
        import termios
        import tty
    except ImportError:  # pragma: no cover - non-POSIX
        return sys.stdin.readline().strip()[:1]
    fileno = sys.stdin.fileno()
    saved = termios.tcgetattr(fileno)
    try:
        tty.setraw(fileno)
        return sys.stdin.read(1)
    finally:
        termios.tcsetattr(fileno, termios.TCSADRAIN, saved)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8737)
    args = parser.parse_args()
    base = f"http://{args.host}:{args.port}"

    secret = read_device_key()
    if not secret:
        print("No device key found. Set TK_VIBEPULSE_DEVICE_KEY in "
              "secrets.h, or VIBEPULSE_DEVICE_KEY in the environment — the "
              "same value the server reads.", file=sys.stderr)
        return 1

    import select

    current = None
    last_poll = 0.0
    while True:
        now = time.monotonic()
        if now - last_poll >= 1.0:  # the device's own cadence
            last_poll = now
            try:
                snapshot = get_json(f"{base}/api/agent-status")
                fresh = snapshot.get("pending")
            except (urllib.error.URLError, TimeoutError, OSError):
                fresh = current  # keep the last good frame, like the panel
            if fresh != current:
                current = fresh
                draw(current)
            elif current is not None:
                draw(current)  # refresh the countdown

        if not select.select([sys.stdin], [], [], 0.2)[0]:
            continue
        key = (read_key_press() or "").lower()
        if key == "q":
            print()
            return 0
        stamp = int(time.time())
        if key == "p":
            status, body = post_json(f"{base}/api/panic", {
                "ts": stamp,
                "hmac": sign_answer(secret, "panic", "deny", stamp)})
            print(f"\n  panic stop: {status} {body}")
            time.sleep(1.0)
            current = None
            continue
        if current is None:
            continue
        # mirror the device screens exactly: private is tap-anywhere =
        # LEAVE IT, and a question never offers DENY (the terminal, which
        # shows every option, takes those refusals)
        if current.get("title") is None:
            key = "l"
        elif key == "d" and current.get("kind") != "approval":
            continue
        if key not in ("a", "d", "l"):
            continue
        verdict = {"a": "approve", "d": "deny", "l": "leave_it"}[key]
        request_id = current["request_id"]
        status, body = post_json(
            f"{base}/api/interaction/{request_id}",
            {"verdict": verdict, "ts": stamp,
             "hmac": sign_answer(secret, request_id, verdict, stamp)})
        if status != 200:
            print(f"\n  refused: {body.get('reason', status)}")
            time.sleep(1.2)
        current = None
        last_poll = 0.0


if __name__ == "__main__":
    raise SystemExit(main())
