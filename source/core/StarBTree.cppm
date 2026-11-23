module;
import std;
import star.list;
import star.format;

namespace Star {

  // Storage owns the opaque index/leaf representation and persistence policy.
  // Loads, stores, root changes and leaf deletion preserve their checked errors;
  // index deletion is deliberately infallible.  Next-leaf hooks may return no
  // pointer and do nothing, leaving traversal to the index.
  // Key/data reads may own their result or borrow it from the retained node.
  template <typename Storage>
  concept btree_storage = requires(Storage& storage,
      typename Storage::Pointer pointer, typename Storage::Key key,
      typename Storage::Data data, typename Storage::Index& index,
      typename Storage::Index const& const_index, typename Storage::Leaf& leaf,
      typename Storage::Leaf const& const_leaf, std::size_t i,
      std::optional<typename Storage::Pointer> next) {
    { storage.root_pointer() } -> std::same_as<typename Storage::Pointer>;
    { storage.root_is_leaf() } -> std::same_as<bool>;
    { storage.set_new_root(pointer, true) } -> std::same_as<std::expected<void, StarException>>;
    { storage.create_index(pointer) } -> std::same_as<typename Storage::Index>;
    { storage.load_index(pointer) } -> std::same_as<std::expected<typename Storage::Index, StarException>>;
    { storage.index_pointer_count(const_index) } -> std::same_as<std::size_t>;
    { storage.index_pointer(const_index, i) } -> std::same_as<typename Storage::Pointer>;
    { storage.index_update_pointer(index, i, pointer) } -> std::same_as<void>;
    requires std::same_as<std::remove_cvref_t<decltype(storage.index_key_before(const_index, i))>, typename Storage::Key>;
    { storage.index_update_key_before(index, i, key) } -> std::same_as<void>;
    { storage.index_erase_before(index, i) } -> std::same_as<void>;
    { storage.index_insert_after(index, i, key, pointer) } -> std::same_as<void>;
    { storage.index_level(const_index) } -> std::same_as<std::size_t>;
    { storage.set_index_level(index, i) } -> std::same_as<void>;
    { storage.index_needs_shift(index) } -> std::same_as<bool>;
    { storage.index_shift(index, key, index) } -> std::same_as<bool>;
    { storage.index_split(index) } -> std::same_as<std::optional<std::pair<typename Storage::Key, typename Storage::Index>>>;
    { storage.store_index(std::move(index)) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.delete_index(std::move(index)) } -> std::same_as<void>;
    { storage.create_leaf() } -> std::same_as<typename Storage::Leaf>;
    { storage.load_leaf(pointer) } -> std::same_as<std::expected<typename Storage::Leaf, StarException>>;
    { storage.leaf_element_count(const_leaf) } -> std::same_as<std::size_t>;
    requires std::same_as<std::remove_cvref_t<decltype(storage.leaf_key(const_leaf, i))>, typename Storage::Key>;
    requires std::same_as<std::remove_cvref_t<decltype(storage.leaf_data(const_leaf, i))>, typename Storage::Data>;
    { storage.leaf_insert(leaf, i, key, std::move(data)) } -> std::same_as<void>;
    { storage.leaf_erase(leaf, i) } -> std::same_as<void>;
    { storage.next_leaf(const_leaf) } -> std::same_as<std::optional<typename Storage::Pointer>>;
    { storage.set_next_leaf(leaf, next) } -> std::same_as<void>;
    { storage.set_next_leaf(leaf, pointer) } -> std::same_as<void>;
    { storage.leaf_needs_shift(leaf) } -> std::same_as<bool>;
    { storage.leaf_shift(leaf, leaf) } -> std::same_as<bool>;
    { storage.leaf_split(leaf) } -> std::same_as<std::optional<typename Storage::Leaf>>;
    { storage.store_leaf(std::move(leaf)) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.store_leaf(storage.create_leaf()) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.delete_leaf(std::move(leaf)) } -> std::same_as<std::expected<void, StarException>>;
    { std::as_const(key) < std::as_const(key) } -> std::convertible_to<bool>;
    { pointer != storage.root_pointer() } -> std::convertible_to<bool>;
    { pointer != storage.index_pointer(const_index, i) } -> std::convertible_to<bool>;
  } && std::default_initializable<typename Storage::Key>
      && std::default_initializable<typename Storage::Data>
      && std::default_initializable<typename Storage::Pointer>
      && std::default_initializable<typename Storage::Index>
      && std::default_initializable<typename Storage::Leaf>
      && std::copyable<typename Storage::Key>
      && std::copy_constructible<typename Storage::Data>
      && std::copyable<typename Storage::Pointer>
      && std::copyable<typename Storage::Index>
      && std::copyable<typename Storage::Leaf>
      && (!std::same_as<typename Storage::Index, typename Storage::Leaf>);

  // Callbacks are borrowed and invoked as lvalues, including mutable visitors.
  template <typename Visitor, typename Key, typename Data>
  concept btree_record_visitor = requires(Visitor& visitor, Key const& key, Data const& data) {
    { std::invoke(visitor, key, data) } -> std::same_as<std::expected<void, StarException>>;
  };

  template <typename ErrorHandler>
  concept btree_error_handler = requires(ErrorHandler& error, ExceptionDiagnostic const& diagnostic) {
    { std::invoke(error, std::string_view{}, diagnostic) } -> std::same_as<std::expected<void, StarException>>;
  };

