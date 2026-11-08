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
  std::size_t pointerCount() const;
  Pointer pointer(std::size_t i) const;
  void updatePointer(std::size_t i, Pointer p);

  Key const& keyBefore(std::size_t i) const;
  void updateKeyBefore(std::size_t i, Key k);

  void removeBefore(std::size_t i);
  void insertAfter(std::size_t i, Key k, Pointer p);

  std::size_t indexLevel() const;
  void setIndexLevel(std::size_t indexLevel);

  // count is number of elements to shift left *including* right's beginPointer
  void shiftLeft(Key const& mid, SimpleBTreeIndex& right, std::size_t count);

  // count is number of elements to shift right
  void shiftRight(Key const& mid, SimpleBTreeIndex& left, std::size_t count);

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
  void remove(std::size_t i);

  std::optional<Pointer> nextLeaf() const;
  void setNextLeaf(std::optional<Pointer> n);

  // count is number of elements to shift left
  void shiftLeft(SimpleBTreeLeaf& right, std::size_t count);

  // count is number of elements to shift right
  void shiftRight(SimpleBTreeLeaf& left, std::size_t count);

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
std::size_t SimpleBTreeIndex<Key, Pointer>::pointerCount() const {
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
void SimpleBTreeIndex<Key, Pointer>::updatePointer(std::size_t i, Pointer p) {
  if (i == 0)
    *beginPointer = p;
  else
    pointers[i - 1].pointer = p;
}

template <typename Key, typename Pointer>
Key const& SimpleBTreeIndex<Key, Pointer>::keyBefore(std::size_t i) const {
  return pointers[i - 1].key;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::updateKeyBefore(std::size_t i, Key k) {
  pointers[i - 1].key = k;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::removeBefore(std::size_t i) {
  if (i == 0) {
    beginPointer = pointers[0].pointer;
    pointers.erase_at(0);
  } else {
    pointers.erase_at(i - 1);
  }
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::insertAfter(std::size_t i, Key k, Pointer p) {
  pointers.insert_at(i, Element{k, p});
}

template <typename Key, typename Pointer>
std::size_t SimpleBTreeIndex<Key, Pointer>::indexLevel() const {
  return level;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::setIndexLevel(std::size_t indexLevel) {
  level = indexLevel;
}

template <typename Key, typename Pointer>
void SimpleBTreeIndex<Key, Pointer>::shiftLeft(Key const& mid, SimpleBTreeIndex& right, std::size_t count) {
  count = std::min(right.pointerCount(), count);

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
void SimpleBTreeIndex<Key, Pointer>::shiftRight(Key const& mid, SimpleBTreeIndex& left, std::size_t count) {
  count = std::min(left.pointerCount(), count);

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
void SimpleBTreeLeaf<Key, Data, Pointer>::remove(std::size_t i) {
  elements.erase_at(i);
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::shiftLeft(SimpleBTreeLeaf& right, std::size_t count) {
  count = std::min(right.count(), count);

  if (count == 0)
    return;

  typename ElementList::iterator s = right.elements.begin();
  std::advance(s, count);

  elements.insert(elements.end(), right.elements.begin(), s);
  right.elements.erase(right.elements.begin(), s);
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::shiftRight(SimpleBTreeLeaf& left, std::size_t count) {
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
std::optional<Pointer> SimpleBTreeLeaf<Key, Data, Pointer>::nextLeaf() const {
  return next;
}

template <typename Key, typename Data, typename Pointer>
void SimpleBTreeLeaf<Key, Data, Pointer>::setNextLeaf(std::optional<Pointer> n) {
  next = std::move(n);
}

// Testing BTree class that simulates storage by storing in-memory copies of
// nodes.  Used to test BTree algorithm.

struct SimpleBTreeStorage {
  typedef int Key;
  typedef String Data;
  typedef int Pointer;

  typedef SimpleBTreeIndex<int, int> Index;
  typedef SimpleBTreeLeaf<int, String, int> Leaf;

  SimpleBTreeStorage(std::size_t maxisize, std::size_t maxlsize, bool leafLinks = true)
    : root(-1), rootleaf(true), maxIndexSize(maxisize), maxLeafSize(maxlsize),
      indexId(0), leafId(0), keepLeafLinks(leafLinks) {}

  Pointer rootPointer() {
    return root;
  }

  bool rootIsLeaf() {
    return rootleaf;
  }

  [[nodiscard]] std::expected<void, StarException> setNewRoot(Pointer pointer, bool isLeaf) {
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
  Leaf createLeaf() {
    Leaf leaf;
    leaf.self = -1;
    return leaf;
  }

  [[nodiscard]] std::expected<Leaf, StarException> loadLeaf(Pointer const& pointer) {
    // To make sure to accurately test storage, always *copy* in and out
    auto leaf = leaves.get(pointer);
    if (!leaf)
      return std::unexpected(std::move(leaf.error()));
    return leaf->get();
  }

  bool leafNeedsShift(Leaf const& leaf) {
    return leaf.count() < (maxLeafSize + 1) / 2;
  }

  bool shouldAppendNewLeaf(Leaf const& leaf) {
    return maxLeafSize == 2 && leaf.count() == 2;
  }

  bool leafShift(Leaf& left, Leaf& right) {
    if (left.count() + right.count() <= maxLeafSize) {
      left.shiftLeft(right, right.count());
      return true;
    } else {
      if (leafNeedsShift(right)) {
        right.shiftRight(left, 1);
        return true;
      } else if (leafNeedsShift(left)) {
        left.shiftLeft(right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  std::optional<Leaf> leafSplit(Leaf& leaf) {
    if (leaf.count() <= maxLeafSize) {
      return {};
    } else {
      Leaf right;
      right.self = -1;

      leaf.split(right, (leaf.count() + 1) / 2);

      return right;
    }
  }

  [[nodiscard]] std::expected<Pointer, StarException> storeLeaf(Leaf leaf) {
    if (leaf.self != -1) {
      auto deleted = deleteLeaf(leaf);
      if (!deleted)
        return std::unexpected(std::move(deleted.error()));
    }

    while (leaves.contains(leafId))
      ++leafId;
    leaf.self = leafId;

    // To make sure to accurately test storage, always *copy* in and out
    leaves[leafId] = leaf;

    return leaf.self;
  }

  [[nodiscard]] std::expected<void, StarException> deleteLeaf(Leaf const& leaf) {
    deletedLeaves.push_back(leaf.self);
    return {};
  }

  // Should create new index with two pointers and one mid key.
  Index createIndex(Pointer beginPointer) {
    Index indexNode;
    indexNode.self = -1;
    indexNode.level = 0;
    indexNode.beginPointer = beginPointer;
    return indexNode;
  }

  [[nodiscard]] std::expected<Index, StarException> loadIndex(Pointer const& pointer) {
    auto index = indexes.get(pointer);
    if (!index)
      return std::unexpected(std::move(index.error()));
    return index->get();
  }

  bool indexNeedsShift(Index const& index) {
    return index.pointerCount() < (maxIndexSize + 1) / 2;
  }

  bool indexShift(Index& left, Key const& mid, Index& right) {
    if (left.pointerCount() + right.pointerCount() <= maxIndexSize) {
      left.shiftLeft(mid, right, right.pointerCount());
      return true;
    } else {
      if (indexNeedsShift(right)) {
        right.shiftRight(mid, left, 1);
        return true;
      } else if (indexNeedsShift(left)) {
        left.shiftLeft(mid, right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  std::optional<std::pair<Key, Index>> indexSplit(Index& index) {
    if (index.pointerCount() <= maxIndexSize) {
      return {};
    } else {
      Index right;
      right.self = -1;

      Key mid = index.split(right, (index.pointerCount() + 1) / 2);

      return std::make_pair(mid, right);
    }
  }

  [[nodiscard]] std::expected<Pointer, StarException> storeIndex(Index index) {
    if (index.self != -1)
      deleteIndex(index);

    while (indexes.contains(indexId))
      ++indexId;
    index.self = indexId;

    indexes[indexId] = index;

    return index.self;
  }

  void deleteIndex(Index const& index) { deletedIndexes.push_back(index.self); }

  std::size_t indexPointerCount(Index const& index) {
    return index.pointerCount();
  }

  Pointer indexPointer(Index const& index, std::size_t i) {
    return index.pointer(i);
  }

  void indexUpdatePointer(Index& index, std::size_t i, Pointer p) {
    index.updatePointer(i, p);
  }

  Key indexKeyBefore(Index const& index, std::size_t i) {
    return index.keyBefore(i);
  }

  void indexUpdateKeyBefore(Index& index, std::size_t i, Key k) {
    index.updateKeyBefore(i, k);
  }

  void indexRemoveBefore(Index& index, std::size_t i) {
    index.removeBefore(i);
  }

  void indexInsertAfter(Index& index, std::size_t i, Key k, Pointer p) {
    index.insertAfter(i, k, p);
  }

  std::size_t indexLevel(Index const& index) {
    return index.indexLevel();
  }

  void setIndexLevel(Index& index, std::size_t indexLevel) {
    index.setIndexLevel(indexLevel);
  }

  std::size_t leafElementCount(Leaf const& leaf) {
    return leaf.count();
  }

  Key leafKey(Leaf const& leaf, std::size_t i) {
    return leaf.key(i);
  }

  Data leafData(Leaf const& leaf, std::size_t i) {
    return leaf.data(i);
  }

  void leafInsert(Leaf& leaf, std::size_t i, Key k, Data d) {
    return leaf.insert(i, k, d);
  }

  void leafRemove(Leaf& leaf, std::size_t i) {
    return leaf.remove(i);
  }

  std::optional<Pointer> nextLeaf(Leaf const& leaf) {
    return keepLeafLinks ? leaf.nextLeaf() : std::nullopt;
  }

  void setNextLeaf(Leaf& leaf, std::optional<Pointer> n) {
    if (keepLeafLinks)
      leaf.setNextLeaf(n);
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

static_assert(btree_storage<SimpleBTreeStorage>);
static_assert(!std::is_base_of_v<SimpleBTreeStorage, BTree<SimpleBTreeStorage>>);

struct SimpleBTree : public BTree<SimpleBTreeStorage> {
  SimpleBTree(std::size_t maxisize, std::size_t maxlsize, bool leafLinks = true)
    : BTree(std::in_place, maxisize, maxlsize, leafLinks) {}

  [[nodiscard]] std::expected<void, StarException> print() {
    auto printed = forAllNodes(Printer());
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

String genValue(int k) {
  return Star::to_string(k * RandFactor);
}

bool checkValue(int k, String v) {
  return genValue(k) == v;
}

void putAll(SimpleBTree& db, List<int> keys) {
  for (int k : keys)
    ASSERT_TRUE(db.insert(k, genValue(k)));
}

void checkAll(SimpleBTree& db, List<int> keys) {
  for (int k : keys) {
    auto v = db.find(k);
    ASSERT_TRUE(v);
    ASSERT_TRUE(*v);
    EXPECT_TRUE(checkValue(k, **v));
  }
}

[[nodiscard]] std::expected<std::size_t, StarException> removeAll(SimpleBTree& db, List<int> keys) {
  std::size_t totalRemoved = 0;
  Set<int> removed;
  for (int k : keys) {
    auto result = db.remove(k);
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

void testBTree(std::size_t maxIndexSize, std::size_t maxLeafSize) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  std::srand(std::time(0));

  SimpleBTree db(maxIndexSize, maxLeafSize);
  ASSERT_TRUE(db.createNewRoot());

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
  putAll(db, keys);

  auto recordCount = db.recordCount();
  ASSERT_TRUE(recordCount);
  EXPECT_EQ(*recordCount, TestCount);

  random.shuffle(keys);
  checkAll(db, keys);

  // Random reads/writes with ShrinkCount cycles...
  for (std::size_t i = 0; i < ShrinkCount; ++i) {
    random.shuffle(keys);
    List<int> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);

    ASSERT_TRUE(removeAll(db, keysTemp));

    random.shuffle(keysTemp);
    putAll(db, keysTemp);

    random.shuffle(keysTemp);
    checkAll(db, keys);
  }

  auto totalRemoved = removeAll(db, keys);
  ASSERT_TRUE(totalRemoved);
  EXPECT_EQ(*totalRemoved, TestCount);
}

TEST(BTreeTest, All) {
  testBTree(3, 2);
  testBTree(6, 6);
}

TEST(BTreeTest, TraversalWithoutLeafLinks) {
  SimpleBTree db(3, 2, false);
  ASSERT_TRUE(db.createNewRoot());
  for (int k = 0; k < 32; ++k)
    ASSERT_TRUE(db.insert(k, genValue(k)));

  auto records = db.find(5, 23);
  ASSERT_TRUE(records);
  ASSERT_EQ(records->size(), 18u);
  for (std::size_t i = 0; i < records->size(); ++i) {
    EXPECT_EQ((*records)[i].first, static_cast<int>(i) + 5);
    EXPECT_EQ((*records)[i].second, genValue(static_cast<int>(i) + 5));
  }

  int nextKey = 0;
  ASSERT_TRUE(db.forAll([&](int k, String const& data) -> std::expected<void, StarException> {
    EXPECT_EQ(k, nextKey++);
    EXPECT_EQ(data, genValue(k));
    return {};
  }));
  EXPECT_EQ(nextKey, 32);

  // Deferred node deletion is applied by setNewRoot, including root shrink.
  for (int k = 0; k < 32; ++k) {
    auto removed = db.remove(k);
    ASSERT_TRUE(removed);
    EXPECT_TRUE(*removed);
  }
  auto levels = db.indexLevels();
  ASSERT_TRUE(levels);
  EXPECT_EQ(*levels, 0u);
  auto count = db.recordCount();
  ASSERT_TRUE(count);
  EXPECT_EQ(*count, 0u);
}
