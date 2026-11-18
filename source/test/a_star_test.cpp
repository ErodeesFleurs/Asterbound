#include "gtest/gtest.h"
import std;
import star.a_star;
import star.format;
import star.list;

using namespace Star;

namespace {

// Ordered metadata needs strict ordering, not all six relational operators or
// a default-constructed sentinel node.
struct Node {
  explicit Node(int value) : id(value) {}
  int id;
  bool operator==(Node const&) const = default;
  bool operator<(Node const& other) const { return id < other.id; }
};

struct Edge {
  double cost;
  Node source;
  Node target;
  bool operator==(Edge const&) const = default;
};

struct UnorderedNode {
  int id;
  bool operator==(UnorderedNode const&) const = default;
};
struct NoncopyableNode {
  NoncopyableNode() = default;
  NoncopyableNode(NoncopyableNode const&) = delete;
  bool operator<(NoncopyableNode const&) const { return false; }
};
template <std::destructible NodeType> struct TypedEdge {
  double cost;
  NodeType source;
  NodeType target;
};
struct MissingTarget {
  double cost;
  Node source;
};
struct WrongSource {
  double cost;
  std::string source;
  Node target;
};
struct WrongCost {
  std::string cost;
  Node source;
  Node target;
};

template <typename EdgeType, typename NodeType>
concept search_consumer = requires { typename AStar::Search<EdgeType, NodeType>; };

using Search = AStar::Search<Edge, Node>;
using Path = AStar::Path<Edge>;
static_assert(search_consumer<Edge, Node>);
static_assert(!std::default_initializable<Node>);
static_assert(!std::totally_ordered<Node>);
static_assert(!search_consumer<TypedEdge<UnorderedNode>, UnorderedNode>);
static_assert(!search_consumer<TypedEdge<NoncopyableNode>, NoncopyableNode>);
static_assert(!search_consumer<MissingTarget, Node>);
static_assert(!search_consumer<WrongSource, Node>);
static_assert(!search_consumer<WrongCost, Node>);
static_assert(std::same_as<Search::HeuristicFunction,
                           std::function<std::expected<double, StarException>(Node const&, Node const&)>>);
static_assert(
  std::same_as<Search::NeighborFunction, std::function<std::expected<void, StarException>(Node const&, List<Edge>&)>>);
static_assert(std::same_as<Search::GoalFunction, std::function<std::expected<bool, StarException>(Node const&)>>);
static_assert(
  std::same_as<Search::ValidateEndFunction, std::function<std::expected<bool, StarException>(Edge const&)>>);
static_assert(std::same_as<decltype(std::declval<Search&>().extract_result()), std::optional<Path>>);

enum class Failure { None, StartHeuristic, NeighborHeuristic, Goal, Neighbors, Validator };

struct Graph {
  std::vector<Edge> edges;
  std::map<int, double> heuristic;
  int goal = 9;
  std::vector<int> heuristic_goals;
  std::vector<int> examined;
  std::vector<int> expanded;
  std::vector<int> validated;
  std::vector<int> rejected_ends;
  std::array<unsigned, 4> callback_calls{};
  Failure failure = Failure::None;
  StarException callback_error = OutOfRangeException("astar-callback-payload", false);
};

// Each callback owns its graph; no fixture or Search callback retains a raw
// borrow of a stack-local graph, node, edge, or another Search's metadata.
Search make_search(std::shared_ptr<Graph> graph, bool return_best = false, std::optional<double> max_f_score = {},
                   std::optional<unsigned> max_nodes_to_search = {}, bool validate_end = false) {
  std::optional<Search::ValidateEndFunction> validator;
  if (validate_end) {
    validator = [graph](Edge const& edge) -> std::expected<bool, StarException> {
      ++graph->callback_calls[3];
      graph->validated.push_back(edge.target.id);
      if (graph->failure == Failure::Validator)
        return std::unexpected(graph->callback_error);
      return std::ranges::find(graph->rejected_ends, edge.target.id) == graph->rejected_ends.end();
    };
  }
  return Search(
    [graph](Node const& node, Node const& goal) -> std::expected<double, StarException> {
      ++graph->callback_calls[0];
      graph->heuristic_goals.push_back(goal.id);
      if ((graph->failure == Failure::StartHeuristic && node.id == 0)
          || (graph->failure == Failure::NeighborHeuristic && node.id == 1))
        return std::unexpected(graph->callback_error);
      auto found = graph->heuristic.find(node.id);
      return found == graph->heuristic.end() ? 0.0 : found->second;
    },
    [graph](Node const& node, List<Edge>& neighbors) -> std::expected<void, StarException> {
      ++graph->callback_calls[1];
      graph->expanded.push_back(node.id);
      if (graph->failure == Failure::Neighbors)
        return std::unexpected(graph->callback_error);
      for (auto const& edge : graph->edges) {
        if (edge.source == node)
          neighbors.push_back(edge);
      }
      return {};
    },
    [graph](Node const& node) -> std::expected<bool, StarException> {
      ++graph->callback_calls[2];
      graph->examined.push_back(node.id);
      if (graph->failure == Failure::Goal)
        return std::unexpected(graph->callback_error);
      return node.id == graph->goal;
    },
    return_best, std::move(validator), max_f_score, max_nodes_to_search);
}

std::shared_ptr<Graph> rediscovery_graph() {
  auto graph = std::make_shared<Graph>();
  graph->edges = {{100, Node(0), Node(1)}, {1, Node(0), Node(2)}, {1, Node(2), Node(1)}, {1, Node(1), Node(9)}};
  return graph;
}

std::vector<int> path_targets(Path const& path) {
  std::vector<int> targets;
  for (auto const& edge : path)
    targets.push_back(edge.target.id);
  return targets;
}

double path_cost(Path const& path) {
  double cost = 0;
  for (auto const& edge : path)
    cost += edge.cost;
  return cost;
}

void expect_status(Search& search, std::optional<bool> expected, std::optional<unsigned> budget = {}) {
  auto status = search.explore(budget);
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, expected);
}

