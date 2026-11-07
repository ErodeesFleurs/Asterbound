module;
import std;
import star.byte_array;
import star.string;
import star.list;
import star.map;
import star.io_device;
import star.format;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.logging;

namespace Star {

struct DBExceptionTag {
  static constexpr std::string_view name() { return "DBException"; }
};
using DBException = StarError<DBExceptionTag, IOException>;

class BTreeDatabase {
public:
  static constexpr std::uint32_t ContentIdentifierStringSize = 16;

  explicit BTreeDatabase(Logger& logger);
  BTreeDatabase(Logger& logger, String const& contentIdentifier, std::size_t keySize);
  // Never commits.  Call commit()/close() explicitly to report save failures.
  ~BTreeDatabase() noexcept;

  // The underlying device will be allocated in "blocks" of this size.
  // The larger the block size, the larger that index and leaf nodes can be
  // before they need to be split, but it also means that more space is wasted
  // for index or leaf nodes that are not completely full.  Cannot be changed
  // once the database is opened.  Defaults to 2048.
  std::uint32_t blockSize() const;
  [[nodiscard]] std::expected<void, StarException> setBlockSize(std::uint32_t blockSize);

  // Constant size of the database keys.  Should be much smaller than the block
  // size, cannot be changed once a database is opened.  Defaults zero, which
  // is invalid, so must be set if opening a new database.
  std::uint32_t keySize() const;
  [[nodiscard]] std::expected<void, StarException> setKeySize(std::uint32_t keySize);

  // Must be no greater than ContentIdentifierStringSize large.  May not be
  // called when the database is opened.
  String contentIdentifier() const;
  [[nodiscard]] std::expected<void, StarException> setContentIdentifier(String contentIdentifier);

  // Cache size for index nodes, defaults to 64
  std::uint32_t indexCacheSize() const;
  void setIndexCacheSize(std::uint32_t indexCacheSize);

  // If true, very write operation will immediately result in a commit.
  // Defaults to true.
  bool autoCommit() const;
  [[nodiscard]] std::expected<void, StarException> setAutoCommit(bool autoCommit);

  std::shared_ptr<IODevice> ioDevice() const;
  [[nodiscard]] std::expected<void, StarException> setIODevice(std::shared_ptr<IODevice> device);

  // If an existing database is opened, this will update the key size, block
  // size, and content identifier with those from the opened database.
  // Otherwise, it will use the currently set values.  Returns true if a new
  // database was created, false if an existing database was found and opened.
  [[nodiscard]] std::expected<bool, StarException> open();

  bool isOpen() const;

  [[nodiscard]] std::expected<bool, StarException> contains(ByteArray const& k);

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(ByteArray const& k);
  [[nodiscard]] std::expected<List<std::pair<ByteArray, ByteArray>>, StarException> find(ByteArray const& lower, ByteArray const& upper);

  // Visitors and recovery handlers must report errors through expected.
  [[nodiscard]] std::expected<void, StarException> forEach(ByteArray const& lower, ByteArray const& upper, std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v);
  [[nodiscard]] std::expected<void, StarException> forAll(std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v);
  [[nodiscard]] std::expected<void, StarException> recoverAll(std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v, std::function<std::expected<void, StarException>(String const&, ExceptionDiagnostic const&)> e);

  // A failed mutation or commit leaves the transaction available for explicit
  // rollback(); it never silently discards earlier uncommitted mutations.
  // Returns true if a value was overwritten
  [[nodiscard]] std::expected<bool, StarException> insert(ByteArray const& k, ByteArray const& data);

  // Returns true if the element was found and removed
  [[nodiscard]] std::expected<bool, StarException> remove(ByteArray const& k);

  // Remove all elements in the given range, returns keys removed.
  [[nodiscard]] std::expected<List<ByteArray>, StarException> remove(ByteArray const& lower, ByteArray const& upper);

  [[nodiscard]] std::expected<std::uint64_t, StarException> recordCount();

  // The depth of the index nodes in this database
  [[nodiscard]] std::expected<std::uint8_t, StarException> indexLevels();