  template <typename Visitor, typename Index, typename Leaf>
  concept btree_node_visitor = requires(Visitor& visitor, Index const& index, Leaf const& leaf) {
    { std::invoke(visitor, index) } -> std::same_as<std::expected<bool, StarException>>;
    { std::invoke(visitor, leaf) } -> std::same_as<std::expected<bool, StarException>>;
  };

  // B+ tree algorithms with an owned storage policy, not a public storage API.
  template <btree_storage Storage>
  class BTree {
    public:
    using Key = typename Storage::Key;
    using Data = typename Storage::Data;
    using Pointer = typename Storage::Pointer;
    using Index = typename Storage::Index;
    using Leaf = typename Storage::Leaf;

    template <typename... Args>
      requires std::constructible_from<Storage, Args...>
    explicit BTree(std::in_place_t, Args&&... args)
      : m_storage(std::forward<Args>(args)...) {}

    [[nodiscard]] std::expected<bool, StarException> contains(Key const& k);

    [[nodiscard]] std::expected<std::optional<Data>, StarException> find(Key const& k);

    // Range is inclusive on lower bound and exclusive on upper bound.
    [[nodiscard]] std::expected<List<std::pair<Key, Data>>, StarException> find(Key const& lower, Key const& upper);

    // Visitor returns expected<void, StarException> as visitor(key, data).
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<void, StarException> for_each(Key const& lower, Key const& upper, Visitor&& visitor);

    // Visitor is called as visitor(key, data).
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<void, StarException> for_all(Visitor&& visitor);

    // Recover all key value pairs possible, reporting node errors during scan and
    // reading as much data as possible.  Visitor is called as visitor(key, data),
    // ErrorHandler returns expected<void, StarException> as
    // error(std::string_view, ExceptionDiagnostic const&).
    template <typename Visitor, typename ErrorHandler>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
          && btree_error_handler<ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recover_all(Visitor&& visitor, ErrorHandler&& error);

    // Visitor is called either as visitor(Index const&) or visitor(Leaf const&).
    // False on an index prunes only its subtree. False on a leaf skips the
    // remaining leaf siblings under its lowest-level parent; ancestor siblings
    // continue. False is not a global stop signal. Errors always propagate.
    template <typename Visitor>
      requires btree_node_visitor<Visitor, typename Storage::Index, typename Storage::Leaf>
    [[nodiscard]] std::expected<void, StarException> for_all_nodes(Visitor&& visitor);

    // returns true if old value overwritten.
    [[nodiscard]] std::expected<bool, StarException> insert(Key k, Data data);

    // returns true if key was found.
    [[nodiscard]] std::expected<bool, StarException> erase(Key k);

    // Removes and returns the key/value pairs in the half-open range.
    // Uses separate mutations for each matching key.
    [[nodiscard]] std::expected<List<std::pair<Key, Data>>, StarException> erase(Key const& lower, Key const& upper);

    [[nodiscard]] std::expected<std::uint64_t, StarException> index_count();
    [[nodiscard]] std::expected<std::uint64_t, StarException> leaf_count();
    [[nodiscard]] std::expected<std::uint64_t, StarException> record_count();

    [[nodiscard]] std::expected<std::uint32_t, StarException> index_levels();

    [[nodiscard]] std::expected<void, StarException> create_new_root();

    private:
    struct DataElement {
      Key key;
      Data data;
    };

    struct DataCollector {
      [[nodiscard]] std::expected<void, StarException> operator()(Key const& k, Data const& d);

      List<std::pair<Key, Data>> list;
    };

    struct RecordCounter {
      [[nodiscard]] std::expected<bool, StarException> operator()(Index const& index);
      [[nodiscard]] std::expected<bool, StarException> operator()(Leaf const& leaf);

      BTree& parent;
      std::uint64_t count;
    };

    struct IndexCounter {
      [[nodiscard]] std::expected<bool, StarException> operator()(Index const& index);
      [[nodiscard]] std::expected<bool, StarException> operator()(Leaf const&);

      BTree& parent;
      std::uint64_t count;
    };

    struct LeafCounter {
      [[nodiscard]] std::expected<bool, StarException> operator()(Index const& index);
      [[nodiscard]] std::expected<bool, StarException> operator()(Leaf const&);

      BTree& parent;
      std::uint64_t count;
    };

    enum ModifyAction {
      InsertAction,
      EraseAction
    };

    enum ModifyState {
      LeafNeedsJoin,
      IndexNeedsJoin,
      LeafSplit,
      IndexSplit,
      LeafNeedsUpdate,
      IndexNeedsUpdate,
      Done
    };

    struct ModifyInfo {
      ModifyInfo(ModifyAction a, DataElement e);

      DataElement target_element;
      ModifyAction action;
      bool found;
      ModifyState state;

      Key new_key;
      Pointer new_pointer;
    };

    [[nodiscard]] std::expected<bool, StarException> contains(Index const& index, Key const& k);
    [[nodiscard]] std::expected<bool, StarException> contains(Leaf const& leaf, Key const& k);

    [[nodiscard]] std::expected<std::optional<Data>, StarException> find(Index const& index, Key const& k);
    [[nodiscard]] std::expected<std::optional<Data>, StarException> find(Leaf const& leaf, Key const& k);

    // Returns the highest key for the last leaf we have searched
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<Key, StarException> for_each(Index const& index, Key const& lower, Key const& upper, Visitor&& o);
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<Key, StarException> for_each(Leaf const& leaf, Key const& lower, Key const& upper, Visitor&& o);

