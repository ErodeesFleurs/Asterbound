#include <benchmark/benchmark.h>

import std;
import star.performance_benchmarks;
import star.movement_controller;
import star.collision_block;
import star.physics_entity;
import star.poly;
import star.rect;
import star.vector;
import star.list;
import star.format;

namespace Star {

// This access seam only constructs operands and calls the production solver. It
// deliberately does not implement SAT, contact ordering, or movement correction.
struct MovementCollisionBenchmarkAccess {
  using Candidate = MovementController::CollisionPoly;
  using Result = MovementController::CollisionResult;

  struct Fixture {
    std::string name;
    List<Candidate> candidates;
    PolyF body;
    Vec2F movement;
    bool ignorePlatforms = false;
    bool slopeCorrection = true;
    float maximumCorrection = 8.0f;
    float maximumPlatformCorrection = 2.0f;
    float dt = 1.0f / 60.0f;
  };

  static Candidate candidate(PolyF poly, CollisionKind kind = CollisionKind::Block,
                             std::optional<MovingCollisionId> id = {}) {
    auto bounds = poly.boundBox();
    auto center = poly.center();
    return {std::move(poly), bounds, center, id, kind, 0.0f};
  }

  static Result move(Fixture const& fixture, List<Candidate>& candidates) {
    return MovementController::collisionMove(candidates, fixture.body, fixture.movement, fixture.ignorePlatforms,
                                             fixture.slopeCorrection, fixture.maximumCorrection,
                                             fixture.maximumPlatformCorrection, fixture.body.center(), fixture.dt);
  }

  static std::string signature(Result const& result) {
    // Record exact consumer-visible float payloads rather than rounded decimal
    // output. These labels are the cross-process, before/after regression oracle.
    auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
    return std::format("m={:08x},{:08x};c={:08x},{:08x};s={:08x},{:08x};stuck={};ground={};kind={};platform={}:{}",
                       bits(result.movement[0]), bits(result.movement[1]), bits(result.correction[0]),
                       bits(result.correction[1]), bits(result.groundSlope[0]), bits(result.groundSlope[1]),
                       result.isStuck, result.onGround, static_cast<unsigned>(result.collisionKind),
                       result.surfaceMovingCollisionId ? result.surfaceMovingCollisionId->physicsEntityId : 0,
                       result.surfaceMovingCollisionId ? result.surfaceMovingCollisionId->collisionIndex : 0);
  }
  static std::uint64_t permutation(List<Candidate> const& candidates) {
    std::uint64_t hash = 14695981039346656037ull;
    auto append = [&hash](std::uint64_t value) {
      for (unsigned i = 0; i < 8; ++i) {
        hash = (hash ^ (value & 0xff)) * 1099511628211ull;
        value >>= 8;
      }
    };
    for (auto const& candidate : candidates) {
      append(static_cast<unsigned>(candidate.collisionKind));
      append(candidate.movingCollisionId ? candidate.movingCollisionId->physicsEntityId : 0);
      append(candidate.movingCollisionId ? candidate.movingCollisionId->collisionIndex : 0);
      for (auto const& vertex : candidate.poly) {
        append(std::bit_cast<std::uint32_t>(vertex[0]));
        append(std::bit_cast<std::uint32_t>(vertex[1]));
      }
    }
    return hash;
  }

  static bool same(Result const& a, Result const& b) {
    auto sameVector = [](Vec2F const& x, Vec2F const& y) {
      return std::bit_cast<std::uint32_t>(x[0]) == std::bit_cast<std::uint32_t>(y[0])
        && std::bit_cast<std::uint32_t>(x[1]) == std::bit_cast<std::uint32_t>(y[1]);
    };
    bool sameId = a.surfaceMovingCollisionId.has_value() == b.surfaceMovingCollisionId.has_value();
    if (sameId && a.surfaceMovingCollisionId)
      sameId = a.surfaceMovingCollisionId->physicsEntityId == b.surfaceMovingCollisionId->physicsEntityId
        && a.surfaceMovingCollisionId->collisionIndex == b.surfaceMovingCollisionId->collisionIndex;
    return sameVector(a.movement, b.movement) && sameVector(a.correction, b.correction)
      && sameVector(a.groundSlope, b.groundSlope) && sameId && a.isStuck == b.isStuck && a.onGround == b.onGround
      && a.collisionKind == b.collisionKind;
  }