void expect_path(Search const& search, std::vector<int> const& targets, double cost) {
  ASSERT_TRUE(search.result());
  EXPECT_EQ(path_targets(*search.result()), targets);
  EXPECT_DOUBLE_EQ(path_cost(*search.result()), cost);
}

}// namespace

TEST(AStar, CheaperOpenAndPreviouslyPrunedNodesAreRediscovered) {
  for (std::optional<double> cap : {std::optional<double>{}, std::optional<double>{3.0}}) {
    SCOPED_TRACE(cap ? "max_f_score=3" : "unbounded f score");
    auto graph = rediscovery_graph();
    auto search = make_search(graph, false, cap);
    ASSERT_TRUE(search.start(Node(0), Node(9)));
    expect_status(search, true);
    expect_path(search, {2, 1, 9}, 3);
    EXPECT_EQ(graph->expanded, (std::vector<int>{0, 2, 1}));
  }
}

TEST(AStar, StaleQueuedCandidatesConsumeBudgetWithoutExpandingTwice) {
  auto graph = std::make_shared<Graph>();
  graph->edges = {{100, Node(0), Node(1)}, {1, Node(0), Node(2)}, {150, Node(0), Node(3)}, {1, Node(2), Node(1)}};
  auto search = make_search(graph);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  for (int candidate : {0, 2, 1}) {
    expect_status(search, std::nullopt, 1);
    ASSERT_FALSE(graph->examined.empty());
    EXPECT_EQ(graph->examined.back(), candidate);
  }
  auto before_stale = graph->callback_calls;
  expect_status(search, std::nullopt, 0);
  expect_status(search, std::nullopt, 1);// obsolete A=100, not C=150
  EXPECT_EQ(graph->callback_calls, before_stale);
  expect_status(search, false, 1);
  EXPECT_EQ(graph->examined, (std::vector<int>{0, 2, 1, 3}));
  EXPECT_EQ(graph->expanded, graph->examined);
  EXPECT_FALSE(search.result());
}

