module;

import std;
import star.list;
import star.map;
import star.block_allocator;
import star.format;

namespace Star::AStar {

struct Score {
  double g_score = std::numeric_limits<double>::max();
  double h_score = 0;
  double f_score = std::numeric_limits<double>::max();
};

template <typename Node>
concept search_node = std::copyable<Node> && requires(Node const& left, Node const& right) {
  { left < right } -> std::convertible_to<bool>;
};

template <typename Edge, typename Node>
concept search_edge_for = search_node<Node> && std::copyable<Edge> && requires(Edge const& edge) {
  { edge.cost } -> std::convertible_to<double>;
  { edge.source } -> std::convertible_to<Node>;
  { edge.target } -> std::convertible_to<Node>;
};

template <std::copyable Edge> using Path = List<Edge>;

template <typename Edge, typename Node>
  requires search_edge_for<Edge, Node>
class Search {
public:
  using HeuristicFunction = std::function<std::expected<double, StarException>(Node const&, Node const&)>;
  using NeighborFunction = std::function<std::expected<void, StarException>(Node const&, List<Edge>&)>;
  using GoalFunction = std::function<std::expected<bool, StarException>(Node const&)>;
  using ValidateEndFunction = std::function<std::expected<bool, StarException>(Edge const&)>;

  Search(HeuristicFunction heuristic_cost, NeighborFunction get_adjacent, GoalFunction goal_reached,
         bool return_best_if_failed = false, std::optional<ValidateEndFunction> validate_end = {},
         std::optional<double> max_f_score = {}, std::optional<unsigned> max_nodes_to_search = {})
      : m_heuristic_cost(std::move(heuristic_cost)), m_get_adjacent(std::move(get_adjacent)),
        m_goal_reached(std::move(goal_reached)), m_return_best_if_failed(return_best_if_failed),
        m_validate_end(std::move(validate_end)), m_max_f_score(max_f_score),
        m_max_nodes_to_search(max_nodes_to_search) {}

  Search(Search const&) = default;
  Search& operator=(Search const&) = default;
  Search(Search&& other)
      : m_heuristic_cost(std::move(other.m_heuristic_cost)), m_get_adjacent(std::move(other.m_get_adjacent)),
        m_goal_reached(std::move(other.m_goal_reached)), m_return_best_if_failed(other.m_return_best_if_failed),
        m_validate_end(std::exchange(other.m_validate_end, std::nullopt)),
        m_max_f_score(std::exchange(other.m_max_f_score, std::nullopt)),
        m_max_nodes_to_search(std::exchange(other.m_max_nodes_to_search, std::nullopt)),
        m_goal(std::exchange(other.m_goal, std::nullopt)), m_nodes(std::move(other.m_nodes)),
        m_open_heap(std::move(other.m_open_heap)), m_neighbors(std::move(other.m_neighbors)),
        m_closed_count(std::exchange(other.m_closed_count, 0)),
        m_early_exploration(std::exchange(other.m_early_exploration, std::nullopt)),
        m_outcome(std::exchange(other.m_outcome, std::optional<bool>{false})),
        m_result(std::exchange(other.m_result, std::nullopt)) {}

  Search& operator=(Search&& other) {
    if (this != &other) {
      m_heuristic_cost = std::move(other.m_heuristic_cost);
      m_get_adjacent = std::move(other.m_get_adjacent);
      m_goal_reached = std::move(other.m_goal_reached);
      m_return_best_if_failed = other.m_return_best_if_failed;
      m_validate_end = std::exchange(other.m_validate_end, std::nullopt);
      m_max_f_score = std::exchange(other.m_max_f_score, std::nullopt);
      m_max_nodes_to_search = std::exchange(other.m_max_nodes_to_search, std::nullopt);
      m_goal = std::exchange(other.m_goal, std::nullopt);
      m_nodes = std::move(other.m_nodes);
      m_open_heap = std::move(other.m_open_heap);
      m_neighbors = std::move(other.m_neighbors);
      m_closed_count = std::exchange(other.m_closed_count, 0);
      m_early_exploration = std::exchange(other.m_early_exploration, std::nullopt);
      m_outcome = std::exchange(other.m_outcome, std::optional<bool>{false});
      m_result = std::exchange(other.m_result, std::nullopt);
    }
    return *this;
  }

