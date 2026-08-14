"""Value multiple: what this month's agent usage would cost at list API prices.

The usage pages answer "how much have I spent?". They cannot answer the
question every subscriber actually has standing: **am I getting my money's
worth?** This module answers it by pricing every token the local logs already
record, at published per-model list rates, and dividing by what the
subscription costs.

Rates live in ``prices.json`` beside this file, not in code, so adopting this
in another build means editing data or passing ``--prices FILE`` -- never
patching Python. A model that ships next month needs one JSON entry.

Four things make the number honest rather than decorative:

* **Cache tokens dominate, and are priced separately.** A real Claude record
  reads 2 input / 4 output against 8 246 cache-write and 23 655 cache-read.
  Pricing only input+output understates it by ~577x.
* **Providers do not count input the same way.** Anthropic's ``input_tokens``
  excludes cached tokens; Codex's *includes* them. Treating them alike
  double-charges one of the two. Each provider therefore declares an
  ``accounting`` mode in the table.
* **An unpriceable token is never silently worth zero.** A model the table
  does not know contributes to ``unpriced_tokens``; past a small tolerance
  the multiple degrades to a dash rather than showing a confident wrong
  number.
* **Provenance survives to the glass.** Rates carry ``verified`` and
  ``as_of``; the payload repeats them, so an estimate is never displayed as
  though it were a vendor-confirmed figure.

Nothing here reads message content -- only numeric usage fields and model ids.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any, Optional


DEFAULT_PRICES_PATH = Path(__file__).with_name("prices.json")

#: Share of month-to-date tokens that may be unpriceable before the multiple
#: is suppressed. Small enough that a stray unknown model cannot move the
#: figure; non-zero so one odd record does not blank an otherwise good page.
UNPRICED_TOLERANCE = 0.02

_ACCOUNTING_MODES = {"cache_excluded_input", "cache_included_input"}


def _number(value: Any) -> Optional[float]:
    """Return a finite, positive-or-zero float, or None if the value is junk."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    if not math.isfinite(value) or value < 0:
        return None
    return float(value)


def _count(value: Any) -> int:
    """Coerce one usage field to a non-negative token count.

    Log writers are not schema-validated, so anything non-numeric, negative,
    boolean or non-finite reads as zero instead of poisoning the total.
    """
    number = _number(value)
    return 0 if number is None else int(number)


class PriceTable:
    """Per-model list rates, indexed by model id across every provider."""

    def __init__(self, document: dict):
        self._doc = document
        self._by_model: dict[str, tuple[str, dict, dict]] = {}
        for provider, spec in (document.get("providers") or {}).items():
            if not isinstance(spec, dict):
                continue
            if spec.get("accounting") not in _ACCOUNTING_MODES:
                raise ValueError(
                    f"provider {provider!r} declares unknown accounting "
                    f"{spec.get('accounting')!r}; expected one of "
                    f"{sorted(_ACCOUNTING_MODES)}")
            for model, rates in (spec.get("models") or {}).items():
                if isinstance(rates, dict):
                    self._by_model[model] = (provider, spec, rates)

    # -- lookup ------------------------------------------------------------

    def knows(self, model: Optional[str]) -> bool:
        return isinstance(model, str) and model in self._by_model

    def provider_of(self, model: Optional[str]) -> Optional[str]:
        entry = self._by_model.get(model) if isinstance(model, str) else None
        return entry[0] if entry else None

    def provenance(self, providers_used: Optional[Any] = None) -> dict:
        """Verification status for the providers that actually priced tokens.

        Scoped to what contributed, not to the whole table: a Claude-only
        machine gets a verified figure even though the table also carries
        estimated Codex rates that nothing used. Within that scope
        ``verified`` is an AND -- one estimated rate makes the whole figure an
        estimate, which is what the display needs to know.

        ``as_of`` is the OLDEST contributing date, since a total is only as
        current as its stalest input.
        """
        specs = self._doc.get("providers") or {}
        if providers_used is not None:
            specs = {name: spec for name, spec in specs.items()
                     if name in set(providers_used)}
        specs = {n: s for n, s in specs.items() if isinstance(s, dict)}
        if not specs:
            return {"verified": True, "as_of": None}
        verified = all(bool(spec.get("verified")) for spec in specs.values())
        dates = sorted(str(spec.get("as_of")) for spec in specs.values()
                       if spec.get("as_of"))
        return {"verified": verified, "as_of": dates[0] if dates else None}

    def _rates(self, model: str, day: Optional[str]) -> dict:
        """Rates in force for ``model`` on ``day``, applying any intro price."""
        _, _, rates = self._by_model[model]
        intro = rates.get("intro")
        if isinstance(intro, dict) and isinstance(day, str):
            until = intro.get("until")
            if isinstance(until, str) and day <= until:
                merged = dict(rates)
                merged.update({k: v for k, v in intro.items() if k != "until"})
                return merged
        return rates

    # -- pricing -----------------------------------------------------------

    def price(self, model: Optional[str], usage: Any,
              day: Optional[str] = None) -> tuple[float, int]:
        """Price one usage record.

        Returns ``(usd, unpriced_tokens)``. Exactly one is ever non-zero: a
        record prices completely or not at all, because a partially priced
        record is indistinguishable from a cheap one.
        """
        if not isinstance(usage, dict):
            return 0.0, 0
        if not self.knows(model):
            return 0.0, _countable_tokens(usage)

        _, spec, _ = self._by_model[model]
        rates = self._rates(model, day)
        in_rate = _number(rates.get("input")) or 0.0
        out_rate = _number(rates.get("output")) or 0.0

        if spec["accounting"] == "cache_excluded_input":
            usd, counted = _price_cache_excluded(usage, spec, in_rate, out_rate)
        else:
            usd, counted = _price_cache_included(
                usage, spec, rates, in_rate, out_rate)

        if counted <= 0:
            return 0.0, 0
        return usd * _tier_multiplier(usage, spec), 0

    # -- plans -------------------------------------------------------------

    def plan_cost(self, provider: str, plan: Optional[str],
                  override: Optional[float] = None) -> tuple[Optional[float], str]:
        """Resolve monthly subscription cost, and say where the figure is from.

        ``configured`` means the operator stated it and it can be trusted;
        ``default`` means this table guessed. The distinction reaches the UI.
        """
        amount = _number(override)
        if amount is not None and amount > 0:
            return amount, "configured"
        plans = (self._doc.get("plans") or {}).get(provider)
        if isinstance(plans, dict) and isinstance(plan, str):
            amount = _number(plans.get(plan))
            if amount is not None and amount > 0:
                return amount, "default"
        return None, "unknown"