TEST(AStar, DisplacedAndImprovedEarlyCandidatesResumeWithTinyBudgets) {
  auto graph = std::make_shared<Graph>();
  graph->heuristic[0] = 10;
  graph->edges = {{8, Node(0), Node(1)}, {1, Node(0), Node(2)}, {0.5, Node(0), Node(1)}, {20, Node(0), Node(3)}};
  auto search = make_search(graph, true);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, std::nullopt, 1);// early A8 -> early B1 -> early A0.5
  expect_status(search, std::nullopt, 0);
  expect_status(search, std::nullopt, 1);
  EXPECT_EQ(graph->examined, (std::vector<int>{0, 1}));
  expect_status(search, std::nullopt, 1);
  EXPECT_EQ(graph->examined, (std::vector<int>{0, 1, 2}));
  auto before_stale = graph->callback_calls;
  expect_status(search, std::nullopt, 1);// displaced A8
  EXPECT_EQ(graph->callback_calls, before_stale);
  expect_status(search, false, 1);
  EXPECT_EQ(graph->expanded, (std::vector<int>{0, 1, 2, 3}));
  expect_path(search, {1}, 0.5);
}

TEST(AStar, ClosedNodesRemainClosedAfterACheaperLaterEdge) {
  auto graph = std::make_shared<Graph>();
  graph->heuristic[2] = 100;
  graph->edges = {{10, Node(0), Node(1)}, {1, Node(0), Node(2)}, {1, Node(2), Node(1)}, {200, Node(1), Node(9)}};
  auto search = make_search(graph);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, true);
  expect_path(search, {1, 9}, 210);
  EXPECT_EQ(graph->expanded, (std::vector<int>{0, 1, 2}));
}

TEST(AStar, ClosestPartialFailureAndExtractionNeverTurnIntoSuccess) {
  auto graph = std::make_shared<Graph>();
  graph->heuristic = {{0, 10}, {1, 1}};
  graph->edges = {{2, Node(0), Node(1)}};
  auto search = make_search(graph, true);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, false);
  expect_path(search, {1}, 2);
  auto calls = graph->callback_calls;
  expect_status(search, false, 0);
  expect_status(search, false);
  auto extracted = search.extract_result();
  ASSERT_TRUE(extracted);
  EXPECT_EQ(path_targets(*extracted), (std::vector<int>{1}));
  EXPECT_FALSE(search.result());
  EXPECT_FALSE(search.extract_result());
  expect_status(search, false, 0);
  expect_status(search, false);
  EXPECT_EQ(graph->callback_calls, calls);
}

TEST(AStar, ClosestFallbackUsesNodeOrderForEqualHeuristicsAndFiltersInvalidEnds) {
  for (bool reject_first_tie : {false, true}) {
    auto graph = std::make_shared<Graph>();
    graph->heuristic = {{0, 10}, {1, 2}, {2, 2}, {3, 1}};
    graph->edges = {{1, Node(0), Node(2)}, {20, Node(0), Node(1)}, {1, Node(0), Node(3)}};
    graph->rejected_ends = reject_first_tie ? std::vector<int>{1, 3} : std::vector<int>{3};
    auto search = make_search(graph, true, {}, {}, true);
    ASSERT_TRUE(search.start(Node(0), Node(9)));
    expect_status(search, false);
    expect_path(search, {reject_first_tie ? 2 : 1}, reject_first_tie ? 1 : 20);
    EXPECT_EQ(graph->validated, (std::vector<int>{1, 2, 3}));
    auto calls = graph->callback_calls;
    expect_status(search, false);
    EXPECT_EQ(graph->callback_calls, calls);
  }
}