  [[nodiscard]] std::expected<std::uint32_t, StarException> totalBlockCount();
  [[nodiscard]] std::expected<std::uint32_t, StarException> freeBlockCount();
  [[nodiscard]] std::expected<std::uint32_t, StarException> indexBlockCount();
  [[nodiscard]] std::expected<std::uint32_t, StarException> leafBlockCount();

  [[nodiscard]] std::expected<void, StarException> commit();
  [[nodiscard]] std::expected<void, StarException> rollback();

  // Commits pending changes and optionally closes the device.  A commit
  // failure leaves the database open so the caller can report it or roll back.
  [[nodiscard]] std::expected<void, StarException> close(bool closeDevice = false);

private:
  using BlockIndex = std::uint32_t;
  static BlockIndex const InvalidBlockIndex = static_cast<BlockIndex>(-1);
  static std::uint32_t const HeaderSize = 512;

  // 8 byte magic file identifier
  static std::array<char, 8> const VersionMagic;
  static std::uint32_t const VersionMagicSize = 8;
  // 2 byte leaf and index start markers.
  static std::array<char, 2> const FreeIndexMagic;
  static std::array<char, 2> const IndexMagic;
  static std::array<char, 2> const LeafMagic;
  // static uint32_t const BlockMagicSize = 2;
  static std::size_t const BTreeRootSelectorBit = 32;
  static std::size_t const BTreeRootInfoStart = 33;
  static std::size_t const BTreeRootInfoSize = 17;

  struct FreeIndexBlock {
    BlockIndex nextFreeBlock;
    List<BlockIndex> freeBlocks;
  };

  struct IndexNode {
    std::size_t pointerCount() const;
    BlockIndex pointer(std::size_t i) const;
    void updatePointer(std::size_t i, BlockIndex p);

    ByteArray const& keyBefore(std::size_t i) const;
    void updateKeyBefore(std::size_t i, ByteArray k);

    void removeBefore(std::size_t i);
    void insertAfter(std::size_t i, ByteArray k, BlockIndex p);

    std::uint8_t indexLevel() const;
    void setIndexLevel(std::uint8_t indexLevel);

    // count is number of elements to shift left *including* right's beginPointer
    void shiftLeft(ByteArray const& mid, IndexNode& right, std::size_t count);

    // count is number of elements to shift right
    void shiftRight(ByteArray const& mid, IndexNode& left, std::size_t count);

    // i should be index of pointer that will be the new beginPointer of right
    // node (cannot be 0).
    ByteArray split(IndexNode& right, std::size_t i);

    struct Element {
      ByteArray key;
      BlockIndex pointer;
    };
    using ElementList = List<Element>;

    BlockIndex self;
    std::uint8_t level;
    std::optional<BlockIndex> beginPointer;
    ElementList pointers;
  };

  struct LeafNode {
    std::size_t count() const;
    ByteArray const& key(std::size_t i) const;
    ByteArray const& data(std::size_t i) const;

    void insert(std::size_t i, ByteArray k, ByteArray d);
    void remove(std::size_t i);

    // count is number of elements to shift left
    void shiftLeft(LeafNode& right, std::size_t count);

    // count is number of elements to shift right
    void shiftRight(LeafNode& left, std::size_t count);

    // i should be index of element that will be the new start of right node.
    // Returns right index node.
    void split(LeafNode& right, std::size_t i);

    struct Element {
      ByteArray key;
      ByteArray data;
    };
    using ElementList = List<Element>;

    BlockIndex self;
    ElementList elements;
  };

  struct BTreeImpl {
    using Key = ByteArray;
    using Data = ByteArray;
    using Pointer = BlockIndex;

    using Index = std::shared_ptr<IndexNode>;
    using Leaf = std::shared_ptr<LeafNode>;

    explicit BTreeImpl(BTreeDatabase& database) : parent(database) {}

    Pointer rootPointer();
    bool rootIsLeaf();
    [[nodiscard]] std::expected<void, StarException> setNewRoot(Pointer pointer, bool isLeaf);

