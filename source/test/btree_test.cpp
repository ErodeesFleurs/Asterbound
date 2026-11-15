import star.btree;
import std;
import star.list;
import star.format;
import star.optional;
import star.string;
import star.map;
import star.set;
import star.lexical_cast;
import star.random;

#include "gtest/gtest.h"

using namespace Star;

template <typename Key, typename Pointer>
struct SimpleBTreeIndex {
  std::size_t pointer_count() const;
  Pointer pointer(std::size_t i) const;
  void update_pointer(std::size_t i, Pointer p);

  Key const& key_before(std::size_t i) const;
  void update_key_before(std::size_t i, Key k);

  void erase_before(std::size_t i);
  void insert_after(std::size_t i, Key k, Pointer p);

  std::size_t index_level() const;
  void set_index_level(std::size_t index_level);

  // count is number of elements to shift left *including* right's beginPointer
  void shift_left(Key const& mid, SimpleBTreeIndex& right, std::size_t count);

  // count is number of elements to shift right
  void shift_right(Key const& mid, SimpleBTreeIndex& left, std::size_t count);

  // i should be index of pointer that will be the new beginPointer of right
  // node (cannot be 0).
  Key split(SimpleBTreeIndex& right, std::size_t i);

  struct Element {
    Key key;
    Pointer pointer;
  };
  typedef List<Element> ElementList;

  Pointer self;
  std::size_t level;
  std::optional<Pointer> beginPointer;
  ElementList pointers;
};

template <typename Key, typename Data, typename Pointer>
struct SimpleBTreeLeaf {
  std::size_t count() const;
  Key const& key(std::size_t i) const;
  Data const& data(std::size_t i) const;

  void insert(std::size_t i, Key k, Data d);
  void erase(std::size_t i);

  std::optional<Pointer> next_leaf() const;
  void set_next_leaf(std::optional<Pointer> n);

  // count is number of elements to shift left
  void shift_left(SimpleBTreeLeaf& right, std::size_t count);

  // count is number of elements to shift right
  void shift_right(SimpleBTreeLeaf& left, std::size_t count);

  // i should be index of element that will be the new start of right node.
  // Returns right index node.
  void split(SimpleBTreeLeaf& right, std::size_t i);

  struct Element {
    Key key;
    Data data;
  };
  typedef List<Element> ElementList;

  std::optional<Pointer> next;
  Pointer self;
  ElementList elements;
};

template <typename Key, typename Pointer>
std::size_t SimpleBTreeIndex<Key, Pointer>::pointer_count() const {
  // If no begin pointer is set then the index is simply uninitialized.
  if (!beginPointer)
    return 0;
  else
    return pointers.size() + 1;
}

