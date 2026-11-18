module;
import std;
import star.format;
import star.a_star;
import star.list;
import star.vector;
import star.rect;
import star.world;
import star.platformer_astar_types;
import star.actor_movement_controller;

namespace Star {
namespace PlatformerAStar {

  class PathFinder {
    class ConstructionKey {
      friend class PathFinder;
      ConstructionKey() = default;

    public:
      ConstructionKey(ConstructionKey const&) = default;
    };

  public:
    [[nodiscard]] static std::expected<std::shared_ptr<PathFinder>, StarException>
    create(World& world, Vec2F search_from, Vec2F search_to, ActorMovementParameters movement_parameters,
           Parameters search_parameters = Parameters());

    PathFinder(ConstructionKey, World& world, Vec2F search_from, Vec2F search_to,
               ActorMovementParameters movement_parameters, Parameters search_parameters);

    PathFinder(PathFinder const&) = delete;
    PathFinder& operator=(PathFinder const&) = delete;

    [[nodiscard]] std::expected<std::optional<bool>, StarException>
    explore(std::optional<unsigned> max_explore_nodes = {});
    std::optional<Path> const& result() const;
    std::optional<Path> extract_result();

  private:
    enum class BoundBoxKind { Full, Drop, Stand };

    [[nodiscard]] std::expected<void, StarException> init_a_star();

    float heuristic_cost(Vec2F const& from_position, Vec2F const& to_position) const;
    Edge default_cost_edge(Action action, Node const& source, Node const& target) const;
    [[nodiscard]] std::expected<void, StarException> neighbors(Node const& node, List<Edge>& neighbors) const;

    [[nodiscard]] std::expected<void, StarException>
    get_drop_neighbors(Node const& node, List<Edge>& neighbors) const;// drop through a platform
    [[nodiscard]] std::expected<void, StarException>
    get_walking_neighbors_in_direction(Node const& node, List<Edge>& neighbors, float direction) const;
    [[nodiscard]] std::expected<void, StarException> get_walking_neighbors(Node const& node,
                                                                           List<Edge>& neighbors) const;
    [[nodiscard]] std::expected<void, StarException> get_falling_neighbors(Node const& node,
                                                                           List<Edge>& neighbors) const;// freefall
    [[nodiscard]] std::expected<void, StarException> get_jumping_neighbors(Node const& node,
                                                                           List<Edge>& neighbors) const;
    [[nodiscard]] std::expected<void, StarException> get_arc_neighbors(Node const& node, List<Edge>& neighbors) const;
    [[nodiscard]] std::expected<void, StarException> get_swimming_neighbors(Node const& node,
                                                                            List<Edge>& neighbors) const;
    [[nodiscard]] std::expected<void, StarException> get_flying_neighbors(Node const& node,
                                                                          List<Edge>& neighbors) const;

    [[nodiscard]] std::expected<void, StarException>
    for_each_arc_velocity(float y_velocity,
                          std::function<std::expected<void, StarException>(Vec2F const&)> const& func) const;
    [[nodiscard]] std::expected<void, StarException>
    for_each_arc_neighbor(Node const& node, float y_velocity,
                          std::function<std::expected<void, StarException>(Node const&, bool)> const& func) const;
    [[nodiscard]] std::expected<Vec2F, StarException> acceleration(Vec2F pos) const;
    [[nodiscard]] std::expected<Vec2F, StarException> simulate_arc_collision(Vec2F position, Vec2F velocity, float dt,
                                                                             bool& collided_x, bool& collided_y) const;
    [[nodiscard]] std::expected<void, StarException>
    simulate_arc(Node const& node,
                 std::function<std::expected<void, StarException>(Node const&, bool)> const& func) const;

    [[nodiscard]] std::expected<bool, StarException> valid_position(Vec2F pos,
                                                                    BoundBoxKind bound_kind = BoundBoxKind::Full) const;
    [[nodiscard]] std::expected<bool, StarException>
    on_ground(Vec2F pos,
              BoundBoxKind bound_kind = BoundBoxKind::Full) const;// Includes non-solids: platforms, objects, etc.
    [[nodiscard]] std::expected<bool, StarException> on_solid_ground(Vec2F pos) const;// Includes only solids
    [[nodiscard]] std::expected<bool, StarException> in_liquid(Vec2F pos) const;

    [[nodiscard]] std::expected<RectF, StarException> bound_box(Vec2F pos,
                                                                BoundBoxKind bound_kind = BoundBoxKind::Full) const;
    // Returns a rect that covers the tiles below the entity's feet if it was at
    // pos
    [[nodiscard]] std::expected<RectI, StarException> ground_collision_rect(Vec2F pos, BoundBoxKind bound_kind) const;
    RectF standing_bounds() const;

    [[nodiscard]] std::expected<Vec2F, StarException> round_to_node(Vec2F pos) const;
    float distance(Vec2F a, Vec2F b) const;

    std::reference_wrapper<World> m_world;
    Vec2F m_search_from;
    Vec2F m_search_to;
    ActorMovementParameters m_movement_params;
    Parameters m_search_params;
    std::optional<AStar::Search<Edge, Node>> m_astar;
    mutable std::optional<RectF> m_standing_bounds;
  };
}
}

export module star.platformer_astar;

export namespace Star::PlatformerAStar {
  using ::Star::PlatformerAStar::PathFinder;
}