    Index createIndex(Pointer beginPointer);
    [[nodiscard]] std::expected<Index, StarException> loadIndex(Pointer pointer);
    bool indexNeedsShift(Index const& index);
    bool indexShift(Index const& left, Key const& mid, Index const& right);
    std::optional<std::pair<Key, Index>> indexSplit(Index const& index);
    [[nodiscard]] std::expected<Pointer, StarException> storeIndex(Index index);
    void deleteIndex(Index index);

    Leaf createLeaf();
    [[nodiscard]] std::expected<Leaf, StarException> loadLeaf(Pointer pointer);
    bool leafNeedsShift(Leaf const& l);
    bool leafShift(Leaf& left, Leaf& right);
    std::optional<Leaf> leafSplit(Leaf& leaf);
    [[nodiscard]] std::expected<Pointer, StarException> storeLeaf(Leaf leaf);
    [[nodiscard]] std::expected<void, StarException> deleteLeaf(Leaf leaf);

    std::size_t indexPointerCount(Index const& index);
    Pointer indexPointer(Index const& index, std::size_t i);
    void indexUpdatePointer(Index& index, std::size_t i, Pointer p);
    Key indexKeyBefore(Index const& index, std::size_t i);
    void indexUpdateKeyBefore(Index& index, std::size_t i, Key k);
    void indexRemoveBefore(Index& index, std::size_t i);
    void indexInsertAfter(Index& index, std::size_t i, Key k, Pointer p);
    std::size_t indexLevel(Index const& index);
    void setIndexLevel(Index& index, std::size_t indexLevel);

    std::size_t leafElementCount(Leaf const& leaf);
    Key leafKey(Leaf const& leaf, std::size_t i);
    Data leafData(Leaf const& leaf, std::size_t i);
    void leafInsert(Leaf& leaf, std::size_t i, Key k, Data d);
    void leafRemove(Leaf& leaf, std::size_t i);
    std::optional<Pointer> nextLeaf(Leaf const& leaf);
    void setNextLeaf(Leaf& leaf, std::optional<Pointer> n);

    BTreeDatabase& parent;
  };

  [[nodiscard]] std::expected<void, StarException> readBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const;
  [[nodiscard]] std::expected<ByteArray, StarException> readBlock(BlockIndex blockIndex) const;
  [[nodiscard]] std::expected<void, StarException> updateBlock(BlockIndex blockIndex, ByteArray const& block);

  [[nodiscard]] std::expected<void, StarException> rawReadBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const;
  [[nodiscard]] std::expected<void, StarException> rawWriteBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char const> block);

  [[nodiscard]] std::expected<FreeIndexBlock, StarException> readFreeIndexBlock(BlockIndex blockIndex);
  [[nodiscard]] std::expected<void, StarException> writeFreeIndexBlock(BlockIndex blockIndex, FreeIndexBlock indexBlock);

  std::uint32_t leafSize(std::shared_ptr<LeafNode> const& leaf) const;
  std::uint32_t maxIndexPointers() const;

  std::uint32_t dataSize(ByteArray const& d) const;
  [[nodiscard]] std::expected<List<BlockIndex>, StarException> leafTailBlocks(BlockIndex leafPointer);

  void freeBlock(BlockIndex b);
  [[nodiscard]] std::expected<BlockIndex, StarException> reserveBlock();
  [[nodiscard]] std::expected<BlockIndex, StarException> makeEndBlock();

  [[nodiscard]] std::expected<void, StarException> writeRoot();
  [[nodiscard]] std::expected<void, StarException> readRoot();
  [[nodiscard]] std::expected<void, StarException> doCommit();
  [[nodiscard]] std::expected<void, StarException> commitWrites();
  [[nodiscard]] std::expected<bool, StarException> tryFlatten();
  [[nodiscard]] std::expected<bool, StarException> flattenVisitor(BTreeImpl::Index& index, BlockIndex& count);

