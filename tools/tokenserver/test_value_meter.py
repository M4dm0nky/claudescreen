"""Tests for the value multiple.

Every expected dollar figure here is computed by hand in the docstring or a
comment and written as a literal. That is deliberate: deriving the expectation
from the same rate table the implementation reads would make the test pass
even if the table were wrong, which is exactly the failure this feature
cannot afford.
"""

import unittest

from tools.tokenserver import value_meter


# A real assistant record, copied from a live transcript. Its shape is the
# whole argument for the feature: 2 input and 4 output tokens against 8 246
# cache-write and 23 655 cache-read.
REAL_USAGE = {
    "cache_creation": {
        "ephemeral_1h_input_tokens": 0,
        "ephemeral_5m_input_tokens": 8246,
    },
    "cache_creation_input_tokens": 8246,
    "cache_read_input_tokens": 23655,
    "input_tokens": 2,
    "output_tokens": 4,
    "service_tier": "standard",
}

# Claude Opus 5 lists at $5.00 input / $25.00 output per million tokens.
#   input        2 x  5.00              =        10.00
#   output       4 x 25.00              =       100.00
#   5m write  8246 x  5.00 x 1.25       =    51 537.50
#   cache read 23655 x 5.00 x 0.10      =    11 827.50
#                                         -------------
#                                            63 475.00  per million
#                                       => $0.06347500
REAL_USAGE_USD = 63475.0 / 1_000_000.0


class PriceUsageTest(unittest.TestCase):

    def test_prices_a_real_record_including_cache(self):
        usd, unpriced = value_meter.price_usage(
            "claude-opus-5", REAL_USAGE, "2026-08-14")
        self.assertEqual(unpriced, 0)
        self.assertAlmostEqual(usd, REAL_USAGE_USD, places=10)

    def test_cache_dominates_input_and_output(self):
        """The reason this module exists, asserted as a number.

        Pricing only input+output would bill $0.00011 for a record actually
        worth $0.063 -- a 577x understatement. If someone ever 'simplifies'
        the cache terms away, this fails loudly.
        """
        usd, _ = value_meter.price_usage(
            "claude-opus-5", REAL_USAGE, "2026-08-14")
        naive = (2 * 5.00 + 4 * 25.00) / 1_000_000.0
        self.assertGreater(usd / naive, 500)

    def test_one_hour_cache_writes_cost_more_than_five_minute(self):
        five = value_meter.price_usage("claude-opus-5", {
            "cache_creation": {"ephemeral_5m_input_tokens": 1_000_000,
                               "ephemeral_1h_input_tokens": 0}},
            "2026-08-14")[0]
        hour = value_meter.price_usage("claude-opus-5", {
            "cache_creation": {"ephemeral_5m_input_tokens": 0,
                               "ephemeral_1h_input_tokens": 1_000_000}},
            "2026-08-14")[0]
        self.assertAlmostEqual(five, 6.25)   # 5.00 x 1.25
        self.assertAlmostEqual(hour, 10.00)  # 5.00 x 2.00

    def test_flat_cache_total_is_attributed_to_the_cheaper_bucket(self):
        """Without the TTL breakdown we must understate, never inflate."""
        usd, _ = value_meter.price_usage(
            "claude-opus-5", {"cache_creation_input_tokens": 1_000_000},
            "2026-08-14")
        self.assertAlmostEqual(usd, 6.25)

    def test_explicit_breakdown_wins_over_flat_total(self):
        usd, _ = value_meter.price_usage("claude-opus-5", {
            "cache_creation_input_tokens": 1_000_000,
            "cache_creation": {"ephemeral_5m_input_tokens": 0,
                               "ephemeral_1h_input_tokens": 1_000_000}},
            "2026-08-14")
        self.assertAlmostEqual(usd, 10.00)

    def test_intro_price_applies_before_it_expires(self):
        # Sonnet 5 introductory input rate is $2.00/M through 2026-08-31.
        usd, _ = value_meter.price_usage(
            "claude-sonnet-5", {"input_tokens": 1_000_000}, "2026-08-14")
        self.assertAlmostEqual(usd, 2.00)

    def test_standard_price_applies_after_intro_expires(self):
        usd, _ = value_meter.price_usage(
            "claude-sonnet-5", {"input_tokens": 1_000_000}, "2026-09-01")
        self.assertAlmostEqual(usd, 3.00)

    def test_intro_boundary_day_is_inclusive(self):
        usd, _ = value_meter.price_usage(
            "claude-sonnet-5", {"input_tokens": 1_000_000}, "2026-08-31")
        self.assertAlmostEqual(usd, 2.00)

    def test_batch_tier_is_half_price(self):
        usd, _ = value_meter.price_usage(
            "claude-opus-5",
            {"input_tokens": 1_000_000, "service_tier": "batch"},
            "2026-08-14")
        self.assertAlmostEqual(usd, 2.50)

    def test_unknown_model_reports_tokens_as_unpriced_not_free(self):
        usd, unpriced = value_meter.price_usage(
            "some-model-shipped-next-year", REAL_USAGE, "2026-08-14")
        self.assertEqual(usd, 0.0)
        self.assertEqual(unpriced, 2 + 4 + 8246 + 23655)

    def test_missing_model_is_unpriced(self):
        usd, unpriced = value_meter.price_usage(None, REAL_USAGE, "2026-08-14")
        self.assertEqual(usd, 0.0)
        self.assertEqual(unpriced, 31907)

    def test_empty_usage_is_neither_priced_nor_unpriced(self):
        for usage in ({}, {"input_tokens": 0}, None, "nonsense", []):
            usd, unpriced = value_meter.price_usage(
                "claude-opus-5", usage, "2026-08-14")
            self.assertEqual((usd, unpriced), (0.0, 0), usage)

    def test_malformed_counts_are_ignored_not_trusted(self):
        usd, unpriced = value_meter.price_usage("claude-opus-5", {
            "input_tokens": -5,
            "output_tokens": True,
            "cache_read_input_tokens": "12000",
            "cache_creation_input_tokens": float("inf"),
        }, "2026-08-14")
        self.assertEqual((usd, unpriced), (0.0, 0))


