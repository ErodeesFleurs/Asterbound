import star.format;
import star.poly;
import star.list;
import star.collision_block;
import std;

import star.collision_generator;

import star.vector;
import star.rect;
namespace Star {
namespace {
void append_block(List<CollisionBlock>& output, Vec2I const& space, CollisionKind kind, PolyF&& poly) {
  auto bounds = poly.bounding_box();
  output.emplace_back(kind, space, std::move(poly), bounds);
}
}

void CollisionGenerator::init(CollisionKindAccessor accessor) { m_accessor = std::move(accessor); }

std::expected<List<CollisionBlock>, StarException> CollisionGenerator::get_blocks(RectI const& region) const {
  if (region.is_null())
    return List<CollisionBlock>{};

  List<CollisionBlock> list;

  if (auto result = populate_collision_buffer(region); !result) return std::unexpected(std::move(result.error()));

  append_marching_squares(list, region);
  append_platforms(list, region);

  return list;
}

void CollisionGenerator::append_platforms(List<CollisionBlock>& list, RectI const& region) const {
  auto platform = [&](int x, int y) { return collision_kind(x, y) == CollisionKind::Platform; };
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      if (!platform(x, y))
        continue;

      auto add_segment = [&](Vec2F start, Vec2F end) {
        append_block(list, Vec2I(x, y), CollisionKind::Platform, {start, end});
      };
      auto rising = [&] { add_segment(Vec2F(x, y), Vec2F(x + 1, y + 1)); };
      auto falling = [&] { add_segment(Vec2F(x + 1, y), Vec2F(x, y + 1)); };
      auto flat = [&] { add_segment(Vec2F(x, y + 1), Vec2F(x + 1, y + 1)); };

      bool right = platform(x + 1, y);
      bool left = platform(x - 1, y);
      bool down_right = !right && platform(x + 1, y - 1);
      bool down_left = !left && platform(x - 1, y - 1);
      bool up_right = !left && !right && platform(x + 1, y + 1);
      bool up_left = !left && !right && platform(x - 1, y + 1);
      bool above = platform(x, y + 1);
      bool below = platform(x, y - 1);

      if (down_right && down_left && up_right && up_left) {
        rising();
        falling();
      } else if (above && below) {
        flat();
      } else if (up_right && down_left) {
        rising();
        if (above)
          add_segment(Vec2F(x + 1, y + 1), Vec2F(x + 2, y + 2));
      } else if (up_left && down_right) {
        falling();
        if (above)
          add_segment(Vec2F(x, y + 1), Vec2F(x - 1, y + 2));
      } else if (above && !down_right && !down_left) {
        flat();
      } else if (up_left && !up_right) {
        falling();
      } else if (up_right && !up_left) {
        rising();
      } else if (down_right && (left || !below)) {
        falling();
      } else if (down_left && (right || !below)) {
        rising();
      } else {
        flat();
      }
    }
  }
}

