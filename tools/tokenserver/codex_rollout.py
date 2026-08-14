"""Codex rollout event acceptance — shared by the live probe and the Max
Tracker backfill so both apply the exact same rule.

Kept as its own leaf module (no imports from :mod:`tokenserver` or
:mod:`max_tracker`) so both of those can import it without ever forming an
import cycle between them.
"""

from __future__ import annotations

from datetime import datetime
from typing import Any, Optional


def codex_rollout_rate_limits(obj: Any) -> Optional[dict]:
    """Accept only Codex's observed token-count rollout event envelope.

    Only a direct ``{"type": "event_msg", "payload": {"type": "token_count",
    "rate_limits": {...}}}`` shape is accepted. Quoted (stringified) or
    nested ``rate_limits`` (for example under ``payload.message`` or inside
    a JSON-encoded ``payload.content`` string) is rejected — this is the
    single acceptance rule every Codex rollout reader must share.
    """
    if not isinstance(obj, dict) or obj.get("type") != "event_msg":
        return None
    payload = obj.get("payload")
    if not isinstance(payload, dict) or payload.get("type") != "token_count":
        return None
    rate_limits = payload.get("rate_limits")
    return rate_limits if isinstance(rate_limits, dict) else None


def observation_timestamp(value: Any) -> Optional[int]:
    """Parse a rollout event's own ISO ``timestamp`` field to epoch seconds."""
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        return int(parsed.timestamp())
    except (ValueError, OverflowError):
        return None


def codex_rollout_session_id(obj: Any) -> Optional[str]:
    """Accept a rollout ``session_meta`` line and return its conversation id.

    Every rollout opens with a ``session_meta`` whose ``payload.session_id``
    names the conversation. A *resumed*, *forked* or subagent-spawned rollout
    keeps the ORIGINAL conversation's ``session_id`` while taking a fresh file
    ``id`` (and, for a fork, a ``forked_from_id``), and it replays the whole
    parent history -- every ``token_count`` re-timestamped to the new file's
    creation moment. ``session_id`` is therefore the one field that ties every
    replay back to the single conversation whose usage was actually billed: a
    reader that sums ``token_count`` across files must group by it, or it counts
    one conversation once per resume.

    Verified against ``codex-rs`` (``SessionMeta``; resume/fork replay, see
    openai/codex PR #18023 "replay token usage after resume and fork") and
    against real ``~/.codex`` rollouts, where one conversation's ~5 000-event
    history was replayed across 113 files under a single ``session_id``.
    """
    if not isinstance(obj, dict) or obj.get("type") != "session_meta":
        return None
    payload = obj.get("payload")
    if not isinstance(payload, dict):
        return None
    session_id = payload.get("session_id")
    return session_id if isinstance(session_id, str) and session_id else None


def codex_rollout_turn_model(obj: Any) -> Optional[str]:
    """Accept a rollout ``turn_context`` line and return the model it names.

    ``token_count`` events carry no model of their own -- ``TokenCountEvent``
    is only ``{info, rate_limits}``. The model is recorded separately on
    ``TurnContextItem.model``, so a reader that wants to price Codex usage has
    to track the most recent ``turn_context`` and attribute later
    ``token_count`` events to it.

    Verified against ``codex-rs/history/src/rollout_payload.rs`` (wire tag
    ``turn_context``, snake_case) and ``protocol.rs``'s ``TurnContextItem``.
    """
    if not isinstance(obj, dict) or obj.get("type") != "turn_context":
        return None
    payload = obj.get("payload")
    if not isinstance(payload, dict):
        return None
    model = payload.get("model")
    return model if isinstance(model, str) and model else None


def codex_rollout_last_token_usage(obj: Any) -> Optional[dict]:
    """Accept a ``token_count`` event and return the LAST turn's token usage.

    Deliberately ``last_token_usage`` and never ``total_token_usage``:
    ``TokenUsageInfo`` carries both, and the total is the accumulated session
    figure. Summing totals across events would multiply a session's usage by
    its event count. ``last_token_usage`` is the usage of the API call that
    produced the event, which is both correctly attributable to the event's
    own timestamp and exactly what that call bills.

    The same strict envelope rule as :func:`codex_rollout_rate_limits`: only a
    direct ``{"type": "event_msg", "payload": {"type": "token_count", ...}}``
    is accepted, never a quoted or renested variant.
    """
    if not isinstance(obj, dict) or obj.get("type") != "event_msg":
        return None
    payload = obj.get("payload")
    if not isinstance(payload, dict) or payload.get("type") != "token_count":
        return None
    info = payload.get("info")
    if not isinstance(info, dict):
        return None
    usage = info.get("last_token_usage")
    return usage if isinstance(usage, dict) else None
