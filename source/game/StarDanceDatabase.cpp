import std;
import star.assets;
import star.dance_database;
import star.format;
import star.game_timers;
import star.json;
import star.json_extra;
import star.logging;
import star.math_common;
import star.root;
import star.string;
import star.vector;

namespace Star {

#pragma float_control(precise, on, push)
namespace {

std::expected<float, StarException> finite_float(double value) {
  if (!is_finite(value) || std::abs(value) > std::numeric_limits<float>::max())
    return std::unexpected(StarException("Dance number outside the finite float range"));
  return static_cast<float>(value);
}

std::expected<float, StarException> read_interval(double value) {
  if (value < 0)
    return std::unexpected(StarException("Dance time interval must be nonnegative"));
  return finite_float(value);
}

std::expected<Vec2F, StarException> read_offset(Json const& value) {
  if (value.is_null())
    return Vec2F{};
  auto offset = json_to_vec2d(value);
  if (!offset)
    return std::unexpected(std::move(offset.error()));
  auto x = finite_float((*offset)[0]);
  if (!x)
    return std::unexpected(std::move(x.error()));
  auto y = finite_float((*offset)[1]);
  if (!y)
    return std::unexpected(std::move(y.error()));
  return Vec2F(*x, *y);
}

std::expected<DanceStep, StarException> read_pose(Json const& body, Json const& front_arm, Json const& back_arm,
    Json const& head_offset, Json const& front_offset, Json const& back_offset) {
  auto body_frame = body.opt_string();
  if (!body_frame)
    return std::unexpected(std::move(body_frame.error()));
  auto front_arm_frame = front_arm.opt_string();
  if (!front_arm_frame)
    return std::unexpected(std::move(front_arm_frame.error()));
  auto back_arm_frame = back_arm.opt_string();
  if (!back_arm_frame)
    return std::unexpected(std::move(back_arm_frame.error()));
  auto head = read_offset(head_offset);
  if (!head)
    return std::unexpected(std::move(head.error()));
  auto front = read_offset(front_offset);
  if (!front)
    return std::unexpected(std::move(front.error()));
  auto back = read_offset(back_offset);
  if (!back)
    return std::unexpected(std::move(back.error()));
  return DanceStep{std::move(*body_frame), std::move(*front_arm_frame), std::move(*back_arm_frame), *head, *front, *back};
}

std::expected<DanceStep, StarException> read_dance_step(Json const& step) {
  if (step.is_type(Json::Type::Array)) {
    auto entries = step.array_ptr();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    auto const& fields = **entries;
    if (fields.size() < 6)
      return std::unexpected(StarException("Legacy dance step needs six fields"));
    return read_pose(fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]);
  }

  auto fields = step.object_ptr();
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  Json const missing;
  auto field = [&](String const& key) -> Json const& {
    auto found = (*fields)->find_ref(key);
    return found ? found->get() : missing;
  };
  auto pose = read_pose(field("bodyFrame"), field("frontArmFrame"), field("backArmFrame"),
                        field("headOffset"), field("frontArmOffset"), field("backArmOffset"));
  if (!pose)
    return pose;
  auto angle = [&](String const& key, float fallback) -> std::expected<float, StarException> {
    auto const& value = field(key);
    if (value.is_null())
      return fallback;
    return value.to_double().and_then(finite_float);
  };
  auto front = angle("frontArmRotation", 0);
  if (!front)
    return std::unexpected(std::move(front.error()));
  // Objects predating backArmRotation used the front arm angle for both arms.
  auto back = angle("backArmRotation", *front);
  if (!back)
    return std::unexpected(std::move(back.error()));
  pose->front_arm_rotation = *front;
  pose->back_arm_rotation = *back;
  return pose;
}

std::expected<std::shared_ptr<Dance const>, StarException> read_dance(Json const& config) {
  auto name = config.get_string("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto states = config.get("states").and_then(json_to_string_list);
  if (!states)
    return std::unexpected(std::move(states.error()));
  auto cycle = config.get_double("cycle").and_then(read_interval);
  if (!cycle)
    return std::unexpected(std::move(cycle.error()));
  auto cyclic = config.get_bool("cyclic");
  if (!cyclic)
    return std::unexpected(std::move(cyclic.error()));
  auto duration = config.get_double("duration").and_then(read_interval);
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  auto steps = config.get("steps").and_then([](Json const& value) { return json_to_list<DanceStep>(value, read_dance_step); });
  if (!steps)
    return std::unexpected(std::move(steps.error()));
  if (*cycle == 0 && !steps->empty())
    return std::unexpected(StarException("Dance cycle must be positive for a nonempty step list"));
  return std::make_shared<Dance const>(std::move(*name), std::move(*states), *cycle, *cyclic, *duration, std::move(*steps));
}

}

std::optional<std::reference_wrapper<DanceStep const>> Dance::step_at(float timer) const& {
  if (steps.empty() || !is_finite(timer) || !is_finite(cycle) || cycle <= 0)
    return {};
  if (timer <= 0)
    return std::cref(steps.front());
  if (timer >= cycle) {
    if (!cyclic)
      return std::cref(steps.back());
    timer = std::fmod(timer, cycle);
  }
  // Keep legacy float rounding at frame boundaries, widening only on overflow.
  float scaled = timer * static_cast<float>(steps.size());
  double frame = scaled <= std::numeric_limits<float>::max() ? static_cast<double>(scaled / cycle)
                                                             : static_cast<double>(timer) * steps.size() / cycle;
  return std::cref(steps[std::min(static_cast<std::size_t>(frame), steps.size() - 1)]);
}
#pragma float_control(pop)

std::expected<void, StarException> TimedDance::set(Root& root, std::optional<String> const& requested) {
  float duration = 0;
  if (requested) {
    auto database = root.dance_database();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto dance = (*database)->get_dance(*requested);
    if (!dance)
      return std::unexpected(std::move(dance.error()));
    duration = (*dance)->duration;
  }
  name = requested;
  timer = GameTimer(duration);
  return {};
}

void TimedDance::tick(float dt) {
  if (name && timer.tick(dt))
    name.reset();
}

std::expected<std::shared_ptr<DanceDatabase>, StarException> DanceDatabase::create(Assets const& assets, Logger& logger) {
  auto database = std::make_shared<DanceDatabase>(ConstructionKey{}, logger);
  auto files = assets.scan_extension("dance");
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& file : files->get()) {
    auto dance = assets.json(file.string()).and_then(read_dance);
    if (!dance) {
      auto error = assets.with_asset_context(std::move(dance.error()), file.string(), "Loading dance");
      auto logged = logger.error_exception(Star::format("Error loading dance file {}", file.string()), ExceptionDiagnostic(error));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    database->m_dances[(*dance)->name] = std::move(*dance);
  }
  return database;
}

std::expected<std::shared_ptr<Dance const>, StarException> DanceDatabase::get_dance(String const& name) const {
  if (auto dance = m_dances.find_ref(name))
    return dance->get();
  auto logged = m_logger.error("Invalid dance '{}', using default", name);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (auto fallback = m_dances.find_ref("assetmissing"))
    return fallback->get();
  return std::unexpected(StarException("Missing fallback dance 'assetmissing'"));
}

}
