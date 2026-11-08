module;
import std;
import star.list;
import star.format;

namespace Star {

  // Storage owns the opaque index/leaf representation and persistence policy.
  // Loads, stores, root changes and leaf deletion preserve their checked errors;
  // index deletion is deliberately infallible.  Next-leaf hooks may return no
  // pointer and do nothing, leaving traversal to the index.
  template <typename Storage>
  concept btree_storage = requires(Storage& storage,
      typename Storage::Pointer pointer, typename Storage::Key key,
      typename Storage::Data data, typename Storage::Index& index,
      typename Storage::Index const& constIndex, typename Storage::Leaf& leaf,
      typename Storage::Leaf const& constLeaf, std::size_t i,
      std::optional<typename Storage::Pointer> next) {
    { storage.rootPointer() } -> std::same_as<typename Storage::Pointer>;
    { storage.rootIsLeaf() } -> std::same_as<bool>;
    { storage.setNewRoot(pointer, true) } -> std::same_as<std::expected<void, StarException>>;
    { storage.createIndex(pointer) } -> std::same_as<typename Storage::Index>;
    { storage.loadIndex(pointer) } -> std::same_as<std::expected<typename Storage::Index, StarException>>;
    { storage.indexPointerCount(constIndex) } -> std::same_as<std::size_t>;
    { storage.indexPointer(constIndex, i) } -> std::same_as<typename Storage::Pointer>;
    { storage.indexUpdatePointer(index, i, pointer) } -> std::same_as<void>;
    { storage.indexKeyBefore(constIndex, i) } -> std::same_as<typename Storage::Key>;
    { storage.indexUpdateKeyBefore(index, i, key) } -> std::same_as<void>;
    { storage.indexRemoveBefore(index, i) } -> std::same_as<void>;
    { storage.indexInsertAfter(index, i, key, pointer) } -> std::same_as<void>;
    { storage.indexLevel(constIndex) } -> std::same_as<std::size_t>;
    { storage.setIndexLevel(index, i) } -> std::same_as<void>;
    { storage.indexNeedsShift(index) } -> std::same_as<bool>;
    { storage.indexShift(index, key, index) } -> std::same_as<bool>;
    { storage.indexSplit(index) } -> std::same_as<std::optional<std::pair<typename Storage::Key, typename Storage::Index>>>;
    { storage.storeIndex(std::move(index)) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.deleteIndex(std::move(index)) } -> std::same_as<void>;
    { storage.createLeaf() } -> std::same_as<typename Storage::Leaf>;
    { storage.loadLeaf(pointer) } -> std::same_as<std::expected<typename Storage::Leaf, StarException>>;
    { storage.leafElementCount(constLeaf) } -> std::same_as<std::size_t>;
    { storage.leafKey(constLeaf, i) } -> std::same_as<typename Storage::Key>;
    { storage.leafData(constLeaf, i) } -> std::same_as<typename Storage::Data>;
    { storage.leafInsert(leaf, i, key, std::move(data)) } -> std::same_as<void>;
    { storage.leafRemove(leaf, i) } -> std::same_as<void>;
    { storage.nextLeaf(constLeaf) } -> std::same_as<std::optional<typename Storage::Pointer>>;
    { storage.setNextLeaf(leaf, next) } -> std::same_as<void>;
    { storage.setNextLeaf(leaf, pointer) } -> std::same_as<void>;
    { storage.leafNeedsShift(leaf) } -> std::same_as<bool>;
    { storage.leafShift(leaf, leaf) } -> std::same_as<bool>;
    { storage.leafSplit(leaf) } -> std::same_as<std::optional<typename Storage::Leaf>>;
    { storage.storeLeaf(std::move(leaf)) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.storeLeaf(storage.createLeaf()) } -> std::same_as<std::expected<typename Storage::Pointer, StarException>>;
    { storage.deleteLeaf(std::move(leaf)) } -> std::same_as<std::expected<void, StarException>>;
    { std::as_const(key) < std::as_const(key) } -> std::convertible_to<bool>;
    { pointer != storage.rootPointer() } -> std::convertible_to<bool>;
    { pointer != storage.indexPointer(constIndex, i) } -> std::convertible_to<bool>;
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

    // Range is inclusve on lower bound and exclusive on upper bound.
    [[nodiscard]] std::expected<List<std::pair<Key, Data>>, StarException> find(Key const& lower, Key const& upper);

    // Visitor returns expected<void, StarException> as visitor(key, data).
    template <typename Visitor>
    [[nodiscard]] std::expected<void, StarException> forEach(Key const& lower, Key const& upper, Visitor&& visitor);

    // Visitor is called as visitor(key, data).
    template <typename Visitor>
    [[nodiscard]] std::expected<void, StarException> forAll(Visitor&& visitor);

    // Recover all key value pairs possible, reporting node errors during scan and
    // reading as much data as possible.  Visitor is called as visitor(key, data),
    // ErrorHandler returns expected<void, StarException> as
    // error(std::string_view, ExceptionDiagnostic const&).
    template <typename Visitor, typename ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recoverAll(Visitor&& visitor, ErrorHandler&& error);

    // Visitor is called either as visitor(Index const&) or visitor(Leaf const&).
    // Visitor returns expected<bool, StarException>: false halts traversal.
    template <typename Visitor>
    [[nodiscard]] std::expected<void, StarException> forAllNodes(Visitor&& visitor);

    // returns true if old value overwritten.
    [[nodiscard]] std::expected<bool, StarException> insert(Key k, Data data);

    // returns true if key was found.
    [[nodiscard]] std::expected<bool, StarException> remove(Key k);

    // Removes and returns the key/value pairs in the half-open range.
    // Uses separate mutations for each matching key.
    [[nodiscard]] std::expected<List<std::pair<Key, Data>>, StarException> remove(Key const& lower, Key const& upper);

    [[nodiscard]] std::expected<std::uint64_t, StarException> indexCount();
    [[nodiscard]] std::expected<std::uint64_t, StarException> leafCount();
    [[nodiscard]] std::expected<std::uint64_t, StarException> recordCount();

    [[nodiscard]] std::expected<std::uint32_t, StarException> indexLevels();

    [[nodiscard]] std::expected<void, StarException> createNewRoot();

    private:
    struct DataElement {
      Key key;
      Data data;
    };
    using DataList = List<DataElement>;

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
      RemoveAction
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

      DataElement targetElement;
      ModifyAction action;
      bool found;
      ModifyState state;

      Key newKey;
      Pointer newPointer;
    };