  static std::vector<Fixture> fixtures() {
    std::vector<Fixture> fixtures;
    Fixture slope;
    slope.name = "Slope";
    slope.body = PolyF(RectF(-0.25f, 0.5f, 0.25f, 1.25f));
    slope.movement = Vec2F(0.5f, -0.25f);
    slope.candidates.push_back(candidate(PolyF{{-2, -1}, {2, -1}, {2, 1}}));
    fixtures.push_back(std::move(slope));

    Fixture corner;
    corner.name = "Corner";
    corner.body = PolyF(RectF(-0.25f, 0.25f, 0.25f, 0.75f));
    corner.movement = Vec2F(0.5f, -0.5f);
    corner.candidates.push_back(candidate(PolyF(RectF(-4, -1, 4, 0))));
    corner.candidates.push_back(candidate(PolyF(RectF(0.5f, 0, 1.5f, 4))));
    fixtures.push_back(std::move(corner));

    // Concentric moving rectangles have genuinely identical sort positions.
    // More than sixteen candidates forces the legacy unstable-sort tie behavior
    // beyond the small-range case; distinct geometry and IDs make order visible.
    for (unsigned count : {17u, 32u, 65u}) {
      Fixture ties;
      ties.name = std::format("EqualDistance{}", count);
      ties.body = PolyF(RectF(-0.125f, 0.25f, 0.125f, 0.5f));
      ties.movement = Vec2F(0.125f, -0.125f);
      for (unsigned i = 0; i < count; ++i) {
        float halfWidth = 1.0f + 0.125f * (i % 3);
        float halfHeight = 1.0f + 0.125f * (i % 5);
        ties.candidates.push_back(candidate(PolyF(RectF(-halfWidth, -halfHeight, halfWidth, halfHeight)),
                                            CollisionKind::Platform, MovingCollisionId(400 + i, i)));
      }
      fixtures.push_back(std::move(ties));

      Fixture multiPassTies;
      multiPassTies.name = std::format("EqualDistanceMultiPass{}", count);
      multiPassTies.body = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
      multiPassTies.slopeCorrection = false;
      for (unsigned i = 0; i < count; ++i) {
        float center = i % 2 ? 1.0f : -1.0f;
        // Binary fractions keep both centers and their squared distances exact.
        float halfWidth = 0.875f + 0.03125f * (i % 3);
        multiPassTies.candidates.push_back(candidate(PolyF(RectF(center - halfWidth, -2, center + halfWidth, 2)),
                                                     CollisionKind::Block, MovingCollisionId(800 + i, i)));
      }
      fixtures.push_back(std::move(multiPassTies));
    }

    Fixture corridor;
    corridor.name = "CorridorMultiPassFallback";
    corridor.body = PolyF(RectF(-3.25f, -0.25f, -2.75f, 0.25f));
    corridor.movement = Vec2F(3, 0);
    corridor.slopeCorrection = false;
    corridor.candidates.push_back(candidate(PolyF(RectF(-2, -2, -0.1f, 2))));
    corridor.candidates.push_back(candidate(PolyF(RectF(0.1f, -2, 2, 2))));
    fixtures.push_back(std::move(corridor));

    Fixture correctionLimit;
    correctionLimit.name = "CorrectionLimitStuckFallback";
    correctionLimit.body = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
    correctionLimit.movement = Vec2F(1, 0);
    correctionLimit.slopeCorrection = false;
    // The normal correction is 0.75, and restoring the original body requires
    // correction 1. Both must exceed the limit, including the fallback attempt.
    correctionLimit.maximumCorrection = 0.5f;
    correctionLimit.candidates.push_back(candidate(PolyF(RectF(0.5f, -10, 10, 10))));
    fixtures.push_back(std::move(correctionLimit));

    Fixture highVelocity;
    highVelocity.name = "HighVelocityDense";
    highVelocity.body = PolyF(RectF(-0.25f, 2, 0.25f, 2.5f));
    highVelocity.movement = Vec2F(4, -4);
    for (int x = -8; x <= 8; ++x) {
      for (int y = -4; y <= 0; ++y)
        highVelocity.candidates.push_back(candidate(PolyF(RectF(x, y, x + 1, y + 1))));
    }
    fixtures.push_back(std::move(highVelocity));

    Fixture unique;
    unique.name = "UniqueDistanceDenseFallback";
    unique.body = PolyF(RectF(-3.25f, -0.25f, -2.75f, 0.25f));
    unique.movement = Vec2F(3, 0);
    unique.slopeCorrection = false;
    unique.candidates.push_back(candidate(PolyF(RectF(-2, -2, -0.1f, 2))));
    unique.candidates.push_back(candidate(PolyF(RectF(0.1f, -2, 2, 2))));
    for (int i = 0; i < 127; ++i) {
      float x = 16.0f + i * 1.25f;
      unique.candidates.push_back(candidate(PolyF(RectF(x, 4, x + 1, 5))));
    }
    fixtures.push_back(std::move(unique));

    Fixture platform;
    platform.name = "MovingPlatformIdentity";
    platform.body = PolyF(RectF(-0.25f, 0.25f, 0.25f, 0.75f));
    // Keep the corrected movement shallower than the 60-degree slope-cheat
    // limit, so the upward contact (and its moving identity) is accepted.
    platform.movement = Vec2F(0.5f, -0.5f);
    platform.candidates.push_back(
      candidate(PolyF(RectF(-2, -0.25f, 2, 0)), CollisionKind::Platform, MovingCollisionId(42, 3)));
    fixtures.push_back(platform);
    platform.name = "IgnoredPlatform";
    platform.ignorePlatforms = true;
    fixtures.push_back(std::move(platform));

    Fixture stuck;
    stuck.name = "ZeroMovementStuck";
    stuck.body = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
    stuck.slopeCorrection = false;
    stuck.candidates.push_back(candidate(PolyF(RectF(-2, -2, -0.1f, 2))));
    stuck.candidates.push_back(candidate(PolyF(RectF(0.1f, -2, 2, 2))));
    fixtures.push_back(std::move(stuck));

    Fixture overflow;
    overflow.name = "OverflowDistance";
    overflow.body = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
    overflow.movement = Vec2F(1, 0);
    for (unsigned i = 1; i <= 32; ++i) {
      float x = 1.0e30f * i;
      overflow.candidates.push_back(candidate(PolyF(RectF(x, 4, x + 1.0e25f, 5))));
    }
    fixtures.push_back(std::move(overflow));

    Fixture empty;
    empty.name = "EmptyCandidates";
    empty.body = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
    empty.movement = Vec2F(1, -1);
    fixtures.push_back(empty);
    empty.name = "NullBody";
    empty.body = {};
    fixtures.push_back(std::move(empty));
    return fixtures;
  }