class PlanCostTest(unittest.TestCase):

    def test_override_wins_and_is_marked_configured(self):
        self.assertEqual(
            value_meter.plan_cost("max5x", 137.5), (137.5, "configured"))

    def test_default_is_marked_as_such(self):
        cost, source = value_meter.plan_cost("pro")
        self.assertEqual(source, "default")
        self.assertEqual(cost, value_meter.DEFAULT_PLAN_COST_USD["pro"])

    def test_unknown_plan_has_no_cost(self):
        self.assertEqual(value_meter.plan_cost(None), (None, "unknown"))
        self.assertEqual(value_meter.plan_cost("team"), (None, "unknown"))

    def test_nonsense_overrides_fall_through_to_the_plan_default(self):
        for bad in (0, -10, float("nan"), float("inf"), True, "100", None):
            cost, source = value_meter.plan_cost("pro", bad)
            self.assertEqual(source, "default", bad)
            self.assertEqual(cost, 20.0, bad)


class BuildPayloadTest(unittest.TestCase):

    def test_ok_state_reports_the_multiple(self):
        payload = value_meter.build_payload(
            312.0, unpriced_tokens=0, priced_tokens=5_000_000,
            plan="max5x", cost_override=100.0)
        self.assertEqual(payload["state"], "ok")
        self.assertEqual(payload["multiple"], 3.12)
        self.assertEqual(payload["value_usd"], 312.0)
        self.assertEqual(payload["plan_usd"], 100.0)
        self.assertEqual(payload["cost_source"], "configured")

    def test_missing_plan_cost_dashes_the_multiple_but_keeps_the_dollars(self):
        payload = value_meter.build_payload(
            312.0, unpriced_tokens=0, priced_tokens=5_000_000, plan=None)
        self.assertEqual(payload["state"], "no_plan_cost")
        self.assertIsNone(payload["multiple"])
        self.assertEqual(payload["value_usd"], 312.0)

    def test_too_many_unpriced_tokens_suppresses_the_multiple(self):
        payload = value_meter.build_payload(
            312.0, unpriced_tokens=500_000, priced_tokens=500_000,
            plan="max5x", cost_override=100.0)
        self.assertEqual(payload["state"], "partial")
        self.assertIsNone(payload["multiple"])
        self.assertEqual(payload["unpriced_token_share"], 0.5)

    def test_a_trace_of_unpriced_tokens_is_tolerated(self):
        payload = value_meter.build_payload(
            312.0, unpriced_tokens=1_000, priced_tokens=1_000_000,
            plan="max5x", cost_override=100.0)
        self.assertEqual(payload["state"], "ok")
        self.assertEqual(payload["multiple"], 3.12)

    def test_partial_outranks_missing_plan_cost(self):
        """Both wrong: report the one that makes the number meaningless."""
        payload = value_meter.build_payload(
            312.0, unpriced_tokens=500_000, priced_tokens=500_000, plan=None)
        self.assertEqual(payload["state"], "partial")

    def test_zero_usage_is_ok_and_not_a_division_error(self):
        payload = value_meter.build_payload(
            0.0, unpriced_tokens=0, priced_tokens=0,
            plan="pro", cost_override=20.0)
        self.assertEqual(payload["state"], "ok")
        self.assertEqual(payload["multiple"], 0.0)
        self.assertEqual(payload["unpriced_token_share"], 0.0)

    def test_payload_states_its_basis_and_price_date(self):
        payload = value_meter.build_payload(
            1.0, 0, 1000, plan="pro", cost_override=20.0)
        self.assertEqual(payload["basis"], "list API prices")
        self.assertEqual(payload["prices_as_of"], value_meter.PRICES_AS_OF)


if __name__ == "__main__":
    unittest.main()