    [[nodiscard]] std::expected<bool, StarException> contains(Index const& index, Key const& k);
    [[nodiscard]] std::expected<bool, StarException> contains(Leaf const& leaf, Key const& k);

    [[nodiscard]] std::expected<std::optional<Data>, StarException> find(Index const& index, Key const& k);
    [[nodiscard]] std::expected<std::optional<Data>, StarException> find(Leaf const& leaf, Key const& k);

    // Returns the highest key for the last leaf we have searched
    template <typename Visitor>
    [[nodiscard]] std::expected<Key, StarException> forEach(Index const& index, Key const& lower, Key const& upper, Visitor&& o);
    template <typename Visitor>
    [[nodiscard]] std::expected<Key, StarException> forEach(Leaf const& leaf, Key const& lower, Key const& upper, Visitor&& o);

    // Returns the highest key for the last leaf we have searched
    template <typename Visitor>
    [[nodiscard]] std::expected<Key, StarException> forAll(Index const& index, Visitor&& o);
    template <typename Visitor>
    [[nodiscard]] std::expected<Key, StarException> forAll(Leaf const& leaf, Visitor&& o);

    template <typename Visitor, typename ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recoverAll(Index const& index, Visitor&& o, ErrorHandler&& error);
    template <typename Visitor, typename ErrorHandler>
    [[nodiscard]] std::expected<void, StarException> recoverAll(Leaf const& leaf, Visitor&& o, ErrorHandler&& error);

    // Variable size values mean that merges can happen on inserts, so can't
    // split up into insert / remove methods
    [[nodiscard]] std::expected<void, StarException> modify(Leaf& leafNode, ModifyInfo& info);
    [[nodiscard]] std::expected<void, StarException> modify(Index& indexNode, ModifyInfo& info);
    [[nodiscard]] std::expected<bool, StarException> modify(DataElement e, ModifyAction action);

    // Traverses Indexes down the tree on the left side to get the least valued
    // key that is pointed to by any leaf under this index.  Needed when joining.
    [[nodiscard]] std::expected<Key, StarException> getLeftKey(Index const& index);

    template <typename Visitor>
    [[nodiscard]] std::expected<void, StarException> forAllNodes(Index const& index, Visitor&& visitor);

    std::pair<std::size_t, bool> leafFind(Leaf const& leaf, Key const& key);
    std::size_t indexFind(Index const& index, Key const& key);