TEST(AStar, LegacyClosedLimitAllowsEqualCountAndStopsOnlyAfterItIsExceeded) {
  for (unsigned limit : {0u, 1u}) {
    auto graph = std::make_shared<Graph>();
    graph->edges = {{1, Node(0), Node(1)}, {1, Node(1), Node(9)}};
    auto search = make_search(graph, false, {}, limit);
    ASSERT_TRUE(search.start(Node(0), Node(9)));
    expect_status(search, std::nullopt, 0);
    EXPECT_TRUE(graph->examined.empty());
    // The historic boundary is closed_count > limit, not >= limit.
    if (limit == 0) {
      expect_status(search, false, 1);
      EXPECT_EQ(graph->expanded, (std::vector<int>{0}));
    } else {
      expect_status(search, std::nullopt, 1);
      expect_status(search, false, 1);
      EXPECT_EQ(graph->expanded, (std::vector<int>{0, 1}));
    }
    expect_status(search, false, 0);
    EXPECT_FALSE(search.result());
  }
  auto graph = std::make_shared<Graph>();
  graph->edges = {{1, Node(0), Node(9)}};
  auto search = make_search(graph, false, {}, 1);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, true);
  expect_path(search, {9}, 1);
}

TEST(AStar, ZeroBudgetDoesNotEvaluateEvenAnImmediatelySuccessfulStart) {
  auto graph = std::make_shared<Graph>();
  graph->goal = 0;
  auto search = make_search(graph);
  ASSERT_TRUE(search.start(Node(0), Node(0)));
  auto calls = graph->callback_calls;
  expect_status(search, std::nullopt, 0);
  expect_status(search, std::nullopt, 0);
  EXPECT_EQ(graph->callback_calls, calls);
  EXPECT_FALSE(search.result());
  expect_status(search, true, 1);
  ASSERT_TRUE(search.result());
  EXPECT_TRUE(search.result()->empty());
  auto extracted = search.extract_result();
  ASSERT_TRUE(extracted);
  EXPECT_TRUE(extracted->empty());
  EXPECT_FALSE(search.result());
  EXPECT_FALSE(search.extract_result());
  calls = graph->callback_calls;
  expect_status(search, true, 0);
  expect_status(search, true);
  EXPECT_EQ(graph->callback_calls, calls);
  graph->goal = 9;
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, false);
  EXPECT_FALSE(search.result());
}

TEST(AStar, RestartAndFindPathReplaceOldGoalsResultsAndActiveFrontiers) {
  auto graph = rediscovery_graph();
  auto search = make_search(graph);
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  expect_status(search, std::nullopt, 1);
  graph->goal = 2;
  ASSERT_TRUE(search.start(Node(0), Node(2)));
  EXPECT_EQ(graph->heuristic_goals.back(), 2);
  EXPECT_FALSE(search.result());
  expect_status(search, true);
  expect_path(search, {2}, 1);
  graph->goal = 1;
  auto found = search.find_path(Node(0), Node(1));
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->get());
  EXPECT_EQ(path_targets(*found->get()), (std::vector<int>{2, 1}));
  EXPECT_EQ(graph->heuristic_goals.back(), 1);
  graph->goal = 99;
  ASSERT_TRUE(search.start(Node(2), Node(99)));
  EXPECT_EQ(graph->heuristic_goals.back(), 99);
  EXPECT_FALSE(search.result());
  expect_status(search, false);
  EXPECT_FALSE(search.result());
  graph->goal = 9;
  ASSERT_TRUE(search.start(Node(0), Node(9)));
  EXPECT_EQ(graph->heuristic_goals.back(), 9);
  expect_status(search, true);
  expect_path(search, {2, 1, 9}, 3);
}

TEST(AStar, ActiveCopiesOwnIndependentFrontiersAndPredecessors) {
  auto graph = rediscovery_graph();
  auto original = make_search(graph);
  ASSERT_TRUE(original.start(Node(0), Node(9)));
  expect_status(original, std::nullopt, 1);
  auto copied = original;
  auto assigned = make_search(std::make_shared<Graph>());
  assigned = original;
  ASSERT_TRUE(copied.start(Node(2), Node(9)));
  expect_status(copied, true);
  expect_path(copied, {1, 9}, 2);
  expect_status(original, true);
  expect_path(original, {2, 1, 9}, 3);
  expect_status(assigned, true);
  expect_path(assigned, {2, 1, 9}, 3);
  ASSERT_TRUE(original.extract_result());
  expect_status(original, true, 0);
  EXPECT_FALSE(original.result());
  expect_path(assigned, {2, 1, 9}, 3);
  expect_path(copied, {1, 9}, 2);
}

