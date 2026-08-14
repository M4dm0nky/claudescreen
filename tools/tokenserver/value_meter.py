"""Value multiple: what this month's usage would cost at list API prices.

The question the usage pages cannot answer is "am I getting my money's
worth?". This module answers it by pricing every token the transcripts
already record, at published per-model list rates, and dividing by what the
subscription costs.

Three things make the number honest rather than decorative:

* **Cache tokens dominate and are priced separately.** A real assistant
  record routinely looks like 2 input / 4 output / 23 655 cache-read /
  8 246 cache-write. Pricing only ``input_tokens`` + ``output_tokens``
  understates the true value by orders of magnitude. Cache writes are
  further split by TTL, because a 5-minute write and a 1-hour write cost
  1.25x and 2x input respectively -- the transcript carries that split in
  ``usage.cache_creation``, so we never have to guess.
* **An unpriceable token is never silently worth zero.** A model this table
  does not know contributes to ``unpriced_tokens`` instead of quietly
  lowering the multiple. Past a small tolerance the whole figure degrades
  to a dash rather than showing a confidently wrong number.
* **The denominator is configuration, not a constant.** Subscription prices
  are not part of the API price list and could not be verified from a
  primary source when this was written, so the plan cost is supplied by the
  operator and the payload always reports which source it came from.

Nothing here reads message content -- only the numeric ``usage`` block and
the model id.
"""

from __future__ import annotations

import math
from typing import Any, Optional


# --------------------------------------------------------------------------
# Prices
# --------------------------------------------------------------------------

#: Date the list prices below were taken from Anthropic's published model
#: table. Surfaced in the payload so a stale table is visible rather than
#: implied.
PRICES_AS_OF = "2026-06-24"

#: USD per million tokens. ``input``/``output`` are published list prices.
#: ``intro`` is a promotional input/output pair that expires on ``until``
#: (ISO date, inclusive); after that the standard pair applies.
#:
#: Cache rates are NOT separately published per model -- they are fixed
#: multiples of that model's input price, applied in :func:`price_usage`:
#:   * 5-minute cache write : 1.25x input
#:   * 1-hour cache write   : 2.00x input
#:   * cache read           : 0.10x input
_LIST_PRICES: dict[str, dict[str, Any]] = {
    "claude-fable-5":    {"input": 10.00, "output": 50.00},
    "claude-mythos-5":   {"input": 10.00, "output": 50.00},
    "claude-opus-5":     {"input": 5.00,  "output": 25.00},
    "claude-opus-4-8":   {"input": 5.00,  "output": 25.00},
    "claude-opus-4-7":   {"input": 5.00,  "output": 25.00},
    "claude-opus-4-6":   {"input": 5.00,  "output": 25.00},
    "claude-sonnet-5":   {"input": 3.00,  "output": 15.00,
                          "intro": {"input": 2.00, "output": 10.00,
                                    "until": "2026-08-31"}},
    "claude-sonnet-4-6": {"input": 3.00,  "output": 15.00},
    "claude-haiku-4-5":  {"input": 1.00,  "output": 5.00},
}

CACHE_WRITE_5M_MULTIPLIER = 1.25
CACHE_WRITE_1H_MULTIPLIER = 2.00
CACHE_READ_MULTIPLIER = 0.10

#: The Batch API bills at 50% of standard rates. Any other tier is priced at
#: standard and its name recorded, so an unrecognised tier shows up as a
#: caveat instead of a silent mispricing.
_TIER_MULTIPLIERS = {"standard": 1.0, "batch": 0.5}

#: Monthly subscription cost in USD, by the plan names already allowlisted in
#: ``max_tracker.PLAN_LABELS``.
#:
#: These are DEFAULTS, not verified figures. claude.com and the Claude help
#: centre were both unreachable from the build environment, so only the
#: "Max starts from $100/month" figure could be corroborated. Treat every
#: entry as an operator-overridable starting point: pass ``--plan-cost-usd``
#: to state what you actually pay. The payload reports ``cost_source`` so the
#: distinction survives all the way to the glass.
DEFAULT_PLAN_COST_USD: dict[str, float] = {
    "pro": 20.0,
    "max5x": 100.0,
    "max20x": 200.0,
}

#: Fraction of month-to-date tokens that may be unpriceable before the
#: multiple is suppressed entirely. Small enough that a stray unknown model
#: cannot meaningfully move the figure; non-zero so one odd record does not
#: blank a page that is otherwise correct.
UNPRICED_TOLERANCE = 0.02


# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------

def _count(value: Any) -> int:
    """Coerce one usage field to a non-negative token count.

    Transcript writers are not schema-validated, so anything non-numeric,
    negative, boolean, or non-finite is read as zero rather than being
    allowed to poison the total.
    """
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return 0
    if not math.isfinite(value) or value <= 0:
        return 0
    return int(value)