    Storage m_storage;
  };

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Key const& k) {
    if (m_storage.rootIsLeaf()) {
      auto leaf = m_storage.loadLeaf(m_storage.rootPointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return contains(*leaf, k);
    } else {
      auto index = m_storage.loadIndex(m_storage.rootPointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return contains(*index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Key const& k) -> std::expected<std::optional<Data>, StarException> {
    if (m_storage.rootIsLeaf()) {
      auto leaf = m_storage.loadLeaf(m_storage.rootPointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return find(*leaf, k);
    } else {
      auto index = m_storage.loadIndex(m_storage.rootPointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return find(*index, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Key const& lower, Key const& upper) -> std::expected<List<std::pair<Key, Data>>, StarException> {
    DataCollector collector;
    auto result94 = forEach(lower, upper, collector);
    if (!result94) return std::unexpected(std::move(result94.error()));

    return std::move(collector.list);
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::forEach(Key const& lower, Key const& upper, Visitor&& visitor) {
    if (m_storage.rootIsLeaf()) {
      auto result90 = m_storage.loadLeaf(m_storage.rootPointer());
      if (!result90) return std::unexpected(std::move(result90.error()));
      auto result91 = forEach((*result90), lower, upper, std::forward<Visitor>(visitor));
      if (!result91) return std::unexpected(std::move(result91.error()));

    } else {
      auto result92 = m_storage.loadIndex(m_storage.rootPointer());
      if (!result92) return std::unexpected(std::move(result92.error()));
      auto result93 = forEach((*result92), lower, upper, std::forward<Visitor>(visitor));
      if (!result93) return std::unexpected(std::move(result93.error()));

    }
    return {};
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::forAll(Visitor&& visitor) {
    if (m_storage.rootIsLeaf()) {
      auto result86 = m_storage.loadLeaf(m_storage.rootPointer());
      if (!result86) return std::unexpected(std::move(result86.error()));
      auto result87 = forAll((*result86), std::forward<Visitor>(visitor));
      if (!result87) return std::unexpected(std::move(result87.error()));

    } else {
      auto result88 = m_storage.loadIndex(m_storage.rootPointer());
      if (!result88) return std::unexpected(std::move(result88.error()));
      auto result89 = forAll((*result88), std::forward<Visitor>(visitor));
      if (!result89) return std::unexpected(std::move(result89.error()));

    }
    return {};
  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recoverAll(Visitor&& visitor, ErrorHandler&& error) {
    if (m_storage.rootIsLeaf()) {
      auto node = m_storage.loadLeaf(m_storage.rootPointer());
      if (!node) return error("Error loading root index or leaf node", ExceptionDiagnostic(node.error()));
      return recoverAll(*node, visitor, error);
    }
    auto node = m_storage.loadIndex(m_storage.rootPointer());
    if (!node) return error("Error loading root index or leaf node", ExceptionDiagnostic(node.error()));
    return recoverAll(*node, visitor, error);
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::forAllNodes(Visitor&& visitor) {
    if (m_storage.rootIsLeaf()) {
      auto leaf = m_storage.loadLeaf(m_storage.rootPointer());
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      if (auto visited = visitor(*leaf); !visited)
        return std::unexpected(std::move(visited.error()));
    } else {
      auto index = m_storage.loadIndex(m_storage.rootPointer());
      if (!index) return std::unexpected(std::move(index.error()));
      return forAllNodes(*index, std::forward<Visitor>(visitor));
    }
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::insert(Key k, Data data) {
    return modify(DataElement{std::move(k), std::move(data)}, InsertAction);
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::remove(Key k) {
    return modify(DataElement{std::move(k), Data()}, RemoveAction);
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::remove(Key const& lower, Key const& upper) -> std::expected<List<std::pair<Key, Data>>, StarException> {
    DataCollector collector;
    auto result78 = forEach(lower, upper, collector);
    if (!result78) return std::unexpected(std::move(result78.error()));

    for (auto const& elem : collector.list) {
      auto result79 = remove(elem.first);
      if (!result79) return std::unexpected(std::move(result79.error()));

    }

    return std::move(collector.list);
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::indexCount() {
    IndexCounter counter = {*this, 0};
    auto result77 = forAllNodes(counter);
    if (!result77) return std::unexpected(std::move(result77.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::leafCount() {
    LeafCounter counter = {*this, 0};
    auto result76 = forAllNodes(counter);
    if (!result76) return std::unexpected(std::move(result76.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint64_t, StarException> BTree<Storage>::recordCount() {
    RecordCounter counter = {*this, 0};
    auto result75 = forAllNodes(counter);
    if (!result75) return std::unexpected(std::move(result75.error()));

    return counter.count;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<std::uint32_t, StarException> BTree<Storage>::indexLevels() {
    if (m_storage.rootIsLeaf()) {
      return 0;
    } else {
      auto result74 = m_storage.loadIndex(m_storage.rootPointer());
      if (!result74) return std::unexpected(std::move(result74.error()));
      return m_storage.indexLevel((*result74)) + 1;
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::createNewRoot() {
    auto rootPointer = m_storage.storeLeaf(m_storage.createLeaf());
    if (!rootPointer) return std::unexpected(std::move(rootPointer.error()));
    return m_storage.setNewRoot(*rootPointer, true);
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
    count += parent.m_storage.leafElementCount(leaf);
    return true;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::IndexCounter::operator()(Index const& index) {
    ++count;
    if (parent.m_storage.indexLevel(index) == 0) {
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
    if (parent.m_storage.indexLevel(index) == 0) {

      count += parent.m_storage.indexPointerCount(index);
      return false;
    } else {

      return true;
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::LeafCounter::operator()(Leaf const&) {
    return false;
  }

  template <btree_storage Storage>
  BTree<Storage>::ModifyInfo::ModifyInfo(ModifyAction a, DataElement e)
  : targetElement(std::move(e)), action(a) {
    found = false;
    state = Done;
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Index const& index, Key const& k) {
    std::size_t i = indexFind(index, k);
    if (m_storage.indexLevel(index) == 0) {
      auto leaf = m_storage.loadLeaf(m_storage.indexPointer(index, i));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return contains(*leaf, k);
    } else {
      auto childIndex = m_storage.loadIndex(m_storage.indexPointer(index, i));
      if (!childIndex) return std::unexpected(std::move(childIndex.error()));
      return contains(*childIndex, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<bool, StarException> BTree<Storage>::contains(Leaf const& leaf, Key const& k) {
    return leafFind(leaf, k).second;
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Index const& index, Key const& k) -> std::expected<std::optional<Data>, StarException> {
    std::size_t i = indexFind(index, k);
    if (m_storage.indexLevel(index) == 0) {
      auto leaf = m_storage.loadLeaf(m_storage.indexPointer(index, i));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return find(*leaf, k);
    } else {
      auto childIndex = m_storage.loadIndex(m_storage.indexPointer(index, i));
      if (!childIndex) return std::unexpected(std::move(childIndex.error()));
      return find(*childIndex, k);
    }
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::find(Leaf const& leaf, Key const& k) -> std::expected<std::optional<Data>, StarException> {
    std::pair<std::size_t, bool> res = leafFind(leaf, k);
    if (res.second) {
      return std::optional<Data>(std::in_place, m_storage.leafData(leaf, res.first));
    } else {
      return {};
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] auto BTree<Storage>::forEach(Index const& index, Key const& lower, Key const& upper, Visitor&& o) -> std::expected<Key, StarException> {
    std::size_t i = indexFind(index, lower);
    Key lastKey;

    if (m_storage.indexLevel(index) == 0) {
      auto result56 = m_storage.loadLeaf(m_storage.indexPointer(index, i));
      if (!result56) return std::unexpected(std::move(result56.error()));
      auto result57 = forEach((*result56), lower, upper, std::forward<Visitor>(o));
      if (!result57) return std::unexpected(std::move(result57.error()));
      lastKey = std::move(*result57);
    } else {
      auto result58 = m_storage.loadIndex(m_storage.indexPointer(index, i));
      if (!result58) return std::unexpected(std::move(result58.error()));
      auto result59 = forEach((*result58), lower, upper, std::forward<Visitor>(o));
      if (!result59) return std::unexpected(std::move(result59.error()));
      lastKey = std::move(*result59);
    }

    if (!(lastKey < upper)) {
      return lastKey;
    }

    while (i < m_storage.indexPointerCount(index) - 1) {

      ++i;

      // We're visiting the right side of the key, so if lastKey >=
      // indexKeyBefore(index, i), we have already visited this node via nextLeaf
      // pointers, so skip it.
      if (!(lastKey < m_storage.indexKeyBefore(index, i))) {
        continue;
      }

      if (m_storage.indexLevel(index) == 0) {
        auto result60 = m_storage.loadLeaf(m_storage.indexPointer(index, i));
        if (!result60) return std::unexpected(std::move(result60.error()));
        auto result61 = forEach((*result60), lower, upper, std::forward<Visitor>(o));
        if (!result61) return std::unexpected(std::move(result61.error()));
        lastKey = std::move(*result61);
      } else {
        auto result62 = m_storage.loadIndex(m_storage.indexPointer(index, i));
        if (!result62) return std::unexpected(std::move(result62.error()));
        auto result63 = forEach((*result62), lower, upper, std::forward<Visitor>(o));
        if (!result63) return std::unexpected(std::move(result63.error()));
        lastKey = std::move(*result63);
      }

      if (!(lastKey < upper)) {
        break;
      }
    }

    return lastKey;
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] auto BTree<Storage>::forEach(Leaf const& leaf, Key const& lower, Key const& upper, Visitor&& o) -> std::expected<Key, StarException> {
    if (m_storage.leafElementCount(leaf) == 0) {
      return Key();
    }

    std::size_t lowerIndex = leafFind(leaf, lower).first;

    for (std::size_t i = lowerIndex; i != m_storage.leafElementCount(leaf); ++i) {

      Key currentKey = m_storage.leafKey(leaf, i);
      if (!(currentKey < lower)) {

        if (currentKey < upper) {
          if (auto visited = o(currentKey, m_storage.leafData(leaf, i)); !visited)
            return std::unexpected(std::move(visited.error()));
        } else {
          return currentKey;
        }
      }
    }

    if (auto nextLeafPointer = m_storage.nextLeaf(leaf)) {
      auto nextLeaf = m_storage.loadLeaf(*nextLeafPointer);
      if (!nextLeaf) return std::unexpected(std::move(nextLeaf.error()));
      return forEach(*nextLeaf, lower, upper, o);
    } else {
      return m_storage.leafKey(leaf, m_storage.leafElementCount(leaf) - 1);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] auto BTree<Storage>::forAll(Index const& index, Visitor&& o) -> std::expected<Key, StarException> {
    Key lastKey;
    for (std::size_t i = 0; i < m_storage.indexPointerCount(index); ++i) {

      // If we're to the right of a given key, but lastKey >= this key, then we
      // must have already visited this node via nextLeaf pointers, so we can
      // skip it.
      if (i > 0 && !(lastKey < m_storage.indexKeyBefore(index, i))) {
        continue;
      }

      if (m_storage.indexLevel(index) == 0) {
        auto result49 = m_storage.loadLeaf(m_storage.indexPointer(index, i));
        if (!result49) return std::unexpected(std::move(result49.error()));
        auto result50 = forAll((*result49), std::forward<Visitor>(o));
        if (!result50) return std::unexpected(std::move(result50.error()));
        lastKey = std::move(*result50);
      } else {
        auto result51 = m_storage.loadIndex(m_storage.indexPointer(index, i));
        if (!result51) return std::unexpected(std::move(result51.error()));
        auto result52 = forAll((*result51), std::forward<Visitor>(o));
        if (!result52) return std::unexpected(std::move(result52.error()));
        lastKey = std::move(*result52);
      }
    }

    return lastKey;
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] auto BTree<Storage>::forAll(Leaf const& leaf, Visitor&& o) -> std::expected<Key, StarException> {
    if (m_storage.leafElementCount(leaf) == 0) {
      return Key();
    }

    for (std::size_t i = 0; i != m_storage.leafElementCount(leaf); ++i) {

      Key currentKey = m_storage.leafKey(leaf, i);
      if (auto visited = o(m_storage.leafKey(leaf, i), m_storage.leafData(leaf, i)); !visited)
        return std::unexpected(std::move(visited.error()));
    }

    if (auto nextLeafPointer = m_storage.nextLeaf(leaf)) {
      auto nextLeaf = m_storage.loadLeaf(*nextLeafPointer);
      if (!nextLeaf) return std::unexpected(std::move(nextLeaf.error()));
      return forAll(*nextLeaf, std::forward<Visitor>(o));
    } else {
      return m_storage.leafKey(leaf, m_storage.leafElementCount(leaf) - 1);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recoverAll(Index const& index, Visitor&& visitor, ErrorHandler&& error) {
    for (std::size_t i = 0; i < m_storage.indexPointerCount(index); ++i) {
      if (m_storage.indexLevel(index) == 0) {
        auto node = m_storage.loadLeaf(m_storage.indexPointer(index, i));
        if (!node) {
          auto reported = error("Error loading leaf node", ExceptionDiagnostic(node.error()));
          if (!reported) return std::unexpected(std::move(reported.error()));
          continue;
        }
        auto recovered = recoverAll(*node, visitor, error);
        if (!recovered) return std::unexpected(std::move(recovered.error()));
      } else {
        auto node = m_storage.loadIndex(m_storage.indexPointer(index, i));
        if (!node) {
          auto reported = error("Error loading index node", ExceptionDiagnostic(node.error()));
          if (!reported) return std::unexpected(std::move(reported.error()));
          continue;
        }
        auto recovered = recoverAll(*node, visitor, error);
        if (!recovered) return std::unexpected(std::move(recovered.error()));
      }
    }
    return {};

  }

  template <btree_storage Storage>
  template <typename Visitor, typename ErrorHandler>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::recoverAll(Leaf const& leaf, Visitor&& visitor, ErrorHandler&& error) {
    for (std::size_t i = 0; i != m_storage.leafElementCount(leaf); ++i) {
      auto visited = visitor(m_storage.leafKey(leaf, i), m_storage.leafData(leaf, i));
      if (!visited) return error("Error reading leaf node", ExceptionDiagnostic(visited.error()));
    }
    return {};

  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::modify(Leaf& leafNode, ModifyInfo& info) {
    info.state = Done;

    std::pair<std::size_t, bool> res = leafFind(leafNode, info.targetElement.key);
    std::size_t i = res.first;
    if (res.second) {

      info.found = true;
      m_storage.leafRemove(leafNode, i);
    }

    // No change necessary.
    if (info.action == RemoveAction && !info.found) {
      return {};
    }

    if (info.action == InsertAction) {
      m_storage.leafInsert(leafNode, i, info.targetElement.key, std::move(info.targetElement.data));
    }

    auto splitResult = m_storage.leafSplit(leafNode);
    if (splitResult) {

      m_storage.setNextLeaf(*splitResult, m_storage.nextLeaf(leafNode));
      info.newKey = m_storage.leafKey(*splitResult, 0);
      auto result45 = m_storage.storeLeaf(std::move(*splitResult));
      if (!result45) return std::unexpected(std::move(result45.error()));
      info.newPointer = std::move(*result45);

      m_storage.setNextLeaf(leafNode, info.newPointer);
      info.state = LeafSplit;
    } else {
      if (m_storage.leafNeedsShift(leafNode)) {

        info.state = LeafNeedsJoin;
      } else {

        info.state = LeafNeedsUpdate;
      }
    }
    return {};
  }

  template <btree_storage Storage>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::modify(Index& indexNode, ModifyInfo& info) {
    std::size_t i = indexFind(indexNode, info.targetElement.key);
    Pointer nextPointer = m_storage.indexPointer(indexNode, i);

    Leaf lowerLeaf;
    Index lowerIndex;
    if (m_storage.indexLevel(indexNode) == 0) {

      auto result22 = m_storage.loadLeaf(nextPointer);
      if (!result22) return std::unexpected(std::move(result22.error()));
      lowerLeaf = std::move(*result22);
      auto result23 = modify(lowerLeaf, info);
      if (!result23) return std::unexpected(std::move(result23.error()));

    } else {

      auto result24 = m_storage.loadIndex(nextPointer);
      if (!result24) return std::unexpected(std::move(result24.error()));
      lowerIndex = std::move(*result24);
      auto result25 = modify(lowerIndex, info);
      if (!result25) return std::unexpected(std::move(result25.error()));

    }

    if (info.state == Done) {
      return {};
    }

    bool selfUpdated = false;

    std::size_t left = 0;
    std::size_t right = 0;
    if (i != 0 && i == m_storage.indexPointerCount(indexNode) - 1) {

      left = i - 1;
      right = i;
    } else {

      left = i;
      right = i + 1;
    }

    if (info.state == LeafNeedsJoin) {

      if (m_storage.indexPointerCount(indexNode) < 2) {

        // Don't have enough leaves to join, just do the pending update.
        info.state = LeafNeedsUpdate;
      } else {

        Leaf leftLeaf;
        Leaf rightLeaf;

        if (left == i) {

          leftLeaf = lowerLeaf;
          auto result26 = m_storage.loadLeaf(m_storage.indexPointer(indexNode, right));
          if (!result26) return std::unexpected(std::move(result26.error()));
          rightLeaf = std::move(*result26);
        } else {

          auto result27 = m_storage.loadLeaf(m_storage.indexPointer(indexNode, left));
          if (!result27) return std::unexpected(std::move(result27.error()));
          leftLeaf = std::move(*result27);
          rightLeaf = lowerLeaf;
        }

        if (!m_storage.leafShift(leftLeaf, rightLeaf)) {

          // Leaves not modified, just do the pending update.
          info.state = LeafNeedsUpdate;
        } else {
          if (m_storage.leafElementCount(rightLeaf) == 0) {

            // Leaves merged.
            m_storage.setNextLeaf(leftLeaf, m_storage.nextLeaf(rightLeaf));
            auto result28 = m_storage.deleteLeaf(std::move(rightLeaf));
            if (!result28) return std::unexpected(std::move(result28.error()));

            // Replace two sibling pointer elements with one pointing to merged
            // leaf.
            if (left != 0) {
              m_storage.indexUpdateKeyBefore(indexNode, left, m_storage.leafKey(leftLeaf, 0));
            }

            auto result29 = m_storage.storeLeaf(std::move(leftLeaf));
            if (!result29) return std::unexpected(std::move(result29.error()));
            m_storage.indexUpdatePointer(indexNode, left, (*result29));
            m_storage.indexRemoveBefore(indexNode, right);

            selfUpdated = true;
          } else {

            // Leaves shifted.
            auto result30 = m_storage.storeLeaf(std::move(leftLeaf));
            if (!result30) return std::unexpected(std::move(result30.error()));
            m_storage.indexUpdatePointer(indexNode, left, (*result30));

            // Right leaf first key changes on shift, so always need to update
            // left index node.
            m_storage.indexUpdateKeyBefore(indexNode, right, m_storage.leafKey(rightLeaf, 0));

            auto result31 = m_storage.storeLeaf(std::move(rightLeaf));
            if (!result31) return std::unexpected(std::move(result31.error()));
            m_storage.indexUpdatePointer(indexNode, right, (*result31));

            selfUpdated = true;
          }
        }
      }
    }

    if (info.state == IndexNeedsJoin) {

      if (m_storage.indexPointerCount(indexNode) < 2) {

        // Don't have enough indexes to join, just do the pending update.
        info.state = IndexNeedsUpdate;
      } else {

        Index leftIndex;
        Index rightIndex;

        if (left == i) {

          leftIndex = lowerIndex;
          auto result32 = m_storage.loadIndex(m_storage.indexPointer(indexNode, right));
          if (!result32) return std::unexpected(std::move(result32.error()));
          rightIndex = std::move(*result32);
        } else {

          auto result33 = m_storage.loadIndex(m_storage.indexPointer(indexNode, left));
          if (!result33) return std::unexpected(std::move(result33.error()));
          leftIndex = std::move(*result33);
          rightIndex = lowerIndex;
        }

        auto result34 = getLeftKey(rightIndex);
        if (!result34) return std::unexpected(std::move(result34.error()));
        if (!m_storage.indexShift(leftIndex, (*result34), rightIndex)) {

          // Indexes not modified, just do the pending update.
          info.state = IndexNeedsUpdate;
        } else {
          if (m_storage.indexPointerCount(rightIndex) == 0) {

            // Indexes merged.
            m_storage.deleteIndex(std::move(rightIndex));

            // Replace two sibling pointer elements with one pointing to merged
            // index.
            if (left != 0) {
              auto result35 = getLeftKey(leftIndex);
              if (!result35) return std::unexpected(std::move(result35.error()));
              m_storage.indexUpdateKeyBefore(indexNode, left, (*result35));
            }

            auto result36 = m_storage.storeIndex(std::move(leftIndex));
            if (!result36) return std::unexpected(std::move(result36.error()));
            m_storage.indexUpdatePointer(indexNode, left, (*result36));
            m_storage.indexRemoveBefore(indexNode, right);

            selfUpdated = true;
          } else {

            // Indexes shifted.
            auto result37 = m_storage.storeIndex(std::move(leftIndex));
            if (!result37) return std::unexpected(std::move(result37.error()));
            m_storage.indexUpdatePointer(indexNode, left, (*result37));

            // Right index first key changes on shift, so always need to update
            // right index node.
            auto result38 = getLeftKey(rightIndex);
            if (!result38) return std::unexpected(std::move(result38.error()));
            Key keyForRight = std::move(*result38);
            auto result39 = m_storage.storeIndex(std::move(rightIndex));
            if (!result39) return std::unexpected(std::move(result39.error()));
            m_storage.indexUpdatePointer(indexNode, right, (*result39));
            m_storage.indexUpdateKeyBefore(indexNode, right, keyForRight);

            selfUpdated = true;
          }
        }
      }
    }

    if (info.state == LeafSplit) {

      auto result40 = m_storage.storeLeaf(std::move(lowerLeaf));
      if (!result40) return std::unexpected(std::move(result40.error()));
      m_storage.indexUpdatePointer(indexNode, i, (*result40));
      m_storage.indexInsertAfter(indexNode, i, info.newKey, info.newPointer);
      selfUpdated = true;
    }

    if (info.state == IndexSplit) {

      auto result41 = m_storage.storeIndex(std::move(lowerIndex));
      if (!result41) return std::unexpected(std::move(result41.error()));
      m_storage.indexUpdatePointer(indexNode, i, (*result41));
      m_storage.indexInsertAfter(indexNode, i, info.newKey, info.newPointer);
      selfUpdated = true;
    }

    if (info.state == LeafNeedsUpdate) {

      auto result42 = m_storage.storeLeaf(std::move(lowerLeaf));
      if (!result42) return std::unexpected(std::move(result42.error()));
      Pointer lowerLeafPointer = std::move(*result42);
      if (lowerLeafPointer != m_storage.indexPointer(indexNode, i)) {

        m_storage.indexUpdatePointer(indexNode, i, lowerLeafPointer);
        selfUpdated = true;
      }
    }

    if (info.state == IndexNeedsUpdate) {

      auto result43 = m_storage.storeIndex(std::move(lowerIndex));
      if (!result43) return std::unexpected(std::move(result43.error()));
      Pointer lowerIndexPointer = std::move(*result43);
      if (lowerIndexPointer != m_storage.indexPointer(indexNode, i)) {

        m_storage.indexUpdatePointer(indexNode, i, lowerIndexPointer);
        selfUpdated = true;
      }
    }

    auto splitResult = m_storage.indexSplit(indexNode);
    if (splitResult) {

      info.newKey = splitResult->first;
      auto result44 = m_storage.storeIndex(std::move(*splitResult).second);
      if (!result44) return std::unexpected(std::move(result44.error()));
      info.newPointer = std::move(*result44);
      info.state = IndexSplit;
      selfUpdated = true;
    } else {
      if (m_storage.indexNeedsShift(indexNode)) {

        info.state = IndexNeedsJoin;
      } else {
        if (selfUpdated) {

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

    Leaf lowerLeaf;
    Index lowerIndex;
    if (m_storage.rootIsLeaf()) {

      auto result9 = m_storage.loadLeaf(m_storage.rootPointer());
      if (!result9) return std::unexpected(std::move(result9.error()));
      lowerLeaf = std::move(*result9);
      auto result10 = modify(lowerLeaf, info);
      if (!result10) return std::unexpected(std::move(result10.error()));

    } else {

      auto result11 = m_storage.loadIndex(m_storage.rootPointer());
      if (!result11) return std::unexpected(std::move(result11.error()));
      lowerIndex = std::move(*result11);
      auto result12 = modify(lowerIndex, info);
      if (!result12) return std::unexpected(std::move(result12.error()));

    }

    if (info.state == IndexNeedsJoin) {

      if (m_storage.indexPointerCount(lowerIndex) == 1) {

        // If root index has single pointer, then make that the new root.

        // release index first (to support the common use case of delaying
        // removes until setNewRoot)
        Pointer pointer = m_storage.indexPointer(lowerIndex, 0);
        std::size_t level = m_storage.indexLevel(lowerIndex);
        m_storage.deleteIndex(std::move(lowerIndex));
        auto result13 = m_storage.setNewRoot(pointer, level == 0);
        if (!result13) return std::unexpected(std::move(result13.error()));

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

      Index newRoot;
      if (info.state == IndexSplit) {

        auto rootIndexLevel = m_storage.indexLevel(lowerIndex) + 1;
        auto result14 = m_storage.storeIndex(std::move(lowerIndex));
        if (!result14) return std::unexpected(std::move(result14.error()));
        newRoot = m_storage.createIndex((*result14));
        m_storage.setIndexLevel(newRoot, rootIndexLevel);
      } else {

        auto result15 = m_storage.storeLeaf(std::move(lowerLeaf));
        if (!result15) return std::unexpected(std::move(result15.error()));
        newRoot = m_storage.createIndex((*result15));
        m_storage.setIndexLevel(newRoot, 0);
      }
      m_storage.indexInsertAfter(newRoot, 0, info.newKey, info.newPointer);
      auto result16 = m_storage.storeIndex(std::move(newRoot));
      if (!result16) return std::unexpected(std::move(result16.error()));
      auto result17 = m_storage.setNewRoot((*result16), false);
      if (!result17) return std::unexpected(std::move(result17.error()));

    }

    if (info.state == IndexNeedsUpdate) {

      auto result18 = m_storage.storeIndex(std::move(lowerIndex));
      if (!result18) return std::unexpected(std::move(result18.error()));
      Pointer newRootPointer = std::move(*result18);
      if (newRootPointer != m_storage.rootPointer()) {
        auto result19 = m_storage.setNewRoot(newRootPointer, false);
        if (!result19) return std::unexpected(std::move(result19.error()));

      }
    }

    if (info.state == LeafNeedsUpdate) {

      auto result20 = m_storage.storeLeaf(std::move(lowerLeaf));
      if (!result20) return std::unexpected(std::move(result20.error()));
      Pointer newRootPointer = std::move(*result20);
      if (newRootPointer != m_storage.rootPointer()) {
        auto result21 = m_storage.setNewRoot(newRootPointer, true);
        if (!result21) return std::unexpected(std::move(result21.error()));

      }
    }

    return info.found;
  }

  template <btree_storage Storage>
  [[nodiscard]] auto BTree<Storage>::getLeftKey(Index const& index) -> std::expected<Key, StarException> {
    if (m_storage.indexLevel(index) == 0) {

      auto leaf = m_storage.loadLeaf(m_storage.indexPointer(index, 0));
      if (!leaf) return std::unexpected(std::move(leaf.error()));
      return m_storage.leafKey(*leaf, 0);
    } else {

      auto childIndex = m_storage.loadIndex(m_storage.indexPointer(index, 0));
      if (!childIndex) return std::unexpected(std::move(childIndex.error()));
      return getLeftKey(*childIndex);
    }
  }

  template <btree_storage Storage>
  template <typename Visitor>
  [[nodiscard]] std::expected<void, StarException> BTree<Storage>::forAllNodes(Index const& index, Visitor&& visitor) {
    auto result1 = visitor(index);
    if (!result1) return std::unexpected(std::move(result1.error()));
    if (!(*result1)) {
      return {};
    }

    for (std::size_t i = 0; i < m_storage.indexPointerCount(index); ++i) {

      if (m_storage.indexLevel(index) != 0) {

        auto result2 = m_storage.loadIndex(m_storage.indexPointer(index, i));
        if (!result2) return std::unexpected(std::move(result2.error()));
        auto result3 = forAllNodes((*result2), std::forward<Visitor>(visitor));
        if (!result3) return std::unexpected(std::move(result3.error()));

      } else {

        auto result4 = m_storage.loadLeaf(m_storage.indexPointer(index, i));
        if (!result4) return std::unexpected(std::move(result4.error()));
        auto result5 = visitor((*result4));
        if (!result5) return std::unexpected(std::move(result5.error()));
        if (!(*result5)) {
          return {};
        }
      }
    }
    return {};
  }

  template <btree_storage Storage>
  std::pair<std::size_t, bool> BTree<Storage>::leafFind(Leaf const& leaf, Key const& key) {
    // Return lower bound binary search result.
    std::size_t size = m_storage.leafElementCount(leaf);
    if (size == 0)
    return {0, false};

    std::size_t len = size;
    std::size_t first = 0;
    std::size_t middle = 0;
    std::size_t half;
    while (len > 0) {
      half = len / 2;
      middle = first + half;
      if (m_storage.leafKey(leaf, middle) < key) {
        first = middle + 1;
        len = len - half - 1;
      } else {
        len = half;
      }
    }
    return std::make_pair(first, first < size && !(key < m_storage.leafKey(leaf, first)));
  }

  template <btree_storage Storage>
  std::size_t BTree<Storage>::indexFind(Index const& index, Key const& key) {
    // Return upper bound binary search result of range [1, size];
    std::size_t size = m_storage.indexPointerCount(index);
    if (size == 0)
    return 0;

    std::size_t len = size - 1;
    std::size_t first = 1;
    std::size_t middle = 1;
    std::size_t half;
    while (len > 0) {
      half = len / 2;
      middle = first + half;
      if (key < m_storage.indexKeyBefore(index, middle)) {
        len = half;
      } else {
        first = middle + 1;
        len = len - half - 1;
      }
    }
    return first - 1;
  }

}

export module star.btree;

export namespace Star {
  using ::Star::BTree;
  using ::Star::btree_storage;
}