TEST(AStar, ActiveMovesAndSelfMoveKeepEarlyAndQueuedStateUsable) {
  auto graph = rediscovery_graph();
  graph->heuristic[0] = 10;// B is an early candidate and A remains queued.
  auto source = make_search(graph);
  ASSERT_TRUE(source.start(Node(0), Node(9)));
  expect_status(source, std::nullopt, 1);
  auto moved = std::move(source);
  EXPECT_FALSE(source.result());
  auto destination = make_search(std::make_shared<Graph>());
  destination = std::move(moved);
  EXPECT_FALSE(moved.result());
  auto& same_search = destination;
  destination = std::move(same_search);
  expect_status(destination, true, 3);
  expect_path(destination, {2, 1, 9}, 3);
  auto completed = std::move(destination);
  EXPECT_FALSE(destination.result());
  expect_path(completed, {2, 1, 9}, 3);
  auto completed_assignment = make_search(std::make_shared<Graph>());
  completed_assignment = std::move(completed);
  EXPECT_FALSE(completed.result());
  expect_path(completed_assignment, {2, 1, 9}, 3);
  auto& same_completed = completed_assignment;
  completed_assignment = std::move(same_completed);
  expect_status(completed_assignment, true, 0);
  expect_path(completed_assignment, {2, 1, 9}, 3);
}

TEST(AStar, CallbacksAndSearchStateOutliveTheirConstructionScope) {
  auto search = [] {
    auto graph = rediscovery_graph();
    auto local = make_search(graph);
    auto started = local.start(Node(0), Node(9));
    EXPECT_TRUE(started);
    expect_status(local, std::nullopt, 1);
    return local;
  }();
  expect_status(search, true);
  expect_path(search, {2, 1, 9}, 3);
}

TEST(AStar, CallbackFailuresKeepTheirTypedPayloadUntilRestart) {
  for (Failure failure :
       {Failure::StartHeuristic, Failure::NeighborHeuristic, Failure::Goal, Failure::Neighbors, Failure::Validator}) {
    SCOPED_TRACE(static_cast<int>(failure));
    auto graph = std::make_shared<Graph>();
    graph->edges = {{1, Node(0), Node(1)}};
    graph->heuristic = {{0, 10}, {1, 1}};
    graph->failure = failure;
    auto search = make_search(graph, true, {}, {}, true);
    auto started = search.start(Node(0), Node(9));
    if (failure == Failure::StartHeuristic) {
      ASSERT_FALSE(started);
      EXPECT_TRUE(started.error().is<OutOfRangeException>());
      EXPECT_EQ(std::string(started.error().what()), std::string(graph->callback_error.what()));
    } else {
      ASSERT_TRUE(started);
    }
    auto failed = search.explore();
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<OutOfRangeException>());
    EXPECT_EQ(std::string(failed.error().what()), std::string(graph->callback_error.what()));
    auto calls = graph->callback_calls;
    graph->failure = Failure::None;
    for (std::optional<unsigned> budget :
         {std::optional<unsigned>{0}, std::optional<unsigned>{1}, std::optional<unsigned>{}}) {
      auto repeated = search.explore(budget);
      ASSERT_FALSE(repeated);
      EXPECT_TRUE(repeated.error().is<OutOfRangeException>());
      EXPECT_EQ(std::string(repeated.error().what()), std::string(graph->callback_error.what()));
    }
    EXPECT_FALSE(search.result());
    EXPECT_FALSE(search.extract_result());
    EXPECT_EQ(graph->callback_calls, calls);
    graph->goal = 1;
    ASSERT_TRUE(search.start(Node(0), Node(1)));
    expect_status(search, true);
    expect_path(search, {1}, 1);
  }
}