template <typename Key, typename Pointer>
Pointer SimpleBTreeIndex<Key, Pointer>::pointer(std::size_t i) const {
  if (i == 0)
    return *beginPointer;
  else
    return pointers[i - 1].pointer;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::update_pointer(std::size_t i, Pointer p) {
  if (i == 0)
    *beginPointer = p;
  else
    pointers[i - 1].pointer = p;
}

template <typename Key, typename Pointer>
Key const& SimpleBTreeIndex<Key, Pointer>::key_before(std::size_t i) const {
  return pointers[i - 1].key;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::update_key_before(std::size_t i, Key k) {
  pointers[i - 1].key = k;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::erase_before(std::size_t i) {
  if (i == 0) {
    beginPointer = pointers[0].pointer;
    pointers.erase_at(0);
  } else {
    pointers.erase_at(i - 1);
  }
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::insert_after(std::size_t i, Key k, Pointer p) {
  pointers.insert_at(i, Element{k, p});
}

template <typename Key, typename Pointer>
std::size_t SimpleBTreeIndex<Key, Pointer>::index_level() const {
  return level;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::set_index_level(std::size_t index_level) {
  level = index_level;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::shift_left(Key const& mid, SimpleBTreeIndex& right, std::size_t count) {
  count = std::min(right.pointer_count(), count);

  if (count == 0)
    return;

  pointers.push_back(Element{mid, *right.beginPointer});

  typename ElementList::iterator s = right.pointers.begin();
  std::advance(s, count - 1);
  pointers.insert(pointers.end(), right.pointers.begin(), s);

  right.pointers.erase(right.pointers.begin(), s);
  if (right.pointers.size() != 0) {
    right.beginPointer = right.pointers[0].pointer;
    right.pointers.erase_at(0);
  } else {
    right.beginPointer.reset();
  }
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::shift_right(Key const& mid, SimpleBTreeIndex& left, std::size_t count) {
  count = std::min(left.pointer_count(), count);

  if (count == 0)
    return;
  --count;

  pointers.insert(pointers.begin(), Element{mid, *beginPointer});

  typename ElementList::iterator s = left.pointers.begin();
  std::advance(s, left.pointers.size() - count);
  pointers.insert(pointers.begin(), s, left.pointers.end());

  left.pointers.erase(s, left.pointers.end());
  if (left.pointers.size() != 0) {
    beginPointer = left.pointers[left.pointers.size() - 1].pointer;
    left.pointers.erase_at(left.pointers.size() - 1);
  } else {
    beginPointer = std::exchange(left.beginPointer, std::nullopt);
  }
}

template <typename Key, typename Pointer>
Key SimpleBTreeIndex<Key, Pointer>::split(SimpleBTreeIndex& right, std::size_t i) {
  typename ElementList::iterator s = pointers.begin();
  std::advance(s, i - 1);

  right.beginPointer = s->pointer;
  Key midKey = s->key;
  right.level = level;
  ++s;

  right.pointers.insert(right.pointers.begin(), s, pointers.end());
  --s;

  pointers.erase(s, pointers.end());

  return midKey;
}

template <typename Key, typename Data, typename Pointer>
std::size_t SimpleBTreeLeaf<Key, Data, Pointer>::count() const {
  return elements.size();
}

template <typename Key, typename Data, typename Pointer>
Key const& SimpleBTreeLeaf<Key, Data, Pointer>::key(std::size_t i) const {
  return elements[i].key;
}

template <typename Key, typename Data, typename Pointer>
Data const& SimpleBTreeLeaf<Key, Data, Pointer>::data(std::size_t i) const {
  return elements[i].data;
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::insert(std::size_t i, Key k, Data d) {
  elements.insert_at(i, Element{std::move(k), std::move(d)});
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::erase(std::size_t i) {
  elements.erase_at(i);
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::shift_left(SimpleBTreeLeaf& right, std::size_t count) {
  count = std::min(right.count(), count);

  if (count == 0)
    return;

  typename ElementList::iterator s = right.elements.begin();
  std::advance(s, count);

  elements.insert(elements.end(), right.elements.begin(), s);
  right.elements.erase(right.elements.begin(), s);
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::shift_right(SimpleBTreeLeaf& left, std::size_t count) {
  count = std::min(left.count(), count);

  if (count == 0)
    return;

  typename ElementList::iterator s = left.elements.begin();
  std::advance(s, left.elements.size() - count);

  elements.insert(elements.begin(), s, left.elements.end());
  left.elements.erase(s, left.elements.end());
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::split(SimpleBTreeLeaf& right, std::size_t i) {
  typename ElementList::iterator s = elements.begin();
  std::advance(s, i);

  right.elements.insert(right.elements.begin(), s, elements.end());
  elements.erase(s, elements.end());
}

template <typename Key, typename Data, typename Pointer>
std::optional<Pointer> SimpleBTreeLeaf<Key, Data, Pointer>::next_leaf() const {
  return next;
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::set_next_leaf(std::optional<Pointer> n) {
  next = std::move(n);
}

// Testing BTree class that simulates storage by storing in-memory copies of
// nodes.  Used to test BTree algorithm.

template <typename StorageKey>
struct InMemoryBTreeStorage {
  typedef StorageKey Key;
  typedef String Data;
  typedef int Pointer;

  typedef SimpleBTreeIndex<Key, int> Index;
  typedef SimpleBTreeLeaf<Key, String, int> Leaf;

  InMemoryBTreeStorage(std::size_t maxisize, std::size_t maxlsize, bool leafLinks = true)
    : root(-1), rootleaf(true), maxIndexSize(maxisize), maxLeafSize(maxlsize),
      indexId(0), leafId(0), keepLeafLinks(leafLinks) {}

  Pointer root_pointer() {
    return root;
  }

  bool root_is_leaf() {
    return rootleaf;
  }

  [[nodiscard]] std::expected<void, StarException> set_new_root(Pointer pointer, bool isLeaf) {
    root = pointer;
    rootleaf = isLeaf;

    for (int i : deletedLeaves)
      leaves.remove(i);

    for (int i : deletedIndexes)
      indexes.remove(i);

    deletedLeaves.clear();
    deletedIndexes.clear();
    return {};
  }

  // Should create new empty leaf.
  Leaf create_leaf() {
    Leaf leaf;
    leaf.self = -1;
    return leaf;
  }

  [[nodiscard]] std::expected<Leaf, StarException> load_leaf(Pointer const& pointer) {
    // To make sure to accurately test storage, always *copy* in and out
    auto leaf = leaves.get(pointer);
    if (!leaf)
      return std::unexpected(std::move(leaf.error()));
    return leaf->get();
  }

  bool leaf_needs_shift(Leaf const& leaf) {
    return leaf.count() < (maxLeafSize + 1) / 2;
  }

  bool should_append_new_leaf(Leaf const& leaf) {
    return maxLeafSize == 2 && leaf.count() == 2;
  }

  bool leaf_shift(Leaf& left, Leaf& right) {
    if (left.count() + right.count() <= maxLeafSize) {
      left.shift_left(right, right.count());
      return true;
    } else {
      if (leaf_needs_shift(right)) {
        right.shift_right(left, 1);
        return true;
      } else if (leaf_needs_shift(left)) {
        left.shift_left(right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  std::optional<Leaf> leaf_split(Leaf& leaf) {
    if (leaf.count() <= maxLeafSize) {
      return {};
    } else {
      Leaf right;
      right.self = -1;

      leaf.split(right, (leaf.count() + 1) / 2);

      return right;
    }
  }

  [[nodiscard]] std::expected<Pointer, StarException> store_leaf(Leaf leaf) {
    auto previous_pointer = leaf.self;
    if (leaf.self != -1) {
      auto deleted = delete_leaf(leaf);
      if (!deleted)
        return std::unexpected(std::move(deleted.error()));
    }

    while (leaves.contains(leafId))
      ++leafId;
    leaf.self = leafId;
    // Relocating a leaf must also redirect incoming links before old nodes are
    // reclaimed at root publication. Index pointers are updated by BTree.
    if (keepLeafLinks && previous_pointer != -1) {
      for (auto& entry : leaves) {
        if (entry.second.next == previous_pointer)
          entry.second.set_next_leaf(leaf.self);
      }
    }

    // To make sure to accurately test storage, always *copy* in and out
    leaves[leafId] = leaf;

    return leaf.self;
  }

  [[nodiscard]] std::expected<void, StarException> delete_leaf(Leaf const& leaf) {
    deletedLeaves.push_back(leaf.self);
    return {};
  }

  // Should create new index with two pointers and one mid key.
  Index create_index(Pointer beginPointer) {
    Index indexNode;
    indexNode.self = -1;
    indexNode.level = 0;
    indexNode.beginPointer = beginPointer;
    return indexNode;
  }

  [[nodiscard]] std::expected<Index, StarException> load_index(Pointer const& pointer) {
    auto index = indexes.get(pointer);
    if (!index)
      return std::unexpected(std::move(index.error()));
    return index->get();
  }

  bool index_needs_shift(Index const& index) {
    return index.pointer_count() < (maxIndexSize + 1) / 2;
  }

  bool index_shift(Index& left, Key const& mid, Index& right) {
    if (left.pointer_count() + right.pointer_count() <= maxIndexSize) {
      left.shift_left(mid, right, right.pointer_count());
      return true;
    } else {
      if (index_needs_shift(right)) {
        right.shift_right(mid, left, 1);
        return true;
      } else if (index_needs_shift(left)) {
        left.shift_left(mid, right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  std::optional<std::pair<Key, Index>> index_split(Index& index) {
    if (index.pointer_count() <= maxIndexSize) {
      return {};
    } else {
      Index right;
      right.self = -1;

      Key mid = index.split(right, (index.pointer_count() + 1) / 2);

      return std::make_pair(mid, right);
    }
  }

  [[nodiscard]] std::expected<Pointer, StarException> store_index(Index index) {
    if (index.self != -1)
      delete_index(index);

    while (indexes.contains(indexId))
      ++indexId;
    index.self = indexId;

    indexes[indexId] = index;

    return index.self;
  }

  void delete_index(Index const& index) { deletedIndexes.push_back(index.self); }

  std::size_t index_pointer_count(Index const& index) {
    return index.pointer_count();
  }

  Pointer index_pointer(Index const& index, std::size_t i) {
    return index.pointer(i);
  }

  void index_update_pointer(Index& index, std::size_t i, Pointer p) {
    index.update_pointer(i, p);
  }

  Key const& index_key_before(Index const& index, std::size_t i) {
    return index.key_before(i);
  }

  void index_update_key_before(Index& index, std::size_t i, Key k) {
    index.update_key_before(i, k);
  }

  void index_erase_before(Index& index, std::size_t i) {
    index.erase_before(i);
  }

  void index_insert_after(Index& index, std::size_t i, Key k, Pointer p) {
    index.insert_after(i, k, p);
  }

  std::size_t index_level(Index const& index) {
    return index.index_level();
  }

  void set_index_level(Index& index, std::size_t index_level) {
    index.set_index_level(index_level);
  }

  std::size_t leaf_element_count(Leaf const& leaf) {
    return leaf.count();
  }

  Key const& leaf_key(Leaf const& leaf, std::size_t i) {
    return leaf.key(i);
  }

  Data const& leaf_data(Leaf const& leaf, std::size_t i) {
    return leaf.data(i);
  }

  void leaf_insert(Leaf& leaf, std::size_t i, Key k, Data d) {
    return leaf.insert(i, k, d);
  }

  void leaf_erase(Leaf& leaf, std::size_t i) {
    return leaf.erase(i);
  }

  std::optional<Pointer> next_leaf(Leaf const& leaf) {
    return keepLeafLinks ? leaf.next_leaf() : std::nullopt;
  }

  void set_next_leaf(Leaf& leaf, std::optional<Pointer> n) {
    if (keepLeafLinks)
      leaf.set_next_leaf(n);
  }

  int root;
  bool rootleaf;

  std::size_t maxIndexSize;
  std::size_t maxLeafSize;

  int indexId;
  int leafId;
  bool keepLeafLinks;

  Map<int, Index> indexes;
  Map<int, Leaf> leaves;

  List<int> deletedLeaves;
  List<int> deletedIndexes;
};

using SimpleBTreeStorage = InMemoryBTreeStorage<int>;

static_assert(btree_storage<SimpleBTreeStorage>);
static_assert(!std::is_base_of_v<SimpleBTreeStorage, BTree<SimpleBTreeStorage>>);

struct SimpleBTree : public BTree<SimpleBTreeStorage> {
  SimpleBTree(std::size_t maxisize, std::size_t maxlsize, bool leafLinks = true)
    : BTree(std::in_place, maxisize, maxlsize, leafLinks) {}

  [[nodiscard]] std::expected<void, StarException> print() {
    auto printed = for_all_nodes(Printer());
    if (!printed)
      return std::unexpected(std::move(printed.error()));
    return Star::print("\n");
  }

  struct Printer {
    [[nodiscard]] std::expected<bool, StarException> operator()(Index const& index) {
      if (auto written = Star::print("[{}:{}] {} ", index.level, index.self, Star::output_optional(index.beginPointer)); !written)
        return std::unexpected(std::move(written.error()));
      for (Index::Element e : index.pointers) {
        if (auto written = Star::print("({}) {} ", e.key, e.pointer); !written)
          return std::unexpected(std::move(written.error()));
      }
      if (auto written = Star::print("\n"); !written)
        return std::unexpected(std::move(written.error()));
      return true;
    }

    [[nodiscard]] std::expected<bool, StarException> operator()(Leaf const& leaf) {
      if (auto written = Star::print("[{}] ", leaf.self); !written)
        return std::unexpected(std::move(written.error()));
      for (Leaf::Element e : leaf.elements) {
        if (auto written = Star::print("({}) {} ", e.key, e.data); !written)
          return std::unexpected(std::move(written.error()));
      }
      if (auto written = Star::print("\n"); !written)
        return std::unexpected(std::move(written.error()));
      return true;
    }
  };
};

const int RandFactor = 0xd5a2f037;
const std::size_t TestCount = 500;
const std::size_t WriteRepeat = 3;
const std::size_t ShrinkCount = 5;

String gen_value(int k) {
  return Star::to_string(k * RandFactor);
}

bool check_value(int k, String v) {
  return gen_value(k) == v;
}

void put_all(SimpleBTree& db, List<int> keys) {
  for (int k : keys)
    ASSERT_TRUE(db.insert(k, gen_value(k)));
}

void check_all(SimpleBTree& db, List<int> keys) {
  for (int k : keys) {
    auto v = db.find(k);
    ASSERT_TRUE(v);
    ASSERT_TRUE(*v);
    EXPECT_TRUE(check_value(k, **v));
  }
}

[[nodiscard]] std::expected<std::size_t, StarException> erase_all(SimpleBTree& db, List<int> keys) {
  std::size_t totalRemoved = 0;
  Set<int> removed;
  for (int k : keys) {
    auto result = db.erase(k);
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result) {
      EXPECT_FALSE(removed.contains(k));
      removed.add(k);
      ++totalRemoved;
    }
  }
  return totalRemoved;
}

void test_btree(std::size_t maxIndexSize, std::size_t maxLeafSize) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  std::srand(std::time(0));

  SimpleBTree db(maxIndexSize, maxLeafSize);
  ASSERT_TRUE(db.create_new_root());

  Set<int> keySet;
  while (keySet.size() < TestCount)
    keySet.add(std::rand());

  List<int> keys;
  for (int k : keySet) {
    for (std::size_t j = 0; j < WriteRepeat; ++j)
      keys.push_back(k);
  }

  // record writes/reads repeated WriteRepeat times randomly each cycle
  random.shuffle(keys);
  put_all(db, keys);

  auto record_count = db.record_count();
  ASSERT_TRUE(record_count);
  EXPECT_EQ(*record_count, TestCount);

  random.shuffle(keys);
  check_all(db, keys);

  // Random reads/writes with ShrinkCount cycles...
  for (std::size_t i = 0; i < ShrinkCount; ++i) {
    random.shuffle(keys);
    List<int> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);

    ASSERT_TRUE(erase_all(db, keysTemp));

    random.shuffle(keysTemp);
    put_all(db, keysTemp);

    random.shuffle(keysTemp);
    check_all(db, keys);
  }

  auto totalRemoved = erase_all(db, keys);
  ASSERT_TRUE(totalRemoved);
  EXPECT_EQ(*totalRemoved, TestCount);
}

TEST(BTreeTest, All) {
  test_btree(3, 2);
  test_btree(6, 6);
}

TEST(BTreeTest, TraversalWithoutLeafLinks) {
  SimpleBTree db(3, 2, false);
  ASSERT_TRUE(db.create_new_root());
  for (int k = 0; k < 32; ++k)
    ASSERT_TRUE(db.insert(k, gen_value(k)));

  auto records = db.find(5, 23);
  ASSERT_TRUE(records);
  ASSERT_EQ(records->size(), 18u);
  for (std::size_t i = 0; i < records->size(); ++i) {
    EXPECT_EQ((*records)[i].first, static_cast<int>(i) + 5);
    EXPECT_EQ((*records)[i].second, gen_value(static_cast<int>(i) + 5));
  }

  int nextKey = 0;
  ASSERT_TRUE(db.for_all([&](int k, String const& data) -> std::expected<void, StarException> {
    EXPECT_EQ(k, nextKey++);
    EXPECT_EQ(data, gen_value(k));
    return {};
  }));
  EXPECT_EQ(nextKey, 32);

  // Deferred node deletion is applied by set_new_root, including root shrink.
  for (int k = 0; k < 32; ++k) {
    auto removed = db.erase(k);
    ASSERT_TRUE(removed);
    EXPECT_TRUE(*removed);
  }
  auto levels = db.index_levels();
  ASSERT_TRUE(levels);
  EXPECT_EQ(*levels, 0u);
  auto count = db.record_count();
  ASSERT_TRUE(count);
  EXPECT_EQ(*count, 0u);
}

struct OwnedReadBTreeStorage : SimpleBTreeStorage {
  using SimpleBTreeStorage::SimpleBTreeStorage;

  Key index_key_before(Index const& index, std::size_t i) { return index.key_before(i); }
  Key leaf_key(Leaf const& leaf, std::size_t i) { return leaf.key(i); }
  Data leaf_data(Leaf const& leaf, std::size_t i) { return leaf.data(i); }
};

struct LessOnlyKey {
  int value = 0;

  friend bool operator<(LessOnlyKey const& left, LessOnlyKey const& right) {
    return left.value < right.value;
  }
};

static_assert(!std::totally_ordered<LessOnlyKey>);
static_assert(btree_storage<InMemoryBTreeStorage<LessOnlyKey>>);
static_assert(btree_storage<OwnedReadBTreeStorage>);

struct MutableRecordVisitor {
  MutableRecordVisitor() = default;
  MutableRecordVisitor(MutableRecordVisitor const&) = delete;
  MutableRecordVisitor& operator=(MutableRecordVisitor const&) = delete;

  std::expected<void, StarException> operator()(int const& key, String const& data) & {
    keys.push_back(key);
    EXPECT_EQ(data, gen_value(key));
    if (fail_after && keys.size() == *fail_after)
      return std::unexpected(StarException("record visitor marker"));
    return {};
  }

  std::vector<int> keys;
  std::optional<std::size_t> fail_after;
};

struct MutableErrorHandler {
  MutableErrorHandler() = default;
  MutableErrorHandler(MutableErrorHandler const&) = delete;
  MutableErrorHandler& operator=(MutableErrorHandler const&) = delete;

  std::expected<void, StarException> operator()(std::string_view message, ExceptionDiagnostic const&) & {
    messages.emplace_back(message);
    if (fail)
      return std::unexpected(StarException("recovery handler marker"));
    return {};
  }

  std::vector<std::string> messages;
  bool fail = false;
};

static_assert(btree_record_visitor<MutableRecordVisitor, int, String>);
static_assert(btree_error_handler<MutableErrorHandler>);
static_assert(!btree_record_visitor<decltype([](int, String const&) { return true; }), int, String>);
static_assert(!btree_record_visitor<decltype([](int, String const&) {}), int, String>);
static_assert(!btree_error_handler<decltype([](std::string_view, ExceptionDiagnostic const&) { return true; })>);

struct MutableNodeVisitor {
  MutableNodeVisitor() = default;
  MutableNodeVisitor(MutableNodeVisitor const&) = delete;
  MutableNodeVisitor& operator=(MutableNodeVisitor const&) = delete;

  std::expected<bool, StarException> operator()(SimpleBTree::Index const& index) & {
    indexes.push_back(index);
    return !pruned_index || index.self != *pruned_index;
  }

  std::expected<bool, StarException> operator()(SimpleBTree::Leaf const& leaf) & {
    leaves.push_back(leaf);
    if (fail_leaf)
      return std::unexpected(StarException("node visitor marker"));
    return !prune_leaf_siblings;
  }

  std::vector<SimpleBTree::Index> indexes;
  std::vector<SimpleBTree::Leaf> leaves;
  std::optional<int> pruned_index;
  bool prune_leaf_siblings = false;
  bool fail_leaf = false;
};

static_assert(btree_node_visitor<MutableNodeVisitor, SimpleBTree::Index, SimpleBTree::Leaf>);
static_assert(!btree_node_visitor<MutableRecordVisitor, SimpleBTree::Index, SimpleBTree::Leaf>);

TEST(BTreeTest, RootLeafCountsAndShrink) {
  for (bool leaf_links : {false, true}) {
    SimpleBTree db(3, 2, leaf_links);
    ASSERT_TRUE(db.create_new_root());
    auto check_root_leaf = [&](std::uint64_t records) {
      auto leaves = db.leaf_count();
      auto indexes = db.index_count();
      auto levels = db.index_levels();
      auto count = db.record_count();
      ASSERT_TRUE(leaves);
      ASSERT_TRUE(indexes);
      ASSERT_TRUE(levels);
      ASSERT_TRUE(count);
      EXPECT_EQ(*leaves, 1u);
      EXPECT_EQ(*indexes, 0u);
      EXPECT_EQ(*levels, 0u);
      EXPECT_EQ(*count, records);
    };
    check_root_leaf(0);
    ASSERT_TRUE(db.insert(0, gen_value(0)));
    check_root_leaf(1);
    for (int key = 1; key < 32; ++key)
      ASSERT_TRUE(db.insert(key, gen_value(key)));
    auto levels = db.index_levels();
    ASSERT_TRUE(levels);
    ASSERT_GT(*levels, 1u);
    auto leaves = db.leaf_count();
    auto indexes = db.index_count();
    ASSERT_TRUE(leaves);
    ASSERT_TRUE(indexes);
    MutableNodeVisitor nodes;
    ASSERT_TRUE(db.for_all_nodes(nodes));
    EXPECT_EQ(*leaves, nodes.leaves.size());
    EXPECT_EQ(*indexes, nodes.indexes.size());
    for (int key = 0; key < 31; ++key) {
      auto erased = db.erase(key);
      ASSERT_TRUE(erased);
      EXPECT_TRUE(*erased);
    }
    check_root_leaf(1);
    auto erased = db.erase(31);
    ASSERT_TRUE(erased);
    EXPECT_TRUE(*erased);
    check_root_leaf(0);
    erased = db.erase(31);
    ASSERT_TRUE(erased);
    EXPECT_FALSE(*erased);
    check_root_leaf(0);
  }
}

TEST(BTreeTest, NodeFalsePrunesLocallyAndErrorsPropagate) {
  SimpleBTree db(3, 2);
  ASSERT_TRUE(db.create_new_root());
  for (int key = 0; key < 64; ++key)
    ASSERT_TRUE(db.insert(key, gen_value(key)));

  MutableNodeVisitor all;
  ASSERT_TRUE(db.for_all_nodes(all));
  ASSERT_FALSE(all.indexes.empty());
  ASSERT_GT(all.indexes.front().level, 0u);

  MutableNodeVisitor prune_root;
  prune_root.pruned_index = all.indexes.front().self;
  ASSERT_TRUE(db.for_all_nodes(prune_root));
  EXPECT_EQ(prune_root.indexes.size(), 1u);
  EXPECT_TRUE(prune_root.leaves.empty());

  auto lowest = std::ranges::find_if(all.indexes, [](auto const& index) { return index.level == 0; });
  ASSERT_NE(lowest, all.indexes.end());
  std::vector<int> expected_leaves;
  for (auto const& leaf : all.leaves) {
    bool pruned = false;
    for (std::size_t i = 0; i < lowest->pointer_count(); ++i)
      pruned |= leaf.self == lowest->pointer(i);
    if (!pruned)
      expected_leaves.push_back(leaf.self);
  }
  ASSERT_FALSE(expected_leaves.empty());
  MutableNodeVisitor prune_index;
  prune_index.pruned_index = lowest->self;
  ASSERT_TRUE(db.for_all_nodes(prune_index));
  std::vector<int> actual_leaves;
  for (auto const& leaf : prune_index.leaves)
    actual_leaves.push_back(leaf.self);
  EXPECT_EQ(actual_leaves, expected_leaves);
  EXPECT_EQ(prune_index.indexes.size(), all.indexes.size());

  // A leaf false skips its siblings, not the remaining ancestor subtrees.
  expected_leaves.clear();
  for (auto const& index : all.indexes) {
    if (index.level == 0)
      expected_leaves.push_back(index.pointer(0));
  }
  ASSERT_GT(expected_leaves.size(), 1u);
  MutableNodeVisitor prune_leaves;
  prune_leaves.prune_leaf_siblings = true;
  ASSERT_TRUE(db.for_all_nodes(prune_leaves));
  actual_leaves.clear();
  for (auto const& leaf : prune_leaves.leaves)
    actual_leaves.push_back(leaf.self);
  EXPECT_EQ(actual_leaves, expected_leaves);
  EXPECT_EQ(prune_leaves.indexes.size(), all.indexes.size());

  MutableNodeVisitor fail_leaf;
  fail_leaf.fail_leaf = true;
  auto failed = db.for_all_nodes(fail_leaf);
  ASSERT_FALSE(failed);
  EXPECT_NE(std::string_view(failed.error().what()).find("node visitor marker"), std::string_view::npos);
  EXPECT_EQ(fail_leaf.leaves.size(), 1u);
  EXPECT_LT(fail_leaf.indexes.size(), all.indexes.size());
}

template <btree_storage Storage>
void check_borrowed_callbacks() {
  BTree<Storage> db(std::in_place, 3, 2);
  ASSERT_TRUE(db.create_new_root());
  for (int key = 0; key < 16; ++key)
    ASSERT_TRUE(db.insert(key, gen_value(key)));

  MutableRecordVisitor all;
  ASSERT_TRUE(db.for_all(all));
  ASSERT_EQ(all.keys.size(), 16u);
  for (int key = 0; key < 16; ++key)
    EXPECT_EQ(all.keys[key], key);

  MutableRecordVisitor range;
  ASSERT_TRUE(db.for_each(3, 9, range));
  EXPECT_EQ(range.keys, (std::vector<int>{3, 4, 5, 6, 7, 8}));

  int temporary_calls = 0;
  ASSERT_TRUE(db.for_all([state = std::make_unique<int>(0), &temporary_calls](int const&, String const&) mutable
      -> std::expected<void, StarException> {
    temporary_calls = ++*state;
    return {};
  }));
  EXPECT_EQ(temporary_calls, 16);

  MutableRecordVisitor fail_all;
  fail_all.fail_after = 2;
  auto failed = db.for_all(fail_all);
  ASSERT_FALSE(failed);
  EXPECT_EQ(fail_all.keys, (std::vector<int>{0, 1}));
  EXPECT_NE(std::string_view(failed.error().what()).find("record visitor marker"), std::string_view::npos);
  MutableRecordVisitor fail_range;
  fail_range.fail_after = 2;
  failed = db.for_each(3, 9, fail_range);
  ASSERT_FALSE(failed);
  EXPECT_EQ(fail_range.keys, (std::vector<int>{3, 4}));
  EXPECT_NE(std::string_view(failed.error().what()).find("record visitor marker"), std::string_view::npos);

  MutableRecordVisitor recovered;
  MutableErrorHandler errors;
  ASSERT_TRUE(db.recover_all(recovered, errors));
  EXPECT_EQ(recovered.keys, all.keys);
  EXPECT_TRUE(errors.messages.empty());

  MutableRecordVisitor fail_recovery;
  fail_recovery.fail_after = 1;
  MutableErrorHandler fail_handler;
  fail_handler.fail = true;
  failed = db.recover_all(fail_recovery, fail_handler);
  ASSERT_FALSE(failed);
  EXPECT_EQ(fail_recovery.keys, (std::vector<int>{0}));
  EXPECT_EQ(fail_handler.messages, (std::vector<std::string>{"Error reading leaf node"}));
  EXPECT_NE(std::string_view(failed.error().what()).find("recovery handler marker"), std::string_view::npos);

  // Suppressing a record error skips the rest of that leaf, then continues.
  MutableRecordVisitor suppressed;
  suppressed.fail_after = 1;
  MutableErrorHandler suppress_handler;
  ASSERT_TRUE(db.recover_all(suppressed, suppress_handler));
  EXPECT_EQ(suppress_handler.messages, (std::vector<std::string>{"Error reading leaf node"}));
  ASSERT_FALSE(suppressed.keys.empty());
  EXPECT_EQ(suppressed.keys.front(), 0);
  EXPECT_EQ(suppressed.keys.back(), 15);
  EXPECT_LT(suppressed.keys.size(), all.keys.size());

  BTree<Storage> missing_root(std::in_place, 3, 2);
  MutableRecordVisitor missing_records;
  MutableErrorHandler missing_handler;
  missing_handler.fail = true;
  failed = missing_root.recover_all(missing_records, missing_handler);
  ASSERT_FALSE(failed);
  EXPECT_TRUE(missing_records.keys.empty());
  EXPECT_EQ(missing_handler.messages, (std::vector<std::string>{"Error loading root index or leaf node"}));
  EXPECT_NE(std::string_view(failed.error().what()).find("recovery handler marker"), std::string_view::npos);
}

TEST(BTreeTest, BorrowedMutableNoncopyableCallbacks) {
  check_borrowed_callbacks<SimpleBTreeStorage>();
  check_borrowed_callbacks<OwnedReadBTreeStorage>();
}

template <btree_storage Storage>
void check_ordered_boundaries() {
  using Key = typename Storage::Key;
  auto key_value = [](Key const& key) {
    if constexpr (std::same_as<Key, int>)
      return key;
    else
      return key.value;
  };
  for (bool leaf_links : {false, true}) {
    BTree<Storage> db(std::in_place, 3, 2, leaf_links);
    ASSERT_TRUE(db.create_new_root());
    auto empty = db.find(Key{-100}, Key{100});
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
    for (int value : {12, -12, 28, 0, 20, -4, 4, 24, -8, 16, 8}) {
      auto inserted = db.insert(Key{value}, gen_value(value));
      ASSERT_TRUE(inserted);
      EXPECT_FALSE(*inserted);
    }
    auto overwritten = db.insert(Key{12}, gen_value(12));
    ASSERT_TRUE(overwritten);
    EXPECT_TRUE(*overwritten);
    for (int value = -15; value <= 31; ++value) {
      bool present = value >= -12 && value <= 28 && value % 4 == 0;
      auto contains = db.contains(Key{value});
      auto found = db.find(Key{value});
      ASSERT_TRUE(contains);
      ASSERT_TRUE(found);
      EXPECT_EQ(*contains, present);
      ASSERT_EQ(found->has_value(), present);
      if (present)
        EXPECT_EQ(**found, gen_value(value));
      for (int upper = value; upper <= 32; ++upper) {
        auto records = db.find(Key{value}, Key{upper});
        ASSERT_TRUE(records);
        std::size_t position = 0;
        for (int expected = -12; expected <= 28; expected += 4) {
          if (expected >= value && expected < upper) {
            ASSERT_LT(position, records->size());
            EXPECT_EQ(key_value((*records)[position].first), expected);
            EXPECT_EQ((*records)[position].second, gen_value(expected));
            ++position;
          }
        }
        EXPECT_EQ(records->size(), position);
      }
    }

    auto all = db.find(Key{std::numeric_limits<int>::min()}, Key{std::numeric_limits<int>::max()});
    ASSERT_TRUE(all);
    EXPECT_EQ(all->size(), 11u);
    auto owned = db.find(Key{0});
    ASSERT_TRUE(owned);
    ASSERT_TRUE(*owned);
    auto erased = db.erase(Key{-4}, Key{13});
    ASSERT_TRUE(erased);
    ASSERT_EQ(erased->size(), 5u);
    for (std::size_t i = 0; i < erased->size(); ++i) {
      int value = -4 + static_cast<int>(i) * 4;
      EXPECT_EQ(key_value((*erased)[i].first), value);
      EXPECT_EQ((*erased)[i].second, gen_value(value));
      auto contains = db.contains(Key{value});
      ASSERT_TRUE(contains);
      EXPECT_FALSE(*contains);
    }
    ASSERT_TRUE(db.insert(Key{0}, "replacement"));
    // Point/range results remain owned across mutations and node replacement.
    EXPECT_EQ(**owned, gen_value(0));
    EXPECT_EQ((*erased)[1].second, gen_value(0));
    ASSERT_EQ(all->size(), 11u);
    EXPECT_EQ(key_value((*all)[3].first), 0);
    EXPECT_EQ((*all)[3].second, gen_value(0));
  }
}

TEST(BTreeTest, OrderedBoundariesWithOwnedAndBorrowedReads) {
  check_ordered_boundaries<SimpleBTreeStorage>();
  check_ordered_boundaries<OwnedReadBTreeStorage>();
  check_ordered_boundaries<InMemoryBTreeStorage<LessOnlyKey>>();
}

template <typename Visitor>
concept accepts_btree_record_visitor = requires(SimpleBTree& tree, Visitor& visitor) {
  tree.for_all(visitor);
  tree.for_each(0, 1, visitor);
};

template <typename ErrorHandler>
concept accepts_btree_error_handler = requires(SimpleBTree& tree,
    MutableRecordVisitor& visitor, ErrorHandler& error) {
  tree.recover_all(visitor, error);
};

template <typename Visitor>
concept accepts_btree_node_visitor = requires(SimpleBTree& tree, Visitor& visitor) {
  tree.for_all_nodes(visitor);
};

static_assert(accepts_btree_record_visitor<MutableRecordVisitor>);
static_assert(!accepts_btree_record_visitor<decltype([](int, String const&) { return true; })>);
static_assert(accepts_btree_error_handler<MutableErrorHandler>);
static_assert(!accepts_btree_error_handler<decltype([](std::string_view, ExceptionDiagnostic const&) {})>);
static_assert(accepts_btree_node_visitor<MutableNodeVisitor>);
static_assert(!accepts_btree_node_visitor<MutableRecordVisitor>);