    // Returns the highest key for the last leaf we have searched
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<Key, StarException> for_all(Index const& index, Visitor&& o);
    template <typename Visitor>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
    [[nodiscard]] std::expected<Key, StarException> for_all(Leaf const& leaf, Visitor&& o);

    template <typename Visitor, typename ErrorHandler>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
          && btree_error_handler<ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recover_all(Index const& index, Visitor&& o, ErrorHandler&& error);
    template <typename Visitor, typename ErrorHandler>
      requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
          && btree_error_handler<ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recover_all(Leaf const& leaf, Visitor&& o, ErrorHandler&& error);

    // Variable size values mean that merges can happen on inserts, so can't
    // split up into insert / erase methods
    [[nodiscard]] std::expected<void, StarException> modify(Leaf& leaf_node, ModifyInfo& info);
    [[nodiscard]] std::expected<void, StarException> modify(Index& index_node, ModifyInfo& info);
    [[nodiscard]] std::expected<bool, StarException> modify(DataElement e, ModifyAction action);

    // Traverses Indexes down the tree on the left side to get the least valued
    // key that is pointed to by any leaf under this index.  Needed when joining.
    [[nodiscard]] std::expected<Key, StarException> leftmost_key(Index const& index);

    template <typename Visitor>
      requires btree_node_visitor<Visitor, typename Storage::Index, typename Storage::Leaf>
    [[nodiscard]] std::expected<void, StarException> for_all_nodes(Index const& index, Visitor&& visitor);

    std::pair<std::size_t, bool> leaf_lower_bound(Leaf const& leaf, Key const& key);
    std::size_t index_upper_bound(Index const& index, Key const& key);