def _countable_tokens(usage: dict) -> int:
    """Tokens in a record, for the unpriced tally, under either convention.

    ``cache_creation_input_tokens`` is preferred over the per-TTL breakdown so
    the two are never added together, and ``reasoning_output_tokens`` is
    excluded because every provider here reports it as a subset of output.
    """
    return (_count(usage.get("input_tokens"))
            + _count(usage.get("output_tokens"))
            + _count(usage.get("cache_read_input_tokens"))
            + _count(usage.get("cache_creation_input_tokens"))
            + _count(usage.get("cache_write_input_tokens")))


def cache_write_split(usage: dict) -> tuple[int, int]:
    """Split Anthropic cache-creation tokens into (5-minute, 1-hour) buckets.

    Prefers the explicit ``usage.cache_creation`` breakdown, because a 5-minute
    write costs 1.25x input and a 1-hour write 2x -- a 60% difference worth
    getting right. When only the flat total exists it all goes to the 5-minute
    bucket, the cheaper of the two, so the fallback can only ever understate.
    """
    detail = usage.get("cache_creation")
    if isinstance(detail, dict):
        five = _count(detail.get("ephemeral_5m_input_tokens"))
        hour = _count(detail.get("ephemeral_1h_input_tokens"))
        if five or hour:
            return five, hour
    return _count(usage.get("cache_creation_input_tokens")), 0


def _price_cache_excluded(usage: dict, spec: dict, in_rate: float,
                          out_rate: float) -> tuple[float, int]:
    """Anthropic convention: ``input_tokens`` is fresh input only."""
    fresh = _count(usage.get("input_tokens"))
    out = _count(usage.get("output_tokens"))
    read = _count(usage.get("cache_read_input_tokens"))
    write_5m, write_1h = cache_write_split(usage)

    usd = (
        fresh * in_rate
        + out * out_rate
        + write_5m * in_rate * (_number(spec.get("cache_write_5m_multiplier")) or 0.0)
        + write_1h * in_rate * (_number(spec.get("cache_write_1h_multiplier")) or 0.0)
        + read * in_rate * (_number(spec.get("cache_read_multiplier")) or 0.0)
    ) / 1_000_000.0
    return usd, fresh + out + read + write_5m + write_1h


def _price_cache_included(usage: dict, spec: dict, rates: dict, in_rate: float,
                          out_rate: float) -> tuple[float, int]:
    """Codex/OpenAI convention: ``input_tokens`` already includes cached ones.

    Verified against the Codex source, which computes its own billing-shaped
    total as ``non_cached_input() + output_tokens`` where
    ``non_cached_input = input_tokens - cached_input_tokens``. That also
    confirms ``reasoning_output_tokens`` is a subset of ``output_tokens`` --
    adding it here would double-count every reasoning token.
    """
    total_in = _count(usage.get("input_tokens"))
    cached = min(_count(usage.get("cached_input_tokens")), total_in)
    fresh = total_in - cached
    out = _count(usage.get("output_tokens"))
    write = _count(usage.get("cache_write_input_tokens"))
    cached_rate = _number(rates.get("cached_input"))
    if cached_rate is None:
        cached_rate = in_rate * 0.1

    usd = (
        fresh * in_rate
        + cached * cached_rate
        + write * in_rate * (_number(spec.get("cache_write_multiplier")) or 0.0)
        + out * out_rate
    ) / 1_000_000.0
    return usd, total_in + out + write