void CollisionGenerator::append_marching_squares(List<CollisionBlock>& list, RectI const& region) const {
  // Half-tile offsets clockwise around the square, plus its center (index 8).
  static Vec2F const offsets[] = {
    Vec2F(0.5, 0.5),
    Vec2F(0.5, 1.0),
    Vec2F(0.5, 1.5),
    Vec2F(1.0, 1.5),
    Vec2F(1.5, 1.5),
    Vec2F(1.5, 1.0),
    Vec2F(1.5, 0.5),
    Vec2F(1.0, 0.5),
    Vec2F(1.0, 1.0)
  };

  static constexpr std::initializer_list<std::uint8_t> shapes[] = {
    {}, {1, 2, 3}, {3, 4, 5}, {1, 2, 4, 5},
    {7, 5, 6}, {1, 2, 3, 5, 6, 7}, {7, 3, 4, 6}, {1, 2, 4, 6, 7},
    {0, 1, 7}, {0, 2, 3, 7}, {0, 1, 3, 4, 5, 7}, {0, 2, 4, 5, 7},
    {0, 1, 5, 6}, {0, 2, 3, 5, 6}, {0, 1, 3, 4, 6}, {0, 2, 4, 6},
    {5, 6, 7, 8}, {0, 1, 8, 7}, // Squared top left/right.
    {0, 2, 3, 8}, {0, 8, 5, 6}, // Hollow bottom left (two polygons).
    {0, 1, 8, 6}, {6, 8, 3, 4}  // Hollow bottom right (two polygons).
  };

  constexpr auto solid = CollisionKind::Dynamic;
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      auto ll = collision_kind(x, y);
      auto lr = collision_kind(x + 1, y);
      auto ul = collision_kind(x, y + 1);
      auto ur = collision_kind(x + 1, y + 1);
      unsigned mask = (ul >= solid) | ((ur >= solid) << 1) | ((lr >= solid) << 2) | ((ll >= solid) << 3);
      if (!mask)
        continue;

      auto kind = std::max({ll, lr, ul, ur});
      Vec2F origin(x, y);
      auto add_shape = [&](unsigned index) {
        auto const& shape = shapes[index];
        PolyF poly;
        poly.vertices().reserve(shape.size());
        for (auto offset : shape)
          poly.add(origin + offsets[offset]);
        append_block(list, Vec2I(x, y), kind, std::move(poly));
      };

      if (mask == 4 && collision_kind(x + 2, y) >= solid && collision_kind(x + 2, y + 1) < solid &&
          collision_kind(x, y - 1) < solid) {
        add_shape(16);
      } else if (mask == 8 && collision_kind(x - 1, y) >= solid && collision_kind(x - 1, y + 1) < solid &&
                 collision_kind(x + 1, y - 1) < solid) {
        add_shape(17);
      } else if (mask == 13 && collision_kind(x, y + 2) >= solid && collision_kind(x + 1, y + 2) < solid &&
                 collision_kind(x + 2, y) >= solid) {
        add_shape(18);
        add_shape(19);
      } else if (mask == 14 && collision_kind(x, y + 2) < solid && collision_kind(x + 1, y + 2) >= solid &&
                 collision_kind(x - 1, y) >= solid) {
        add_shape(20);
        add_shape(21);
      } else {
        add_shape(mask);
      }
    }
  }
}

std::expected<void, StarException> CollisionGenerator::populate_collision_buffer(RectI const& region) const {
  auto halo_fits = [](int low, int high) {
    constexpr int minimum = std::numeric_limits<int>::min();
    constexpr int maximum = std::numeric_limits<int>::max();
    return low >= minimum + block_influence_radius && high <= maximum - block_influence_radius &&
           low <= high && std::int64_t(high) - low <= maximum - 2 * block_influence_radius;
  };
  if (!halo_fits(region.x_min(), region.x_max()) || !halo_fits(region.y_min(), region.y_max()))
    return std::unexpected(StarException("CollisionGenerator region exceeds collision buffer coordinate range"));

  int xmin = region.x_min() - block_influence_radius;
  int ymin = region.y_min() - block_influence_radius;
  int xmax = region.x_max() + block_influence_radius;
  int ymax = region.y_max() + block_influence_radius;

  if (!m_accessor) return std::unexpected(StarException("CollisionGenerator not initialized"));
  auto resized = m_collision_buffer.set_size(xmax - xmin, ymax - ymin);
  if (!resized) return std::unexpected(std::move(resized.error()));
  m_buffer_corner = {xmin, ymin};
  for (int x = xmin; x < xmax; ++x)
    for (int y = ymin; y < ymax; ++y) {
      auto kind = m_accessor(x, y);
      if (!kind) return std::unexpected(std::move(kind.error()));
      m_collision_buffer(x - xmin, y - ymin) = *kind;
    }
  return {};
}

CollisionKind CollisionGenerator::collision_kind(int x, int y) const {
  return m_collision_buffer(x - m_buffer_corner[0], y - m_buffer_corner[1]);
}
  
}
