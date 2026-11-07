module;

import std;
import star.optional;
import star.list;
import star.map;
import star.set;
import star.math_common;
import star.block_allocator;
import star.lexical_cast;
import star.format;

namespace Star {
namespace AStar {

  struct Score {
    Score();

    double gScore;
    double hScore = 0;
    double fScore;
  };

  // 'Edge' should be implemented as a class with public fields compatible with
  // these:
  //    double cost;
  //    Node source;
  //    Node target;

  template <class Edge>
  using Path = List<Edge>;

  template <class Edge, class Node>
  class Search {
  public:
    using HeuristicFunction = std::function<std::expected<double, StarException>(Node, Node)>;
    using NeighborFunction = std::function<std::expected<void, StarException>(Node, List<Edge>& neighbors)>;
    using GoalFunction = std::function<std::expected<bool, StarException>(Node)>;
    using CompareFunction = std::function<bool(Node, Node)>;
    using ValidateEndFunction = std::function<std::expected<bool, StarException>(Edge)>;

    Search(HeuristicFunction heuristicCost,
        NeighborFunction getAdjacent,
        GoalFunction goalReached,
        bool returnBestIfFailed = false,
        // In returnBestIfFailed mode, validateEnd checks the end of the path
        // is valid, e.g. not floating in the air.
        std::optional<ValidateEndFunction> validateEnd = {},
        std::optional<double> maxFScore = {},
        std::optional<unsigned> maxNodesToSearch = {});

    Search(Search const&) = default;
    Search& operator=(Search const&) = default;
    Search(Search&& other)
      : m_heuristicCost(std::move(other.m_heuristicCost)),
        m_getAdjacent(std::move(other.m_getAdjacent)),
        m_goalReached(std::move(other.m_goalReached)),
        m_returnBestIfFailed(other.m_returnBestIfFailed),
        m_validateEnd(std::move(other.m_validateEnd)),
        m_maxFScore((other.m_validateEnd.reset(), std::move(other.m_maxFScore))),
        m_maxNodesToSearch((other.m_maxFScore.reset(), std::move(other.m_maxNodesToSearch))),
        m_goal((other.m_maxNodesToSearch.reset(), std::move(other.m_goal))),
        m_nodeMeta(std::move(other.m_nodeMeta)),
        m_openQueue(std::move(other.m_openQueue)),
        m_openSet(std::move(other.m_openSet)),
        m_closedSet(std::move(other.m_closedSet)),
        m_earlyExploration(std::move(other.m_earlyExploration)),
        m_finished((other.m_earlyExploration.reset(), other.m_finished)),
        m_result(std::move(other.m_result)) {
      other.m_result.reset();
    }

    Search& operator=(Search&& other) {
      if (this != &other) {
        m_heuristicCost = std::move(other.m_heuristicCost);
        m_getAdjacent = std::move(other.m_getAdjacent);
        m_goalReached = std::move(other.m_goalReached);
        m_returnBestIfFailed = other.m_returnBestIfFailed;
        m_validateEnd = std::move(other.m_validateEnd);
        other.m_validateEnd.reset();
        m_maxFScore = std::move(other.m_maxFScore);
        other.m_maxFScore.reset();
        m_maxNodesToSearch = std::move(other.m_maxNodesToSearch);
        other.m_maxNodesToSearch.reset();
        m_goal = std::move(other.m_goal);
        m_nodeMeta = std::move(other.m_nodeMeta);
        m_openQueue = std::move(other.m_openQueue);
        m_openSet = std::move(other.m_openSet);
        m_closedSet = std::move(other.m_closedSet);
        m_earlyExploration = std::move(other.m_earlyExploration);
        other.m_earlyExploration.reset();
        m_finished = other.m_finished;
        m_result = std::move(other.m_result);
        other.m_result.reset();
      }
      return *this;
    }

    // Start a new exploration, resets result if it was found before.
    [[nodiscard]] std::expected<void, StarException> start(Node startNode, Node goalNode);
    // Explore the given number of nodes in the search space.  If
    // maxNodesToSearch is reached, or the search space is exhausted, will
    // return
    // false to signal failure.  On success, will return true.  If the given
    // maxExploreNodes is exhausted before success or failure, will return
    // nothing.
    [[nodiscard]] std::expected<std::optional<bool>, StarException> explore(std::optional<unsigned> maxExploreNodes = {});
    // Returns the result if it was found.
    std::optional<Path<Edge>> const& result() const;