  static void run(benchmark::State& state, PerformanceBenchmarkSession& session, Fixture const& fixture) {
    constexpr std::size_t BatchSize = 64;
    std::array<List<Candidate>, BatchSize> candidates;
    std::array<Result, BatchSize> results;
    for (auto& working : candidates)
      working = fixture.candidates;
    auto referenceCandidates = fixture.candidates;
    auto reference = move(fixture, referenceCandidates);
    auto expected = signature(reference);
    auto expectedPermutation = permutation(referenceCandidates);
    state.SetLabel(std::format("{};permutation={:016x}", expected, expectedPermutation));

    // Simple boundary invariants supplement the recorded complex-fixture oracle.
    if (fixture.name == "MovingPlatformIdentity"
        && (!reference.surfaceMovingCollisionId || reference.surfaceMovingCollisionId->physicsEntityId != 42
            || reference.surfaceMovingCollisionId->collisionIndex != 3 || !reference.onGround || reference.isStuck)) {
      session.fail(state, std::format("Moving platform contact identity/ground invariant failed: {}", expected));
      return;
    }
    if ((fixture.name == "IgnoredPlatform" || fixture.name == "EmptyCandidates" || fixture.name == "NullBody")
        && (reference.movement != fixture.movement || reference.correction != Vec2F() || reference.isStuck)) {
      session.fail(state, "Unobstructed movement invariant failed");
      return;
    }
    if ((fixture.name == "CorridorMultiPassFallback" || fixture.name == "UniqueDistanceDenseFallback")
        && (reference.movement != Vec2F() || reference.isStuck)) {
      session.fail(state, std::format("Fallback must recover the original unobstructed body: {}", expected));
      return;
    }
    if (fixture.name == "CorrectionLimitStuckFallback"
        && (!reference.isStuck || reference.movement != Vec2F() || reference.correction != -fixture.movement)) {
      session.fail(state, std::format("Correction limit must reject normal and fallback displacement: {}", expected));
      return;
    }
    if (fixture.name == "ZeroMovementStuck" && !reference.isStuck) {
      session.fail(state, "Unsolvable zero-movement corridor must remain stuck");
      return;
    }

    for (auto _ : state) {
      (void)_;
      state.PauseTiming();
      for (auto& working : candidates)
        working = fixture.candidates;
      state.ResumeTiming();
      for (std::size_t i = 0; i < BatchSize; ++i) {
        results[i] = move(fixture, candidates[i]);
        benchmark::DoNotOptimize(results[i]);
      }
      state.PauseTiming();
      bool valid = true;
      for (auto const& result : results) {
        if (!same(result, reference)) {
          session.fail(state, "Collision consumer result changed after resetting identical operands");
          valid = false;
          break;
        }
      }
      state.ResumeTiming();
      if (!valid)
        break;
    }
    auto moves = state.iterations() * static_cast<std::int64_t>(BatchSize);
    state.SetItemsProcessed(moves);
    state.counters["candidates/move"] = static_cast<double>(fixture.candidates.size());
  }
};

std::expected<void, StarException> registerCollisionPerformanceBenchmarks(PerformanceBenchmarkSession& session) {
  for (auto& fixture : MovementCollisionBenchmarkAccess::fixtures()) {
    auto name = std::format("Collision/{}", fixture.name);
    benchmark::RegisterBenchmark(name.c_str(), [&session, fixture = std::move(fixture)](benchmark::State& state) {
      MovementCollisionBenchmarkAccess::run(state, session, fixture);
    });
  }
  return {};
}

}// namespace Star