  // Callbacks run synchronously and must not reenter this search. start resets
  // paths and sticky callback failures while retaining owned scratch capacity.
  [[nodiscard]] std::expected<void, StarException> start(Node start_node, Node goal_node) {
    m_goal = std::move(goal_node);
    m_nodes.clear();
    m_open_heap.clear();
    m_neighbors.clear();
    m_closed_count = 0;
    m_early_exploration.reset();
    m_outcome = std::optional<bool>{};
    m_result.reset();
    if (!m_heuristic_cost)
      return fail(StarException("AStar search requires a heuristic callback"));
    auto heuristic = m_heuristic_cost(start_node, *m_goal);
    if (!heuristic)
      return fail(std::move(heuristic.error()));
    auto position = m_nodes.try_emplace(start_node).first;
    position->second = NodeMeta{Score{0, *heuristic, *heuristic}, {}, 1, NodeState::Open};
    enqueue(ScoredNode{*heuristic, 1, std::move(start_node)});
    return {};
  }

  // nullopt means pending, true means reached goal, false means exhausted or
  // limited, even when a closest partial result exists. Terminal outcomes and
  // errors repeat until start(). A budget counts queue candidates, including
  // stale entries; the legacy total limit remains closed_count > limit.
  [[nodiscard]] std::expected<std::optional<bool>, StarException>
  explore(std::optional<unsigned> max_explore_nodes = {}) {
    if (!m_outcome || m_outcome->has_value())
      return m_outcome;
    while (true) {
      if ((m_max_nodes_to_search && m_closed_count > *m_max_nodes_to_search)
          || (m_open_heap.empty() && !m_early_exploration))
        return finish_partial();
      if (max_explore_nodes) {
        if (*max_explore_nodes == 0)
          return std::nullopt;
        --*max_explore_nodes;
      }
      auto current = pop_next();
      auto& current_meta = m_nodes.find(current.node)->second;
      if (current_meta.state != NodeState::Open || current_meta.revision != current.revision)
        continue;
      auto current_score = current_meta.score;
      if (!m_goal_reached)
        return fail(StarException("AStar search requires a goal callback"));
      auto reached = m_goal_reached(current.node);
      if (!reached)
        return fail(std::move(reached.error()));
      if (*reached) {
        m_result = reconstruct_path(current.node);
        m_outcome = true;
        return true;
      }
      current_meta.state = NodeState::Closed;
      ++m_closed_count;
      m_neighbors.clear();
      if (!m_get_adjacent)
        return fail(StarException("AStar search requires a neighbor callback"));
      auto generated = m_get_adjacent(current.node, m_neighbors);
      if (!generated)
        return fail(std::move(generated.error()));
      for (auto const& edge : m_neighbors) {
        auto [position, inserted] = m_nodes.try_emplace(edge.target);
        auto& target = position->second;
        if (target.state == NodeState::Closed)
          continue;
        double g_score = current_score.g_score + edge.cost;
        if (!inserted && !(g_score < target.score.g_score))
          continue;
        if (!m_heuristic_cost)
          return fail(StarException("AStar search requires a heuristic callback"));
        auto heuristic = m_heuristic_cost(edge.target, *m_goal);
        if (!heuristic)
          return fail(std::move(heuristic.error()));
        target.score = Score{g_score, *heuristic, g_score + *heuristic};
        target.came_from = edge;
        ++target.revision;
        target.state = NodeState::Pruned;
        if (m_max_f_score && target.score.f_score > *m_max_f_score)
          continue;
        target.state = NodeState::Open;
        ScoredNode next{target.score.f_score, target.revision, edge.target};
        if (next.f_score <= current_score.f_score) {
          if (!m_early_exploration) {
            m_early_exploration = std::move(next);
            continue;
          }
          if (m_early_exploration->f_score > next.f_score) {
            enqueue(std::move(*m_early_exploration));
            m_early_exploration = std::move(next);
            continue;
          }
        }
        enqueue(std::move(next));
      }
    }
  }