    // Convenience, equivalent to calling start, then explore({}) and returns
    // result()
    [[nodiscard]] std::expected<std::reference_wrapper<std::optional<Path<Edge>> const>, StarException> findPath(Node startNode, Node goalNode);

  private:
    struct ScoredNode {
      bool operator<(ScoredNode const& other) const {
        return score.fScore > other.score.fScore;
      }

      Score score;
      Node node;
    };

    struct NodeMeta {
      Score score;
      std::optional<Edge> cameFrom;
    };

    Path<Edge> reconstructPath(Node currentNode);

    HeuristicFunction m_heuristicCost;
    NeighborFunction m_getAdjacent;
    GoalFunction m_goalReached;
    bool m_returnBestIfFailed;
    std::optional<ValidateEndFunction> m_validateEnd;
    std::optional<double> m_maxFScore;
    std::optional<unsigned> m_maxNodesToSearch;

    Node m_goal;
    Map<Node, NodeMeta, std::less<Node>, BlockAllocator<std::pair<Node const, NodeMeta>, 1024>> m_nodeMeta;
    std::priority_queue<ScoredNode> m_openQueue;
    Set<Node, std::less<Node>, BlockAllocator<Node, 1024>> m_openSet;
    Set<Node, std::less<Node>, BlockAllocator<Node, 1024>> m_closedSet;
    std::optional<ScoredNode> m_earlyExploration;

    bool m_finished = false;
    std::optional<Path<Edge>> m_result;
  };

  inline Score::Score() : gScore(std::numeric_limits<double>::max()), fScore(std::numeric_limits<double>::max()) {}

  template <class Edge, class Node>
  Search<Edge, Node>::Search(HeuristicFunction heuristicCost,
      NeighborFunction getAdjacent,
      GoalFunction goalReached,
      bool returnBestIfFailed,
      std::optional<ValidateEndFunction> validateEnd,
      std::optional<double> maxFScore,
      std::optional<unsigned> maxNodesToSearch)
    : m_heuristicCost(heuristicCost),
      m_getAdjacent(getAdjacent),
      m_goalReached(goalReached),
      m_returnBestIfFailed(returnBestIfFailed),
      m_validateEnd(validateEnd),
      m_maxFScore(maxFScore),
      m_maxNodesToSearch(maxNodesToSearch) {}

  template <class Edge, class Node>
  std::expected<void, StarException> Search<Edge, Node>::start(Node startNode, Node goalNode) {
    m_goal = std::move(goalNode);
    m_nodeMeta.clear();
    m_openQueue = std::priority_queue<ScoredNode>();
    m_openSet.clear();
    m_closedSet.clear();
    m_earlyExploration = {};
    m_finished = false;
    m_result.reset();

    Score startScore;
    startScore.gScore = 0;
    if (!m_heuristicCost)
      return std::unexpected(StarException("AStar search requires a heuristic callback"));
    auto heuristicCost = m_heuristicCost(startNode, m_goal);
    if (!heuristicCost)
      return std::unexpected(std::move(heuristicCost.error()));
    startScore.hScore = *heuristicCost;
    startScore.fScore = startScore.hScore;
    m_nodeMeta[startNode].score = startScore;

    m_openSet.insert(startNode);
    m_openQueue.push(ScoredNode{startScore, std::move(startNode)});
    return {};
  }