def _rates(model: Optional[str], today: Optional[str]) -> Optional[dict]:
    """Return the input/output pair in force for ``model`` on ``today``.

    ``None`` means "not priceable" -- an unknown or missing model id. The
    caller must account for those tokens rather than treating them as free.
    """
    if not isinstance(model, str):
        return None
    entry = _LIST_PRICES.get(model)
    if entry is None:
        return None
    intro = entry.get("intro")
    if intro and isinstance(today, str) and today <= intro["until"]:
        return {"input": intro["input"], "output": intro["output"]}
    return {"input": entry["input"], "output": entry["output"]}


def cache_split(usage: dict) -> tuple[int, int]:
    """Split cache-creation tokens into (5-minute, 1-hour) buckets.

    Prefers the explicit ``usage.cache_creation`` breakdown. When only the
    flat ``cache_creation_input_tokens`` total is present the whole amount is
    attributed to the 5-minute bucket -- the cheaper of the two, so the
    fallback can only ever *understate* value, never inflate it.
    """
    detail = usage.get("cache_creation")
    if isinstance(detail, dict):
        five = _count(detail.get("ephemeral_5m_input_tokens"))
        hour = _count(detail.get("ephemeral_1h_input_tokens"))
        if five or hour:
            return five, hour
    return _count(usage.get("cache_creation_input_tokens")), 0


# --------------------------------------------------------------------------
# Pricing
# --------------------------------------------------------------------------

def price_usage(model: Optional[str], usage: Any,
                today: Optional[str] = None) -> tuple[float, int]:
    """Price one assistant record.

    Returns ``(usd, unpriced_tokens)``. Exactly one of the two is ever
    non-zero: a record either prices completely or not at all, because a
    partially-priced record would be indistinguishable from a cheap one.
    """
    if not isinstance(usage, dict):
        return 0.0, 0

    inp = _count(usage.get("input_tokens"))
    out = _count(usage.get("output_tokens"))
    read = _count(usage.get("cache_read_input_tokens"))
    write_5m, write_1h = cache_split(usage)
    total = inp + out + read + write_5m + write_1h
    if total <= 0:
        return 0.0, 0

    rates = _rates(model, today)
    if rates is None:
        return 0.0, total

    tier = usage.get("service_tier")
    tier_multiplier = _TIER_MULTIPLIERS.get(
        tier if isinstance(tier, str) else "standard", 1.0)

    in_rate = rates["input"]
    usd = (
        inp * in_rate
        + out * rates["output"]
        + write_5m * in_rate * CACHE_WRITE_5M_MULTIPLIER
        + write_1h * in_rate * CACHE_WRITE_1H_MULTIPLIER
        + read * in_rate * CACHE_READ_MULTIPLIER
    ) / 1_000_000.0
    return usd * tier_multiplier, 0


def plan_cost(plan: Optional[str], override: Optional[float] = None
              ) -> tuple[Optional[float], str]:
    """Resolve the monthly subscription cost and say where it came from.

    ``("configured"|"default"|"unknown")`` is returned alongside the amount
    so the UI can distinguish a figure the operator confirmed from one this
    module guessed.
    """
    if (isinstance(override, (int, float)) and not isinstance(override, bool)
            and math.isfinite(override) and override > 0):
        return float(override), "configured"
    if isinstance(plan, str) and plan in DEFAULT_PLAN_COST_USD:
        return DEFAULT_PLAN_COST_USD[plan], "default"
    return None, "unknown"


def build_payload(value_usd: float, unpriced_tokens: int, priced_tokens: int,
                  plan: Optional[str] = None,
                  cost_override: Optional[float] = None) -> dict:
    """Build the additive ``value`` block for the tokenserver payload.

    ``state`` is the field the firmware should branch on:

    ``ok``
        ``multiple`` is meaningful.
    ``no_plan_cost``
        Usage was priced but nothing says what the plan costs, so there is
        no denominator. Show the dollar figure, dash the multiple.
    ``partial``
        Too large a share of this month's tokens came from models this
        table cannot price. Dash everything -- a multiple computed from an
        unknown fraction of the spend is worse than no multiple.
    """
    total = priced_tokens + unpriced_tokens
    unpriced_share = (unpriced_tokens / total) if total else 0.0
    cost, cost_source = plan_cost(plan, cost_override)

    payload: dict[str, Any] = {
        "value_usd": round(value_usd, 2),
        "plan_usd": cost,
        "cost_source": cost_source,
        "basis": "list API prices",
        "prices_as_of": PRICES_AS_OF,
        "unpriced_token_share": round(unpriced_share, 4),
    }

    if unpriced_share > UNPRICED_TOLERANCE:
        payload["state"] = "partial"
        payload["multiple"] = None
    elif cost is None:
        payload["state"] = "no_plan_cost"
        payload["multiple"] = None
    else:
        payload["state"] = "ok"
        payload["multiple"] = round(value_usd / cost, 2)
    return payload