  std::optional<Path<Edge>> const& result() const { return m_result; }
  std::optional<Path<Edge>> extract_result() { return std::exchange(m_result, std::nullopt); }

  [[nodiscard]] std::expected<std::reference_wrapper<std::optional<Path<Edge>> const>, StarException>
  find_path(Node start_node, Node goal_node) {
    auto started = start(std::move(start_node), std::move(goal_node));
    if (!started)
      return std::unexpected(std::move(started.error()));
    auto explored = explore();
    if (!explored)
      return std::unexpected(std::move(explored.error()));
    return std::cref(m_result);
  }

private:
  enum class NodeState { Pruned, Open, Closed };
  struct ScoredNode {
    bool operator<(ScoredNode const& other) const { return f_score > other.f_score; }
    double f_score;
    std::size_t revision;
    Node node;
  };
  struct NodeMeta {
    Score score;
    std::optional<Edge> came_from;
    std::size_t revision = 0;
    NodeState state = NodeState::Pruned;
  };

  std::unexpected<StarException> fail(StarException error) {
    m_outcome = std::unexpected(std::move(error));
    return std::unexpected(m_outcome.error());
  }

  void enqueue(ScoredNode node) {
    m_open_heap.push_back(std::move(node));
    std::push_heap(m_open_heap.begin(), m_open_heap.end());
  }

  ScoredNode pop_next() {
    if (m_early_exploration) {
      auto current = std::move(*m_early_exploration);
      m_early_exploration.reset();
      return current;
    }
    std::pop_heap(m_open_heap.begin(), m_open_heap.end());
    auto current = std::move(m_open_heap.back());
    m_open_heap.pop_back();
    return current;
  }

  std::expected<std::optional<bool>, StarException> finish_partial() {
    if (m_return_best_if_failed) {
      double best_score = std::numeric_limits<double>::max();
      std::optional<std::reference_wrapper<Node const>> best_node;
      for (auto const& [node, metadata] : m_nodes) {
        if (metadata.state != NodeState::Closed)
          continue;
        if (m_validate_end && metadata.came_from) {
          if (!*m_validate_end)
            return fail(StarException("AStar search requires a callable end validator"));
          auto valid = (*m_validate_end)(*metadata.came_from);
          if (!valid)
            return fail(std::move(valid.error()));
          if (!*valid)
            continue;
        }
        if (metadata.score.h_score < best_score) {
          best_score = metadata.score.h_score;
          best_node = std::cref(node);
        }
      }
      if (best_node)
        m_result = reconstruct_path(best_node->get());
    }
    m_outcome = false;
    return false;
  }

  Path<Edge> reconstruct_path(Node const& node) const {
    Path<Edge> path;
    auto position = m_nodes.find(node);
    while (position != m_nodes.end() && position->second.came_from) {
      auto const& edge = *position->second.came_from;
      path.push_back(edge);
      position = m_nodes.find(edge.source);
    }
    std::reverse(path.begin(), path.end());
    return path;
  }

  HeuristicFunction m_heuristic_cost;
  NeighborFunction m_get_adjacent;
  GoalFunction m_goal_reached;
  bool m_return_best_if_failed;
  std::optional<ValidateEndFunction> m_validate_end;
  std::optional<double> m_max_f_score;
  std::optional<unsigned> m_max_nodes_to_search;
  std::optional<Node> m_goal;
  Map<Node, NodeMeta, std::less<Node>, BlockAllocator<std::pair<Node const, NodeMeta>, 1024>> m_nodes;
  std::vector<ScoredNode> m_open_heap;
  List<Edge> m_neighbors;
  std::size_t m_closed_count = 0;
  std::optional<ScoredNode> m_early_exploration;
  std::expected<std::optional<bool>, StarException> m_outcome;
  std::optional<Path<Edge>> m_result;
};

}// namespace Star::AStar

export module star.a_star;

export namespace Star::AStar {
using ::Star::AStar::Path;
using ::Star::AStar::Score;
using ::Star::AStar::Search;
using ::Star::AStar::search_edge_for;
using ::Star::AStar::search_node;
}
