import std;
import star.assets;
import star.format;
import star.game_test;
import star.game_timers;
import star.interpolation_tracker;
import star.json;
import star.map;
import star.root;
import star.string;
#include "gtest/gtest.h"

using namespace Star;

namespace {

bool check(bool passed, std::string_view message, std::source_location at = std::source_location::current()) {
  if (!passed)
    testing::internal::AssertHelper(testing::TestPartResult::kNonFatalFailure, at.file_name(), static_cast<int>(at.line()), "")
        = testing::Message() << message;
  return passed;
}

const GameTestRegistration config_sources("InterpolationTracker", "ConfigSourcesAndDisabledCadence", [](GameTest& fixture) {
  for (Json const& config : {Json{}, Json(JsonObject{})}) {
    auto defaults = InterpolationTracker::create(fixture.root(), config);
    if (!check(defaults.has_value(), "default configuration")) return;
    check(!defaults->interpolation_enabled() && defaults->entity_update_delta() == 0.05f, "default cadence");
  }
  auto disabled = InterpolationTracker::create(fixture.root(), JsonObject{
      {"entityUpdateDelta", 0}, {"extrapolationHint", 9}, {"stepLead", 60}});
  if (!check(disabled.has_value(), "inline configuration")) return;
  GameTimer timer(disabled->entity_update_delta());
  for (double local_time : {5, 9, 0}) {
    disabled->receive_time_update(local_time + 4);
    disabled->update(local_time);
    check(!disabled->interpolation_enabled() && disabled->interpolation_lead_time() == 0 && disabled->extrapolation_hint() == 0,
        "disabled clock has no lead or hint");
    check(disabled->entity_update_delta() == 0 && timer.wrap_tick(0.016f), "explicit zero interval expires on every call");
  }

  String const path("/client.config:interpolationSettings.local");
  auto assets = fixture.root().assets();
  if (!check(assets.has_value(), "root assets")) return;
  auto config = (*assets)->json(path);
  if (!check(config.has_value(), "real local interpolation config")) return;
  auto resolved = InterpolationTracker::create(fixture.root(), Json(path));
  auto direct = InterpolationTracker::from_config(*config);
  if (!check(resolved && direct, "asset path and inline object")) return;
  check(resolved->interpolation_enabled() == direct->interpolation_enabled() &&
      resolved->entity_update_delta() == direct->entity_update_delta() && resolved->extrapolation_hint() == direct->extrapolation_hint(),
      "resolved asset settings");
  for (double remote : {32, 34, 40}) {
    direct->receive_time_update(remote);
    resolved->receive_time_update(remote);
    direct->update(0);
    resolved->update(0);
    check(resolved->interpolation_lead_time() == direct->interpolation_lead_time(), "resolved asset clock");
  }
  check(!InterpolationTracker::from_config(Json(path)), "rootless parser does not resolve paths");
  check(!InterpolationTracker::create(fixture.root(), Json("/client.config:interpolationSettings.local.entityUpdateDelta")),
      "invalid resolved config type is not silently defaulted");
});

const GameTestRegistration validation("InterpolationTracker", "ConfigBoundsAndFirstError", [](GameTest&) {
  auto reject = [](std::string_view key, Json const& value) {
    auto result = InterpolationTracker::from_config(JsonObject{{String(key), value}});
    check(!result && result.error().is<JsonException>() && result.error().what().contains(key), key);
  };
  for (auto bits : {0x7ff8000000000001ull, 0x7ff0000000000000ull, 0xfff0000000000000ull}) {
    volatile std::uint64_t input = bits;
    Json invalid(std::bit_cast<double>(static_cast<std::uint64_t>(input)));
    auto stored = invalid.to_double();
    if (!check(stored && std::bit_cast<std::uint64_t>(*stored) == bits, "nonfinite fixture bits")) return;
    for (std::string_view key : {"entityUpdateDelta", "stepLead", "extrapolationHint", "stepTrackFactor", "stepMaxDistance"})
      reject(key, invalid);
  }
  auto const max_interval = static_cast<double>(std::numeric_limits<float>::max()) * 60;
  auto const max_hint = std::numeric_limits<unsigned>::max();
  reject("entityUpdateDelta", -0.25);
  reject("entityUpdateDelta", 0x1p-200);
  reject("entityUpdateDelta", std::nextafter(max_interval, std::numeric_limits<double>::max()));
  reject("extrapolationHint", -1);
  reject("extrapolationHint", std::ldexp(1.0, std::numeric_limits<unsigned>::digits));
  reject("stepMaxDistance", -1);
  auto limits = InterpolationTracker::from_config(JsonObject{{"interpolationEnabled", true},
      {"entityUpdateDelta", max_interval}, {"extrapolationHint", max_hint}});
  check(limits && limits->entity_update_delta() == std::numeric_limits<float>::max() && limits->extrapolation_hint() == max_hint,
      "inclusive float and unsigned maxima");
  auto fraction = InterpolationTracker::from_config(JsonObject{{"interpolationEnabled", true}, {"extrapolationHint", 7.75}});
  check(fraction && fraction->extrapolation_hint() == 7, "existing fractional hint truncation");

  // Compare the exact first diagnostic with that key in isolation, regardless
  // of the hash table's iteration order or which later field would also fail.
  std::array<std::pair<String, Json>, 6> const bad_fields{{{"interpolationEnabled", 1}, {"entityUpdateDelta", -3},
      {"stepLead", JsonObject{}}, {"extrapolationHint", -2}, {"stepTrackFactor", false}, {"stepMaxDistance", -5}}};
  JsonObject combined;
  for (auto const& [key, value] : bad_fields | std::views::reverse) {
    combined[key] = value;
    auto single = InterpolationTracker::from_config(JsonObject{{key, value}});
    auto many = InterpolationTracker::from_config(combined);
    check(!single && !many && single.error().what() == many.error().what(), key.utf8());
  }
});

const GameTestRegistration heartbeat_sequence("InterpolationTracker", "HeartbeatTickRollbackAndReplacement", [](GameTest&) {
  Json const config = JsonObject{{"interpolationEnabled", true}, {"stepLead", 30}, {"stepTrackFactor", 0.25}, {"stepMaxDistance", 120}};
  auto tracker = InterpolationTracker::from_config(config);
  if (!check(tracker.has_value(), "enabled tracker")) return;
  auto lead = [&](float expected, std::source_location at = std::source_location::current()) {
    check(tracker->interpolation_lead_time() == expected, "exact lead after clock event", at);
  };
  tracker->update(5); lead(0);
  tracker->receive_time_update(12); lead(0);
  tracker->receive_time_update(16); lead(0);
  tracker->update(7); lead(0.5f);
  tracker->receive_time_update(20); lead(4.5f);
  tracker->receive_time_update(24); lead(8.5f);
  tracker->update(8); lead(2.5f);
  tracker->update(8); lead(2);
  tracker->update(9); lead(1.625f);
  tracker->receive_time_update(18); lead(-6.375f);
  tracker->update(9); lead(-1.5f);
  tracker->update(3); lead(0.5f);
  tracker->receive_time_update(19); lead(1.5f);
  tracker->update(4); lead(1.25f);
  tracker = InterpolationTracker::from_config(config);
  if (!check(tracker.has_value(), "world replacement")) return;
  lead(0);
  tracker->receive_time_update(800); lead(0);
  tracker->update(0); lead(0.5f);

  for (auto [factor, expected] : {std::pair{-1, 1.75f}, std::pair{2, -1.25f}}) {
    auto signed_tracker = InterpolationTracker::from_config(JsonObject{{"interpolationEnabled", true}, {"stepLead", -15},
        {"stepTrackFactor", factor}, {"stepMaxDistance", 240}});
    if (!check(signed_tracker.has_value(), "signed lead and unrestricted finite factor")) return;
    signed_tracker->receive_time_update(1);
    signed_tracker->update(0);
    signed_tracker->receive_time_update(2);
    signed_tracker->update(0);
    check(signed_tracker->interpolation_lead_time() == expected, "track factor is not clamped");
  }
});

const GameTestRegistration numeric_edges("InterpolationTracker", "AdvanceOrderAndFiniteLead", [](GameTest&) {
  auto tracker = InterpolationTracker::from_config(JsonObject{{"interpolationEnabled", true},
      {"stepMaxDistance", 480}, {"stepTrackFactor", 0.5}});
  if (!check(tracker.has_value(), "ordered clock")) return;
  // Clock additions round in opposite directions. Cancelling dt from the
  // correction term before adding it to the clocks would give the wrong lead.
  tracker->receive_time_update(0x1p53);
  tracker->update(0);
  tracker->receive_time_update(0x1p53 + 2);
  tracker->update(1);
  check(tracker->interpolation_lead_time() == 2, "advance both clocks before computing their difference");

  auto extreme = InterpolationTracker::from_config(JsonObject{{"interpolationEnabled", true}, {"stepTrackFactor", 0}});
  if (!check(extreme.has_value(), "zero correction")) return;
  constexpr auto large = std::numeric_limits<double>::max();
  constexpr auto limit = std::numeric_limits<float>::max();
  extreme->receive_time_update(-large);
  extreme->update(0);
  for (double sample : {large, -large}) {
    extreme->receive_time_update(sample);
    check(extreme->interpolation_lead_time() == (sample > 0 ? limit : -limit), "opposite finite heartbeat saturates lead");
    extreme->update(0);
    check(extreme->interpolation_lead_time() == 0, "overflow did not poison subsequent prediction");
  }
});

}