    Storage m_storage;
  };

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Key const& k) {
    if (m_storage.root_is_leaf()) {
      auto leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return contains(*leaf, k);
    } else {
      auto index = m_storage.load_index(m_storage.root_pointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return contains(*index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Key const& k) -> std::expected<std::optional<Data>, StarException> {
    if (m_storage.root_is_leaf()) {
      auto leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return find(*leaf, k);
    } else {
      auto index = m_storage.load_index(m_storage.root_pointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return find(*index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Key const& lower, Key const& upper) -> std::expected<List<std::pair<Key, Data>>, StarException> {
    DataCollector collector;
    auto visited = for_each(lower, upper, collector);
    if (!visited) return std::unexpected(std::move(visited.error()));

    return std::move(collector.list);
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::for_each(Key const& lower, Key const& upper, Visitor&& visitor) {
    if (m_storage.root_is_leaf()) {
      auto root_leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!root_leaf) return std::unexpected(std::move(root_leaf.error()));
      auto visited = for_each((*root_leaf), lower, upper, visitor);
      if (!visited) return std::unexpected(std::move(visited.error()));

    } else {
      auto root_index = m_storage.load_index(m_storage.root_pointer());
      if (!root_index) return std::unexpected(std::move(root_index.error()));
      auto visited = for_each((*root_index), lower, upper, visitor);
      if (!visited) return std::unexpected(std::move(visited.error()));

    }
    return {};
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::for_all(Visitor&& visitor) {
    if (m_storage.root_is_leaf()) {
      auto root_leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!root_leaf) return std::unexpected(std::move(root_leaf.error()));
      auto visited = for_all((*root_leaf), visitor);
      if (!visited) return std::unexpected(std::move(visited.error()));

    } else {
      auto root_index = m_storage.load_index(m_storage.root_pointer());
      if (!root_index) return std::unexpected(std::move(root_index.error()));
      auto visited = for_all((*root_index), visitor);
      if (!visited) return std::unexpected(std::move(visited.error()));

    }
    return {};
  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
        && btree_error_handler<ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recover_all(Visitor&& visitor, ErrorHandler&& error) {
    if (m_storage.root_is_leaf()) {
      auto node = m_storage.load_leaf(m_storage.root_pointer());
      if (!node) {
        ExceptionDiagnostic const diagnostic(node.error());
        return std::invoke(error, std::string_view("Error loading root index or leaf node"), diagnostic);
      }
      return recover_all(*node, visitor, error);
    }
    auto node = m_storage.load_index(m_storage.root_pointer());
    if (!node) {
      ExceptionDiagnostic const diagnostic(node.error());
      return std::invoke(error, std::string_view("Error loading root index or leaf node"), diagnostic);
    }
    return recover_all(*node, visitor, error);
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_node_visitor<Visitor, typename Storage::Index, typename Storage::Leaf>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::for_all_nodes(Visitor&& visitor) {
    if (m_storage.root_is_leaf()) {
      auto leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      if (auto visited = std::invoke(visitor, std::as_const(*leaf)); !visited)
        return std::unexpected(std::move(visited.error()));
    } else {
      auto index = m_storage.load_index(m_storage.root_pointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return for_all_nodes(*index, visitor);
    }
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::insert(Key k, Data data) {
    return modify(DataElement{std::move(k), std::move(data)}, InsertAction);
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::erase(Key k) {
    return modify(DataElement{std::move(k), Data()}, EraseAction);
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::erase(Key const& lower, Key const& upper) -> std::expected<List<std::pair<Key, Data>>, StarException> {
    DataCollector collector;
    auto visited = for_each(lower, upper, collector);
    if (!visited) return std::unexpected(std::move(visited.error()));

    for (auto const& elem : collector.list) {
      auto erased = erase(elem.first);
      if (!erased) return std::unexpected(std::move(erased.error()));

    }

    return std::move(collector.list);
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::index_count() {
    IndexCounter counter = {*this, 0};
    auto visited = for_all_nodes(counter);
    if (!visited) return std::unexpected(std::move(visited.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::leaf_count() {
    LeafCounter counter = {*this, 0};
    auto visited = for_all_nodes(counter);
    if (!visited) return std::unexpected(std::move(visited.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::record_count() {
    RecordCounter counter = {*this, 0};
    auto visited = for_all_nodes(counter);
    if (!visited) return std::unexpected(std::move(visited.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint32_t, StarException> BTree<Storage>::index_levels() {
    if (m_storage.root_is_leaf()) {
      return 0;
    } else {
      auto root_index = m_storage.load_index(m_storage.root_pointer());
      if (!root_index) return std::unexpected(std::move(root_index.error()));
      return m_storage.index_level((*root_index)) + 1;
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::create_new_root() {
    auto root_pointer = m_storage.store_leaf(m_storage.create_leaf());
    if (!root_pointer) return std::unexpected(std::move(root_pointer.error()));
    return m_storage.set_new_root(*root_pointer, true);
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::DataCollector::operator()(Key const& k, Data const& d) {
    list.push_back({k, d});
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::RecordCounter::operator()(Index const&) {
    return true;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::RecordCounter::operator()(Leaf const& leaf) {
    count += parent.m_storage.leaf_element_count(leaf);
    return true;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::IndexCounter::operator()(Index const& index) {
    ++count;
    if (parent.m_storage.index_level(index) == 0) {
      return false;
    } else {
      return true;
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::IndexCounter::operator()(Leaf const&) {
    return false;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::LeafCounter::operator()(Index const& index) {
    if (parent.m_storage.index_level(index) == 0) {

      count += parent.m_storage.index_pointer_count(index);
      return false;
    } else {

      return true;
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::LeafCounter::operator()(Leaf const&) {
    ++count;
    return false;
  }

  template <btree_storage Storage>
  BTree<Storage>::ModifyInfo::ModifyInfo(ModifyAction a, DataElement e)
  : target_element(std::move(e)), action(a) {
    found = false;
    state = Done;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Index const& index, Key const& k) {
    std::size_t i = index_upper_bound(index, k);
    if (m_storage.index_level(index) == 0) {
      auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return contains(*leaf, k);
    } else {
      auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
      if (!child_index) return std::unexpected(std::move(child_index.error()));
      return contains(*child_index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Leaf const& leaf, Key const& k) {
    return leaf_lower_bound(leaf, k).second;
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Index const& index, Key const& k) -> std::expected<std::optional<Data>, StarException> {
    std::size_t i = index_upper_bound(index, k);
    if (m_storage.index_level(index) == 0) {
      auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return find(*leaf, k);
    } else {
      auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
      if (!child_index) return std::unexpected(std::move(child_index.error()));
      return find(*child_index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Leaf const& leaf, Key const& k) -> std::expected<std::optional<Data>, StarException> {
    std::pair<std::size_t, bool> res = leaf_lower_bound(leaf, k);
    if (res.second) {
      return std::optional<Data>(std::in_place, m_storage.leaf_data(leaf, res.first));
    } else {
      return {};
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] auto BTree<Storage>::for_each(Index const& index, Key const& lower, Key const& upper, Visitor&& o) -> std::expected<Key, StarException> {
    std::size_t i = index_upper_bound(index, lower);
    Key last_key;

    if (m_storage.index_level(index) == 0) {
      auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      auto visited = for_each((*leaf), lower, upper, o);
      if (!visited) return std::unexpected(std::move(visited.error()));
      last_key = std::move(*visited);
    } else {
      auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
      if (!child_index) return std::unexpected(std::move(child_index.error()));
      auto visited = for_each((*child_index), lower, upper, o);
      if (!visited) return std::unexpected(std::move(visited.error()));
      last_key = std::move(*visited);
    }

    if (!(last_key < upper)) {
      return last_key;
    }

    while (i < m_storage.index_pointer_count(index) - 1) {

      ++i;

      // We're visiting the right side of the key, so if last_key >=
      // index_key_before(index, i), we have already visited this node via next_leaf
      // pointers, so skip it.
      if (!(last_key < m_storage.index_key_before(index, i))) {
        continue;
      }

      if (m_storage.index_level(index) == 0) {
        auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
        if (!leaf) return std::unexpected(std::move(leaf.error()));
        auto visited = for_each((*leaf), lower, upper, o);
        if (!visited) return std::unexpected(std::move(visited.error()));
        last_key = std::move(*visited);
      } else {
        auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
        if (!child_index) return std::unexpected(std::move(child_index.error()));
        auto visited = for_each((*child_index), lower, upper, o);
        if (!visited) return std::unexpected(std::move(visited.error()));
        last_key = std::move(*visited);
      }

      if (!(last_key < upper)) {
        break;
      }
    }

    return last_key;
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] auto BTree<Storage>::for_each(Leaf const& leaf, Key const& lower, Key const& upper, Visitor&& o) -> std::expected<Key, StarException> {
    if (m_storage.leaf_element_count(leaf) == 0) {
      return Key();
    }

    std::size_t lower_index = leaf_lower_bound(leaf, lower).first;

    for (std::size_t i = lower_index; i != m_storage.leaf_element_count(leaf); ++i) {

      decltype(auto) current_key = m_storage.leaf_key(leaf, i);
      auto const& key = current_key;
      if (!(key < lower)) {

        if (key < upper) {
          auto const& data = m_storage.leaf_data(leaf, i);
          if (auto visited = std::invoke(o, key, data); !visited)
            return std::unexpected(std::move(visited.error()));
        } else {
          return Key(std::forward<decltype(current_key)>(current_key));
        }
      }
    }

    if (auto next_leaf_pointer = m_storage.next_leaf(leaf)) {
      auto next_leaf = m_storage.load_leaf(*next_leaf_pointer);
      if (!next_leaf) return std::unexpected(std::move(next_leaf.error()));
      return for_each(*next_leaf, lower, upper, o);
    } else {
      return m_storage.leaf_key(leaf, m_storage.leaf_element_count(leaf) - 1);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] auto BTree<Storage>::for_all(Index const& index, Visitor&& o) -> std::expected<Key, StarException> {
    Key last_key;
    for (std::size_t i = 0; i < m_storage.index_pointer_count(index); ++i) {

      // If we're to the right of a given key, but last_key >= this key, then we
      // must have already visited this node via next_leaf pointers, so we can
      // skip it.
      if (i > 0 && !(last_key < m_storage.index_key_before(index, i))) {
        continue;
      }

      if (m_storage.index_level(index) == 0) {
        auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
        if (!leaf) return std::unexpected(std::move(leaf.error()));
        auto visited = for_all((*leaf), o);
        if (!visited) return std::unexpected(std::move(visited.error()));
        last_key = std::move(*visited);
      } else {
        auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
        if (!child_index) return std::unexpected(std::move(child_index.error()));
        auto visited = for_all((*child_index), o);
        if (!visited) return std::unexpected(std::move(visited.error()));
        last_key = std::move(*visited);
      }
    }

    return last_key;
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
  [[nodiscard]] auto BTree<Storage>::for_all(Leaf const& leaf, Visitor&& o) -> std::expected<Key, StarException> {
    if (m_storage.leaf_element_count(leaf) == 0) {
      return Key();
    }

    for (std::size_t i = 0; i != m_storage.leaf_element_count(leaf); ++i) {

      auto const& key = m_storage.leaf_key(leaf, i);
      auto const& data = m_storage.leaf_data(leaf, i);
      if (auto visited = std::invoke(o, key, data); !visited)
        return std::unexpected(std::move(visited.error()));
    }

    if (auto next_leaf_pointer = m_storage.next_leaf(leaf)) {
      auto next_leaf = m_storage.load_leaf(*next_leaf_pointer);
      if (!next_leaf) return std::unexpected(std::move(next_leaf.error()));
      return for_all(*next_leaf, o);
    } else {
      return m_storage.leaf_key(leaf, m_storage.leaf_element_count(leaf) - 1);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
        && btree_error_handler<ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recover_all(Index const& index, Visitor&& visitor, ErrorHandler&& error) {
    for (std::size_t i = 0; i < m_storage.index_pointer_count(index); ++i) {
      if (m_storage.index_level(index) == 0) {
        auto node = m_storage.load_leaf(m_storage.index_pointer(index, i));
        if (!node) {
          ExceptionDiagnostic const diagnostic(node.error());
          auto reported = std::invoke(error, std::string_view("Error loading leaf node"), diagnostic);
          if (!reported) return std::unexpected(std::move(reported.error()));
          continue;
        }
        auto recovered = recover_all(*node, visitor, error);
        if (!recovered) return std::unexpected(std::move(recovered.error()));
      } else {
        auto node = m_storage.load_index(m_storage.index_pointer(index, i));
        if (!node) {
          ExceptionDiagnostic const diagnostic(node.error());
          auto reported = std::invoke(error, std::string_view("Error loading index node"), diagnostic);
          if (!reported) return std::unexpected(std::move(reported.error()));
          continue;
        }
        auto recovered = recover_all(*node, visitor, error);
        if (!recovered) return std::unexpected(std::move(recovered.error()));
      }
    }
    return {};

  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
    requires btree_record_visitor<Visitor, typename Storage::Key, typename Storage::Data>
        && btree_error_handler<ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recover_all(Leaf const& leaf, Visitor&& visitor, ErrorHandler&& error) {
    for (std::size_t i = 0; i != m_storage.leaf_element_count(leaf); ++i) {
      auto const& key = m_storage.leaf_key(leaf, i);
      auto const& data = m_storage.leaf_data(leaf, i);
      auto visited = std::invoke(visitor, key, data);
      if (!visited) {
        ExceptionDiagnostic const diagnostic(visited.error());
        return std::invoke(error, std::string_view("Error reading leaf node"), diagnostic);
      }
    }
    return {};

  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::modify(Leaf& leaf_node, ModifyInfo& info) {
    info.state = Done;

    std::pair<std::size_t, bool> res = leaf_lower_bound(leaf_node, info.target_element.key);
    std::size_t i = res.first;
    if (res.second) {

      info.found = true;
      m_storage.leaf_erase(leaf_node, i);
    }

    // No change necessary.
    if (info.action == EraseAction && !info.found) {
      return {};
    }

    if (info.action == InsertAction) {
      m_storage.leaf_insert(leaf_node, i, info.target_element.key, std::move(info.target_element.data));
    }

    auto split_result = m_storage.leaf_split(leaf_node);
    if (split_result) {

      m_storage.set_next_leaf(*split_result, m_storage.next_leaf(leaf_node));
      info.new_key = m_storage.leaf_key(*split_result, 0);
      auto stored_leaf = m_storage.store_leaf(std::move(*split_result));
      if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
      info.new_pointer = std::move(*stored_leaf);

      m_storage.set_next_leaf(leaf_node, info.new_pointer);
      info.state = LeafSplit;
    } else {
      if (m_storage.leaf_needs_shift(leaf_node)) {

        info.state = LeafNeedsJoin;
      } else {

        info.state = LeafNeedsUpdate;
      }
    }
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::modify(Index& index_node, ModifyInfo& info) {
    std::size_t i = index_upper_bound(index_node, info.target_element.key);
    Pointer next_pointer = m_storage.index_pointer(index_node, i);

    Leaf lower_leaf;
    Index lower_index;
    if (m_storage.index_level(index_node) == 0) {

      auto loaded_leaf = m_storage.load_leaf(next_pointer);
      if (!loaded_leaf) return std::unexpected(std::move(loaded_leaf.error()));
      lower_leaf = std::move(*loaded_leaf);
      auto modified = modify(lower_leaf, info);
      if (!modified) return std::unexpected(std::move(modified.error()));

    } else {

      auto loaded_index = m_storage.load_index(next_pointer);
      if (!loaded_index) return std::unexpected(std::move(loaded_index.error()));
      lower_index = std::move(*loaded_index);
      auto modified = modify(lower_index, info);
      if (!modified) return std::unexpected(std::move(modified.error()));

    }

    if (info.state == Done) {
      return {};
    }

    bool self_updated = false;

    std::size_t left = 0;
    std::size_t right = 0;
    if (i != 0 && i == m_storage.index_pointer_count(index_node) - 1) {

      left = i - 1;
      right = i;
    } else {

      left = i;
      right = i + 1;
    }

    if (info.state == LeafNeedsJoin) {

      if (m_storage.index_pointer_count(index_node) < 2) {

        // Don't have enough leaves to join, just do the pending update.
        info.state = LeafNeedsUpdate;
      } else {

        Leaf left_leaf;
        Leaf right_leaf;

        if (left == i) {

          left_leaf = lower_leaf;
          auto loaded_right_leaf = m_storage.load_leaf(m_storage.index_pointer(index_node, right));
          if (!loaded_right_leaf) return std::unexpected(std::move(loaded_right_leaf.error()));
          right_leaf = std::move(*loaded_right_leaf);
        } else {

          auto loaded_left_leaf = m_storage.load_leaf(m_storage.index_pointer(index_node, left));
          if (!loaded_left_leaf) return std::unexpected(std::move(loaded_left_leaf.error()));
          left_leaf = std::move(*loaded_left_leaf);
          right_leaf = lower_leaf;
        }

        if (!m_storage.leaf_shift(left_leaf, right_leaf)) {

          // Leaves not modified, just do the pending update.
          info.state = LeafNeedsUpdate;
        } else {
          if (m_storage.leaf_element_count(right_leaf) == 0) {

            // Leaves merged.
            m_storage.set_next_leaf(left_leaf, m_storage.next_leaf(right_leaf));
            auto deleted_leaf = m_storage.delete_leaf(std::move(right_leaf));
            if (!deleted_leaf) return std::unexpected(std::move(deleted_leaf.error()));

            // Replace two sibling pointer elements with one pointing to merged
            // leaf.
            if (left != 0) {
              m_storage.index_update_key_before(index_node, left, m_storage.leaf_key(left_leaf, 0));
            }

            auto stored_left_leaf = m_storage.store_leaf(std::move(left_leaf));
            if (!stored_left_leaf) return std::unexpected(std::move(stored_left_leaf.error()));
            m_storage.index_update_pointer(index_node, left, (*stored_left_leaf));
            m_storage.index_erase_before(index_node, right);

            self_updated = true;
          } else {

            // Leaves shifted.
            auto stored_left_leaf = m_storage.store_leaf(std::move(left_leaf));
            if (!stored_left_leaf) return std::unexpected(std::move(stored_left_leaf.error()));
            m_storage.index_update_pointer(index_node, left, (*stored_left_leaf));

            // Right leaf first key changes on shift, so always need to update
            // left index node.
            m_storage.index_update_key_before(index_node, right, m_storage.leaf_key(right_leaf, 0));

            auto stored_right_leaf = m_storage.store_leaf(std::move(right_leaf));
            if (!stored_right_leaf) return std::unexpected(std::move(stored_right_leaf.error()));
            m_storage.index_update_pointer(index_node, right, (*stored_right_leaf));

            self_updated = true;
          }
        }
      }
    }

    if (info.state == IndexNeedsJoin) {

      if (m_storage.index_pointer_count(index_node) < 2) {

        // Don't have enough indexes to join, just do the pending update.
        info.state = IndexNeedsUpdate;
      } else {

        Index left_index;
        Index right_index;

        if (left == i) {

          left_index = lower_index;
          auto loaded_right_index = m_storage.load_index(m_storage.index_pointer(index_node, right));
          if (!loaded_right_index) return std::unexpected(std::move(loaded_right_index.error()));
          right_index = std::move(*loaded_right_index);
        } else {

          auto loaded_left_index = m_storage.load_index(m_storage.index_pointer(index_node, left));
          if (!loaded_left_index) return std::unexpected(std::move(loaded_left_index.error()));
          left_index = std::move(*loaded_left_index);
          right_index = lower_index;
        }

        auto right_key = leftmost_key(right_index);
        if (!right_key) return std::unexpected(std::move(right_key.error()));
        if (!m_storage.index_shift(left_index, (*right_key), right_index)) {

          // Indexes not modified, just do the pending update.
          info.state = IndexNeedsUpdate;
        } else {
          if (m_storage.index_pointer_count(right_index) == 0) {

            // Indexes merged.
            m_storage.delete_index(std::move(right_index));

            // Replace two sibling pointer elements with one pointing to merged
            // index.
            if (left != 0) {
              auto left_key = leftmost_key(left_index);
              if (!left_key) return std::unexpected(std::move(left_key.error()));
              m_storage.index_update_key_before(index_node, left, (*left_key));
            }

            auto stored_left_index = m_storage.store_index(std::move(left_index));
            if (!stored_left_index) return std::unexpected(std::move(stored_left_index.error()));
            m_storage.index_update_pointer(index_node, left, (*stored_left_index));
            m_storage.index_erase_before(index_node, right);

            self_updated = true;
          } else {

            // Indexes shifted.
            auto stored_left_index = m_storage.store_index(std::move(left_index));
            if (!stored_left_index) return std::unexpected(std::move(stored_left_index.error()));
            m_storage.index_update_pointer(index_node, left, (*stored_left_index));

            // Right index first key changes on shift, so always need to update
            // right index node.
            auto right_key = leftmost_key(right_index);
            if (!right_key) return std::unexpected(std::move(right_key.error()));
            Key key_for_right = std::move(*right_key);
            auto stored_right_index = m_storage.store_index(std::move(right_index));
            if (!stored_right_index) return std::unexpected(std::move(stored_right_index.error()));
            m_storage.index_update_pointer(index_node, right, (*stored_right_index));
            m_storage.index_update_key_before(index_node, right, key_for_right);

            self_updated = true;
          }
        }
      }
    }

    if (info.state == LeafSplit) {

      auto stored_leaf = m_storage.store_leaf(std::move(lower_leaf));
      if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
      m_storage.index_update_pointer(index_node, i, (*stored_leaf));
      m_storage.index_insert_after(index_node, i, info.new_key, info.new_pointer);
      self_updated = true;
    }

    if (info.state == IndexSplit) {

      auto stored_index = m_storage.store_index(std::move(lower_index));
      if (!stored_index) return std::unexpected(std::move(stored_index.error()));
      m_storage.index_update_pointer(index_node, i, (*stored_index));
      m_storage.index_insert_after(index_node, i, info.new_key, info.new_pointer);
      self_updated = true;
    }

    if (info.state == LeafNeedsUpdate) {

      auto stored_leaf = m_storage.store_leaf(std::move(lower_leaf));
      if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
      Pointer lower_leaf_pointer = std::move(*stored_leaf);
      if (lower_leaf_pointer != m_storage.index_pointer(index_node, i)) {

        m_storage.index_update_pointer(index_node, i, lower_leaf_pointer);
        self_updated = true;
      }
    }

    if (info.state == IndexNeedsUpdate) {

      auto stored_index = m_storage.store_index(std::move(lower_index));
      if (!stored_index) return std::unexpected(std::move(stored_index.error()));
      Pointer lower_index_pointer = std::move(*stored_index);
      if (lower_index_pointer != m_storage.index_pointer(index_node, i)) {

        m_storage.index_update_pointer(index_node, i, lower_index_pointer);
        self_updated = true;
      }
    }

    auto split_result = m_storage.index_split(index_node);
    if (split_result) {

      info.new_key = split_result->first;
      auto stored_index = m_storage.store_index(std::move(*split_result).second);
      if (!stored_index) return std::unexpected(std::move(stored_index.error()));
      info.new_pointer = std::move(*stored_index);
      info.state = IndexSplit;
      self_updated = true;
    } else {
      if (m_storage.index_needs_shift(index_node)) {

        info.state = IndexNeedsJoin;
      } else {
        if (self_updated) {

          info.state = IndexNeedsUpdate;
        } else {

          info.state = Done;
        }
      }
    }
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::modify(DataElement e, ModifyAction action) {
    ModifyInfo info(action, std::move(e));

    Leaf lower_leaf;
    Index lower_index;
    if (m_storage.root_is_leaf()) {

      auto loaded_leaf = m_storage.load_leaf(m_storage.root_pointer());
      if (!loaded_leaf) return std::unexpected(std::move(loaded_leaf.error()));
      lower_leaf = std::move(*loaded_leaf);
      auto modified = modify(lower_leaf, info);
      if (!modified) return std::unexpected(std::move(modified.error()));

    } else {

      auto loaded_index = m_storage.load_index(m_storage.root_pointer());
      if (!loaded_index) return std::unexpected(std::move(loaded_index.error()));
      lower_index = std::move(*loaded_index);
      auto modified = modify(lower_index, info);
      if (!modified) return std::unexpected(std::move(modified.error()));

    }

    if (info.state == IndexNeedsJoin) {

      if (m_storage.index_pointer_count(lower_index) == 1) {

        // If root index has single pointer, then make that the new root.

        // release index first (to support the common use case of delaying
        // removes until set_new_root)
        Pointer pointer = m_storage.index_pointer(lower_index, 0);
        std::size_t level = m_storage.index_level(lower_index);
        m_storage.delete_index(std::move(lower_index));
        auto root_changed = m_storage.set_new_root(pointer, level == 0);
        if (!root_changed) return std::unexpected(std::move(root_changed.error()));

      } else {

        // Else just update.
        info.state = IndexNeedsUpdate;
      }
    }

    if (info.state == LeafNeedsJoin) {

      // Ignore NeedsJoin on LeafNode root, just update.
      info.state = LeafNeedsUpdate;
    }

    if (info.state == LeafSplit || info.state == IndexSplit) {

      Index new_root;
      if (info.state == IndexSplit) {

        auto root_index_level = m_storage.index_level(lower_index) + 1;
        auto stored_index = m_storage.store_index(std::move(lower_index));
        if (!stored_index) return std::unexpected(std::move(stored_index.error()));
        new_root = m_storage.create_index((*stored_index));
        m_storage.set_index_level(new_root, root_index_level);
      } else {

        auto stored_leaf = m_storage.store_leaf(std::move(lower_leaf));
        if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
        new_root = m_storage.create_index((*stored_leaf));
        m_storage.set_index_level(new_root, 0);
      }
      m_storage.index_insert_after(new_root, 0, info.new_key, info.new_pointer);
      auto stored_root = m_storage.store_index(std::move(new_root));
      if (!stored_root) return std::unexpected(std::move(stored_root.error()));
      auto root_changed = m_storage.set_new_root((*stored_root), false);
      if (!root_changed) return std::unexpected(std::move(root_changed.error()));

    }

    if (info.state == IndexNeedsUpdate) {

      auto stored_index = m_storage.store_index(std::move(lower_index));
      if (!stored_index) return std::unexpected(std::move(stored_index.error()));
      Pointer new_root_pointer = std::move(*stored_index);
      if (new_root_pointer != m_storage.root_pointer()) {
        auto root_changed = m_storage.set_new_root(new_root_pointer, false);
        if (!root_changed) return std::unexpected(std::move(root_changed.error()));

      }
    }

    if (info.state == LeafNeedsUpdate) {

      auto stored_leaf = m_storage.store_leaf(std::move(lower_leaf));
      if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
      Pointer new_root_pointer = std::move(*stored_leaf);
      if (new_root_pointer != m_storage.root_pointer()) {
        auto root_changed = m_storage.set_new_root(new_root_pointer, true);
        if (!root_changed) return std::unexpected(std::move(root_changed.error()));

      }
    }

    return info.found;
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::leftmost_key(Index const& index) -> std::expected<Key, StarException> {
    if (m_storage.index_level(index) == 0) {

      auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, 0));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return m_storage.leaf_key(*leaf, 0);
    } else {

      auto child_index = m_storage.load_index(m_storage.index_pointer(index, 0));
      if (!child_index) return std::unexpected(std::move(child_index.error()));
      return leftmost_key(*child_index);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
    requires btree_node_visitor<Visitor, typename Storage::Index, typename Storage::Leaf>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::for_all_nodes(Index const& index, Visitor&& visitor) {
    auto visited = std::invoke(visitor, index);
    if (!visited) return std::unexpected(std::move(visited.error()));
    if (!(*visited)) {
      return {};
    }

    for (std::size_t i = 0; i < m_storage.index_pointer_count(index); ++i) {

      if (m_storage.index_level(index) != 0) {

        auto child_index = m_storage.load_index(m_storage.index_pointer(index, i));
        if (!child_index) return std::unexpected(std::move(child_index.error()));
        auto visited_child = for_all_nodes((*child_index), visitor);
        if (!visited_child) return std::unexpected(std::move(visited_child.error()));

      } else {

        auto leaf = m_storage.load_leaf(m_storage.index_pointer(index, i));
        if (!leaf) return std::unexpected(std::move(leaf.error()));
        auto visited_leaf = std::invoke(visitor, std::as_const(*leaf));
        if (!visited_leaf) return std::unexpected(std::move(visited_leaf.error()));
        if (!(*visited_leaf)) {
          return {};
        }
      }
    }
    return {};
  }

  template <btree_storage Storage>
  std::pair<std::size_t, bool> BTree<Storage>::leaf_lower_bound(Leaf const& leaf, Key const& key) {
    auto const size = m_storage.leaf_element_count(leaf);
    auto indices = std::views::iota(std::size_t{0}, size);
    auto found = std::ranges::lower_bound(indices, key,
        [](Key const& left, Key const& right) { return left < right; },
        [&](std::size_t i) -> decltype(auto) { return m_storage.leaf_key(leaf, i); });
    auto const position = static_cast<std::size_t>(found - indices.begin());
    return {position, position < size && !(key < m_storage.leaf_key(leaf, position))};
  }

  template <btree_storage Storage>
  std::size_t BTree<Storage>::index_upper_bound(Index const& index, Key const& key) {
    auto const size = m_storage.index_pointer_count(index);
    if (size == 0)
      return 0;

    // Explicit < preserves the storage contract without totally_ordered keys.
    auto indices = std::views::iota(std::size_t{1}, size);
    auto found = std::ranges::upper_bound(indices, key,
        [](Key const& left, Key const& right) { return left < right; },
        [&](std::size_t i) -> decltype(auto) { return m_storage.index_key_before(index, i); });
    return static_cast<std::size_t>(found - indices.begin());
  }

}

export module star.btree;

export namespace Star {
  using ::Star::BTree;
  using ::Star::btree_storage;
  using ::Star::btree_record_visitor;
  using ::Star::btree_error_handler;
  using ::Star::btree_node_visitor;
}