  template <class Edge, class Node>
  std::expected<std::optional<bool>, StarException> Search<Edge, Node>::explore(std::optional<unsigned> maxExploreNodes) {
    if (m_finished)
      return m_result.has_value();

    List<Edge> neighbors;
    while (true) {
      if ((m_maxNodesToSearch && m_closedSet.size() > *m_maxNodesToSearch)
          || (m_openQueue.empty() && !m_earlyExploration)) {
        m_finished = true;
        // Search failed: retain the closest path only when requested.
        if (m_returnBestIfFailed) {
          double bestScore = std::numeric_limits<double>::max();
          std::optional<Node> bestNode;
          for (Node node : m_closedSet) {
            NodeMeta const& nodeMeta = m_nodeMeta[node];
            if (m_validateEnd && nodeMeta.cameFrom) {
              if (!*m_validateEnd)
                return std::unexpected(StarException("AStar search requires a callable end validator"));
              auto endValidation = (*m_validateEnd)(*nodeMeta.cameFrom);
              if (!endValidation)
                return std::unexpected(std::move(endValidation.error()));
              if (!*endValidation)
                continue;
            }
            if (nodeMeta.score.hScore < bestScore) {
              bestScore = nodeMeta.score.hScore;
              bestNode = node;
            }
          }

          if (bestNode)
            m_result = reconstructPath(*bestNode);
        }

        return false;
      }

      if (maxExploreNodes) {
        if (*maxExploreNodes == 0)
          return std::nullopt;
        --*maxExploreNodes;
      }

      ScoredNode currentScoredNode;
      if (m_earlyExploration) {
        currentScoredNode = std::move(*m_earlyExploration);
        m_earlyExploration.reset();
      } else {
        currentScoredNode = m_openQueue.top();
        m_openQueue.pop();
        if (!m_openSet.remove(currentScoredNode.node))
          // A score update left a duplicate queue entry; this node was searched.
          continue;
      }

      Node const& current = currentScoredNode.node;
      Score const& currentScore = currentScoredNode.score;

      if (!m_goalReached)
        return std::unexpected(StarException("AStar search requires a goal callback"));
      auto goalDecision = m_goalReached(current);
      if (!goalDecision)
        return std::unexpected(std::move(goalDecision.error()));
      if (*goalDecision) {
        m_finished = true;
        m_result = reconstructPath(current);
        return true;
      }

      m_closedSet.insert(current);

      neighbors.clear();
      if (!m_getAdjacent)
        return std::unexpected(StarException("AStar search requires a neighbor callback"));
      auto neighborsRead = m_getAdjacent(current, neighbors);
      if (!neighborsRead)
        return std::unexpected(std::move(neighborsRead.error()));

      for (Edge const& edge : neighbors) {
        if (m_closedSet.find(edge.target) != m_closedSet.end() || m_nodeMeta.contains(edge.target))
          // We've already visited this node.
          continue;

        double newGScore = currentScore.gScore + edge.cost;
        NodeMeta& targetMeta = m_nodeMeta[edge.target];
        Score& targetScore = targetMeta.score;
        if (m_openSet.find(edge.target) == m_openSet.end() || newGScore < targetScore.gScore) {
          targetMeta.cameFrom = edge;
          targetScore.gScore = newGScore;
          if (!m_heuristicCost)
            return std::unexpected(StarException("AStar search requires a heuristic callback"));
          auto heuristicCost = m_heuristicCost(edge.target, m_goal);
          if (!heuristicCost)
            return std::unexpected(std::move(heuristicCost.error()));
          targetScore.hScore = *heuristicCost;
          targetScore.fScore = targetScore.gScore + targetScore.hScore;

          if (m_maxFScore && targetScore.fScore > *m_maxFScore)
            continue;

          // Explore immediately without queueing a node at least as good as
          // the current one.
          if (targetScore.fScore <= currentScore.fScore) {
            if (!m_earlyExploration.has_value()) {
              m_earlyExploration = ScoredNode{targetScore, edge.target};
              continue;
            } else if (m_earlyExploration->score.fScore > targetScore.fScore) {
              m_openSet.insert(m_earlyExploration->node);
              m_openQueue.push(*m_earlyExploration);
              m_earlyExploration = ScoredNode{targetScore, edge.target};
              continue;
            }
          }
          m_openSet.insert(edge.target);
          m_openQueue.push(ScoredNode{targetScore, edge.target});
        }
      }
    }
  }

  template <class Edge, class Node>
  std::optional<Path<Edge>> const& Search<Edge, Node>::result() const {
    return m_result;
  }

  template <class Edge, class Node>
  std::expected<std::reference_wrapper<std::optional<Path<Edge>> const>, StarException> Search<Edge, Node>::findPath(Node startNode, Node goalNode) {
    auto started = start(std::move(startNode), std::move(goalNode));
    if (!started)
      return std::unexpected(std::move(started.error()));
    auto explorationStatus = explore();
    if (!explorationStatus)
      return std::unexpected(std::move(explorationStatus.error()));
    return std::cref(result());
  }

  template <class Edge, class Node>
  Path<Edge> Search<Edge, Node>::reconstructPath(Node currentNode) {
    Path<Edge> path;
    while (m_nodeMeta.find(currentNode) != m_nodeMeta.end()) {
      auto const& incomingEdge = m_nodeMeta[currentNode].cameFrom;
      if (!incomingEdge)
        break;
      path.append(*incomingEdge);
      currentNode = incomingEdge->source;
    }
    std::reverse(path.begin(), path.end());
    return path;
  }
}

}

export module star.a_star;

export namespace Star::AStar {
  using ::Star::AStar::Score;
  using ::Star::AStar::Path;
  using ::Star::AStar::Search;
}
