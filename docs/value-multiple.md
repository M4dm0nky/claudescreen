# The value multiple

**"You've pulled $312 of API value out of a $100 plan. 3.1×."**

Every usage page in this ecosystem answers *how much have I spent?*. None of
them answers the question a subscriber actually has standing every month:
**am I getting my money's worth?** This does, by pricing the tokens your
agents already logged at published list API rates and dividing by what you
pay.

It is the same data the Usage page shows, aimed at a different question.

---

## Turning it on

Nothing to install. Run the tokenserver with what you actually pay:

```sh
python3 tools/tokenserver/tokenserver.py \
  --claude-plan max5x \
  --plan-cost-usd 100
```

`--plan-cost-usd` is the important one. Subscription prices are not part of
any API price list, so the server cannot look yours up — see
[Where the numbers come from](#where-the-numbers-come-from). Without it you
get an unverified default and the payload says so.

Running Codex as well? Add `--codex-plan pro`. The two monthly costs are
added, so the multiple answers *what do I get back for everything I pay for?*

The figure lands on `GET /api/tokens` under an additive `value` key:

```json
"value": {
  "value_usd": 92.82,        "plan_usd": 100.0,
  "multiple": 0.93,          "state": "ok",
  "cost_source": "configured",
  "basis": "list API prices", "prices_as_of": "2026-06-24",
  "prices_verified": true,   "unpriced_token_share": 0.0
}
```

Already-flashed screens ignore the key — `tokens_parse.c` skips unknown
top-level fields — so adding this cannot break a device you have not
reflashed.

---

## The four states

`state` is what a display should branch on. It exists because a wrong number
here is worse than no number.

| `state` | Meaning | Show |
|---|---|---|
| `ok` | The multiple is meaningful. | The multiple. |
| `no_plan_cost` | Usage priced, but nothing says what the plan costs — there is no denominator. | The dollars; dash the multiple. |
| `partial` | Too much of the month came from models the price table does not know. | Dash everything. |

Two more fields modify how confidently you present it:

- **`cost_source`** — `configured` means you stated what you pay.
  `default` means the table guessed; say so rather than implying precision.
- **`prices_verified`** — false when any *contributing* provider's rates are
  estimates. It is scoped to what actually priced, so a Claude-only machine
  reports `true` even though the table also carries estimated Codex rates.

---

## Why this is not just input + output

A real Claude assistant record from this repo's own logs:

```json
{"input_tokens": 2, "output_tokens": 4,
 "cache_creation_input_tokens": 8246, "cache_read_input_tokens": 23655}
```

Two input tokens. Twenty-three thousand cache-read tokens. Pricing only
input and output bills **$0.00011** for a record actually worth **$0.063** —
a 577× understatement. Cache is not a rounding error here; it is the bill.

Cache writes are split further, because a 5-minute write costs 1.25× input
and a 1-hour write 2×. The transcript carries that split in
`usage.cache_creation`, so it is never guessed. Where only a flat total
exists it is attributed to the cheaper bucket, so the fallback can only ever
*understate*.

### Providers do not count input the same way

This is the trap worth knowing if you extend this to another provider.

| Convention | `input_tokens` means | Used by |
|---|---|---|
| `cache_excluded_input` | Fresh input only; cache read/write reported separately. | Anthropic |
| `cache_included_input` | **Already includes** `cached_input_tokens`. | OpenAI / Codex |

Reading Codex's `input_tokens` as fresh input overcharges the input side by
3.6× on a typical turn. Each provider therefore declares its `accounting`
mode as data, not as an assumption in code.

Two more Codex specifics, both read out of `codex-rs` rather than assumed:
`reasoning_output_tokens` is a **subset** of `output_tokens` (adding it
double-counts), and `total_tokens` is the context window size, not a billing
figure.

---

## Adding or correcting a rate

Rates live in [`tools/tokenserver/prices.json`](../tools/tokenserver/prices.json),
not in code. Point `--prices` at your own file and it is merged **per model**
over the bundled one — state only what differs:

```jsonc
// my-prices.json — corrects one rate, adds one model
{
  "providers": {
    "anthropic": {
      "models": {
        "claude-opus-5":  { "input": 4.50, "output": 22.00 },
        "claude-opus-6":  { "input": 7.00, "output": 35.00 }
      }
    }
  }
}
```

```sh
python3 tools/tokenserver/tokenserver.py --prices my-prices.json
```

A model that ships after this release needs one JSON entry and no code
change. An unknown model is never silently free — its tokens count toward
`unpriced_token_share`, and past 2% the multiple degrades to a dash.

A malformed `--prices` file is a **hard startup failure**, never a silent
fallback. Quietly pricing against rates you believe you replaced is exactly
the failure this feature exists to avoid.

### Adding a whole provider

```jsonc
{
  "providers": {
    "yourprovider": {
      "accounting": "cache_included_input",
      "verified": true,
      "as_of": "2026-08-14",
      "source": "vendor pricing page, checked by hand",
      "cache_write_multiplier": 1.25,
      "tier_multipliers": { "standard": 1.0, "batch": 0.5 },
      "models": { "their-model-1": { "input": 3.0, "output": 12.0,
                                     "cached_input": 0.3 } }
    }
  }
}
```

`accounting` is required and validated at startup — an unrecognised mode
raises rather than defaulting to a convention that might overcharge.

---

## Where the numbers come from

Every rate carries `source`, `as_of` and `verified`, because a price you
cannot trace is a price you should not display.

| Numbers | Provenance |
|---|---|
| **Anthropic model rates** | Anthropic's published pricing table. Verified. |
| **OpenAI / Codex model rates** | Third-party pricing aggregators, August 2026. **Not verified** — `openai.com` was unreachable from the build environment. Replace them via `--prices` once you can check. |
| **Subscription costs** | **Not verified.** `claude.com` and the Claude help centre were both unreachable; only "Max starts from $100/month" could be corroborated. Set `--plan-cost-usd`. |

Cache multipliers (5-minute 1.25×, 1-hour 2×, read 0.1×) are documented
multiples of each model's input price, not separately published per-model
figures.

---

## What it never does

- **Never reads message content.** Only numeric usage fields and model ids.
  Nothing leaves the Mac.
- **Never counts a duplicate twice.** Claude Code writes the same record into
  more than one transcript; the price rides on the record, so the existing
  `(message id, requestId)` dedup covers dollars exactly as it covers tokens.
- **Never prices a record halfway.** A record prices completely or not at
  all — a partially priced record is indistinguishable from a cheap one.
- **Never invents a denominator.** No plan cost means a dash, not a guess.

---

## Known limits

- **Codex is untested against a real install.** Its shapes were read out of
  the Codex source (`TokenCountEvent`, `TurnContextItem.model`,
  `TokenUsage`), and its tests are synthesised from those shapes. Confirm
  against a real `~/.codex` before trusting that half of the figure.
- **List prices are not your prices.** Enterprise discounts, credits and
  promotional rates are not modelled. The multiple answers "what would this
  have cost at list?", which is the honest version of the question.
- **The multiple is month-to-date**, so it climbs through the month and
  resets on the 1st. Early-month figures are not a monthly rate.