def _tier_multiplier(usage: dict, spec: dict) -> float:
    """Service-tier discount (batch/flex). Unknown tiers price at standard."""
    tier = usage.get("service_tier")
    table = spec.get("tier_multipliers")
    if not isinstance(table, dict) or not isinstance(tier, str):
        return 1.0
    return _number(table.get(tier)) if _number(table.get(tier)) else 1.0


# --------------------------------------------------------------------------
# Loading
# --------------------------------------------------------------------------

def load_prices(override_path: Optional[Any] = None) -> PriceTable:
    """Load the bundled table, then merge an operator override over it.

    Merging is per model and per provider key, so an override file states only
    what differs -- correcting one rate does not mean restating the catalogue.
    A malformed or unreadable override is a hard error rather than a silent
    fallback: quietly pricing against rates the operator thinks they replaced
    is exactly the failure this module exists to avoid.
    """
    document = json.loads(DEFAULT_PRICES_PATH.read_text(encoding="utf-8"))
    if override_path:
        extra = json.loads(Path(override_path).read_text(encoding="utf-8"))
        _merge(document, extra)
    return PriceTable(document)


def _merge(base: dict, extra: dict) -> None:
    """Recursively merge ``extra`` into ``base`` (dicts deep, scalars replace)."""
    for key, value in extra.items():
        if (isinstance(value, dict) and isinstance(base.get(key), dict)):
            _merge(base[key], value)
        else:
            base[key] = value


_default_table: Optional[PriceTable] = None


def default_table() -> PriceTable:
    global _default_table
    if _default_table is None:
        _default_table = load_prices()
    return _default_table


def price_usage(model: Optional[str], usage: Any,
                day: Optional[str] = None,
                table: Optional[PriceTable] = None) -> tuple[float, int]:
    """Price one record against the default table (or a supplied one)."""
    return (table or default_table()).price(model, usage, day)


# --------------------------------------------------------------------------
# Payload
# --------------------------------------------------------------------------

def build_payload(value_usd: float, unpriced_tokens: int, priced_tokens: int,
                  claude_plan: Optional[str] = None,
                  codex_plan: Optional[str] = None,
                  cost_override: Optional[float] = None,
                  table: Optional[PriceTable] = None,
                  providers_used: Optional[Any] = None) -> dict:
    """Build the additive ``value`` block for the tokenserver payload.

    ``state`` is what the firmware branches on:

    ``ok``
        ``multiple`` is meaningful.
    ``no_plan_cost``
        Usage priced, but nothing says what the plan costs, so there is no
        denominator. Show the dollars, dash the multiple.
    ``partial``
        Too large a share of the month came from models the table cannot
        price. Dash everything -- a multiple over an unknown fraction of the
        spend is worse than no multiple at all.

    When both subscriptions are configured their monthly costs are added, so
    the multiple answers "what do I get back for everything I pay for?".
    """
    total = priced_tokens + unpriced_tokens
    unpriced_share = (unpriced_tokens / total) if total else 0.0
    prices = table or default_table()

    claude_cost, claude_src = prices.plan_cost(
        "claude", claude_plan, cost_override)
    codex_cost, codex_src = prices.plan_cost("codex", codex_plan)

    costs = [c for c in (claude_cost, codex_cost) if c is not None]
    plan_usd = sum(costs) if costs else None
    # "configured" only when nothing in the denominator was guessed.
    sources = [s for s in (claude_src, codex_src) if s != "unknown"]
    cost_source = ("configured" if sources and all(s == "configured" for s in sources)
                   else "default" if sources else "unknown")

    provenance = prices.provenance(providers_used)
    payload: dict[str, Any] = {
        "value_usd": round(value_usd, 2),
        "plan_usd": plan_usd,
        "cost_source": cost_source,
        "basis": "list API prices",
        "prices_as_of": provenance["as_of"],
        "prices_verified": provenance["verified"],
        "unpriced_token_share": round(unpriced_share, 4),
    }

    if unpriced_share > UNPRICED_TOLERANCE:
        payload["state"] = "partial"
        payload["multiple"] = None
    elif plan_usd is None:
        payload["state"] = "no_plan_cost"
        payload["multiple"] = None
    else:
        payload["state"] = "ok"
        payload["multiple"] = round(value_usd / plan_usd, 2)
    return payload