  [[nodiscard]] std::expected<void, StarException> checkIfOpen(std::string_view methodName, bool shouldBeOpen) const;
  [[nodiscard]] std::expected<void, StarException> checkBlockIndex(std::size_t blockIndex) const;
  [[nodiscard]] std::expected<void, StarException> checkKeySize(ByteArray const& k) const;
  std::uint32_t maxFreeIndexLength() const;

  Logger& m_logger;
  mutable ReadersWriterMutex m_lock;

  BTree<BTreeImpl> m_impl;

  std::shared_ptr<IODevice> m_device;
  bool m_open;

  std::uint32_t m_blockSize;
  String m_contentIdentifier;
  std::uint32_t m_keySize;

  bool m_autoCommit;

  // Reading values can mutate the index cache, so the index cache is kept
  // using a different lock.  It is only necessary to acquire this lock when
  // NOT holding the main writer lock, because if the main writer lock is held
  // then no other method would be loading an index anyway.
  mutable SpinLock m_indexCacheSpinLock;
  LruCache<BlockIndex, std::shared_ptr<IndexNode>> m_indexCache;

  BlockIndex m_headFreeIndexBlock;
  StreamOffset m_deviceSize;
  BlockIndex m_root;
  bool m_rootIsLeaf;
  bool m_usingAltRoot;

  // Blocks that can be freely allocated and written to without violating
  // atomic consistency.
  Set<BlockIndex> m_availableBlocks;

  // Blocks that have been written in uncommitted portions of the tree.
  Set<BlockIndex> m_uncommitted;

  // Temporarily holds written data so that it can be rolled back.
  mutable Map<BlockIndex, ByteArray> m_uncommittedWrites;
};

// Version of BTreeDatabase that hashes keys with SHA-256 to produce a unique
// constant size key.
class BTreeSha256Database : private BTreeDatabase {
public:
  explicit BTreeSha256Database(Logger& logger);
  // Creates a closed database with a checked, nonempty content identifier.
  [[nodiscard]] static std::expected<std::unique_ptr<BTreeSha256Database>, StarException> create(Logger& logger, String const& contentIdentifier);

  // Keys can be arbitrary size, actual key is the SHA-256 checksum of the key.
  [[nodiscard]] std::expected<bool, StarException> contains(ByteArray const& key);
  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(ByteArray const& key);
  [[nodiscard]] std::expected<bool, StarException> insert(ByteArray const& key, ByteArray const& value);
  [[nodiscard]] std::expected<bool, StarException> remove(ByteArray const& key);

  // Convenience string versions of access methods.  Equivalent to the utf8
  // bytes of the string minus the null terminator.
  [[nodiscard]] std::expected<bool, StarException> contains(String const& key);
  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(String const& key);
  [[nodiscard]] std::expected<bool, StarException> insert(String const& key, ByteArray const& value);
  [[nodiscard]] std::expected<bool, StarException> remove(String const& key);

  using BTreeDatabase::ContentIdentifierStringSize;
  using BTreeDatabase::blockSize;
  using BTreeDatabase::setBlockSize;
  using BTreeDatabase::contentIdentifier;
  using BTreeDatabase::setContentIdentifier;
  using BTreeDatabase::indexCacheSize;
  using BTreeDatabase::setIndexCacheSize;
  using BTreeDatabase::autoCommit;
  using BTreeDatabase::setAutoCommit;
  using BTreeDatabase::ioDevice;
  using BTreeDatabase::setIODevice;
  using BTreeDatabase::open;
  using BTreeDatabase::isOpen;
  using BTreeDatabase::recordCount;
  using BTreeDatabase::indexLevels;
  using BTreeDatabase::totalBlockCount;
  using BTreeDatabase::freeBlockCount;
  using BTreeDatabase::indexBlockCount;
  using BTreeDatabase::leafBlockCount;
  using BTreeDatabase::commit;
  using BTreeDatabase::rollback;
  using BTreeDatabase::close;
};

}

export module star.btree_database;

export namespace Star {
  using ::Star::DBExceptionTag;
  using ::Star::DBException;
  using ::Star::BTreeDatabase;
  using ::Star::BTreeSha256Database;
}
