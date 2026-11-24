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
  static constexpr std::uint32_t content_identifier_string_size = 16;

  explicit BTreeDatabase(Logger& logger);
  BTreeDatabase(Logger& logger, String const& content_identifier, std::size_t key_size);
  // Never commits.  Call commit()/close() explicitly to report save failures.
  ~BTreeDatabase() noexcept;

  // The underlying device will be allocated in "blocks" of this size.
  // The larger the block size, the larger that index and leaf nodes can be
  // before they need to be split, but it also means that more space is wasted
  // for index or leaf nodes that are not completely full.  Cannot be changed
  // once the database is opened.  Defaults to 2048.
  std::uint32_t block_size() const;
  [[nodiscard]] std::expected<void, StarException> set_block_size(std::uint32_t block_size);

  // Constant size of the database keys.  Should be much smaller than the block
  // size, cannot be changed once a database is opened.  Defaults zero, which
  // is invalid, so must be set if opening a new database.
  std::uint32_t key_size() const;
  [[nodiscard]] std::expected<void, StarException> set_key_size(std::uint32_t key_size);

  // Must be no greater than content_identifier_string_size large.  May not be
  // called when the database is opened.
  String content_identifier() const;
  [[nodiscard]] std::expected<void, StarException> set_content_identifier(String content_identifier);

  // Cache size for index nodes, defaults to 64
  std::uint32_t index_cache_size() const;
  void set_index_cache_size(std::uint32_t index_cache_size);

  // If true, very write operation will immediately result in a commit.
  // Defaults to true.
  bool auto_commit() const;
  [[nodiscard]] std::expected<void, StarException> set_auto_commit(bool auto_commit);

  std::shared_ptr<IODevice> io_device() const;
  [[nodiscard]] std::expected<void, StarException> set_io_device(std::shared_ptr<IODevice> device);

  // If an existing database is opened, this will update the key size, block
  // size, and content identifier with those from the opened database.
  // Otherwise, it will use the currently set values.  Returns true if a new
  // database was created, false if an existing database was found and opened.
  [[nodiscard]] std::expected<bool, StarException> open();

  bool is_open() const;

  [[nodiscard]] std::expected<bool, StarException> contains(ByteArray const& k);

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(ByteArray const& k);
  [[nodiscard]] std::expected<List<std::pair<ByteArray, ByteArray>>, StarException> find(ByteArray const& lower, ByteArray const& upper);

  // Visitors borrow node-owned bytes for the call and report errors through expected.
  // Calls run under the shared database lock; do not mutate, close or reconfigure
  // this database from a callback. Copy records that must outlive the call.
  [[nodiscard]] std::expected<void, StarException> for_each(ByteArray const& lower, ByteArray const& upper, std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v);
  [[nodiscard]] std::expected<void, StarException> for_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v);
  [[nodiscard]] std::expected<void, StarException> recover_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v, std::function<std::expected<void, StarException>(String const&, ExceptionDiagnostic const&)> e);

  // A failed mutation or commit leaves the transaction available for explicit
  // rollback(); it never silently discards earlier uncommitted mutations.
  // Returns true if a value was overwritten
  [[nodiscard]] std::expected<bool, StarException> insert(ByteArray const& k, ByteArray const& data);

  // Returns true if the element was found and removed
  [[nodiscard]] std::expected<bool, StarException> erase(ByteArray const& k);

  // Remove all elements in the given range, returns keys removed.
  [[nodiscard]] std::expected<List<ByteArray>, StarException> erase(ByteArray const& lower, ByteArray const& upper);

  [[nodiscard]] std::expected<std::uint64_t, StarException> record_count();

  // The depth of the index nodes in this database
  [[nodiscard]] std::expected<std::uint8_t, StarException> index_levels();

  [[nodiscard]] std::expected<std::uint32_t, StarException> total_block_count();
  [[nodiscard]] std::expected<std::uint32_t, StarException> free_block_count();
  [[nodiscard]] std::expected<std::uint32_t, StarException> index_block_count();
  [[nodiscard]] std::expected<std::uint32_t, StarException> leaf_block_count();

  [[nodiscard]] std::expected<void, StarException> commit();
  [[nodiscard]] std::expected<void, StarException> rollback();

  // Commits pending changes and optionally closes the device.  A commit
  // failure leaves the database open so the caller can report it or roll back.
  [[nodiscard]] std::expected<void, StarException> close(bool close_device = false);

private:
  using BlockIndex = std::uint32_t;
  static BlockIndex const invalid_block_index = static_cast<BlockIndex>(-1);
  static std::uint32_t const header_size = 512;

  // 8 byte magic file identifier
  static std::array<char, 8> const version_magic;
  // 2 byte leaf and index start markers.
  static std::array<char, 2> const free_index_magic;
  static std::array<char, 2> const index_magic;
  static std::array<char, 2> const leaf_magic;
  static std::size_t const btree_root_selector_bit = 32;
  static std::size_t const btree_root_info_start = 33;
  static std::size_t const btree_root_info_size = 17;

  struct FreeIndexBlock {
    BlockIndex next_free_block;
    List<BlockIndex> free_blocks;
  };

  struct IndexNode {
    std::size_t pointer_count() const;
    BlockIndex pointer(std::size_t i) const;
    void update_pointer(std::size_t i, BlockIndex p);

    ByteArray const& key_before(std::size_t i) const;
    void update_key_before(std::size_t i, ByteArray k);

    void erase_before(std::size_t i);
    void insert_after(std::size_t i, ByteArray k, BlockIndex p);

    std::uint8_t index_level() const;
    void set_index_level(std::uint8_t index_level);

    // count is number of elements to shift left *including* right's begin_pointer
    void shift_left(ByteArray const& mid, IndexNode& right, std::size_t count);

    // count is number of elements to shift right
    void shift_right(ByteArray const& mid, IndexNode& left, std::size_t count);

    // i should be index of pointer that will be the new begin_pointer of right
    // node (cannot be 0).
    ByteArray split(IndexNode& right, std::size_t i);

    struct Element {
      ByteArray key;
      BlockIndex pointer;
    };
    using ElementList = List<Element>;

    BlockIndex self;
    std::uint8_t level;
    std::optional<BlockIndex> begin_pointer;
    ElementList pointers;
  };

  struct LeafNode {
    std::size_t size() const;
    ByteArray const& key(std::size_t i) const;
    ByteArray const& data(std::size_t i) const;

    void insert(std::size_t i, ByteArray k, ByteArray d);
    void erase(std::size_t i);

    // count is number of elements to shift left
    void shift_left(LeafNode& right, std::size_t count);

    // count is number of elements to shift right
    void shift_right(LeafNode& left, std::size_t count);

    // i should be index of element that will be the new start of right node.
    // Returns right index node.
    void split(LeafNode& right, std::size_t i);

    struct Element {
      ByteArray key;
      ByteArray data;
    };
    using ElementList = List<Element>;
    static std::uint32_t element_size(Element const& element);

    BlockIndex self;
    ElementList elements;
    // Magic and element count plus the encoded key/value payload, excluding tails.
    std::uint32_t encoded_size = 6;
  };

  struct BTreeImpl {
    using Key = ByteArray;
    using Data = ByteArray;
    using Pointer = BlockIndex;

    using Index = std::shared_ptr<IndexNode>;
    using Leaf = std::shared_ptr<LeafNode>;

    explicit BTreeImpl(BTreeDatabase& database) : parent(database) {}

    Pointer root_pointer();
    bool root_is_leaf();
    [[nodiscard]] std::expected<void, StarException> set_new_root(Pointer pointer, bool is_leaf);

    Index create_index(Pointer begin_pointer);
    [[nodiscard]] std::expected<Index, StarException> load_index(Pointer pointer);
    bool index_needs_shift(Index const& index);
    bool index_shift(Index const& left, Key const& mid, Index const& right);
    std::optional<std::pair<Key, Index>> index_split(Index const& index);
    [[nodiscard]] std::expected<Pointer, StarException> store_index(Index index);
    void delete_index(Index index);

    Leaf create_leaf();
    [[nodiscard]] std::expected<Leaf, StarException> load_leaf(Pointer pointer);
    bool leaf_needs_shift(Leaf const& l);
    bool leaf_shift(Leaf& left, Leaf& right);
    std::optional<Leaf> leaf_split(Leaf& leaf);
    [[nodiscard]] std::expected<Pointer, StarException> store_leaf(Leaf leaf);
    [[nodiscard]] std::expected<void, StarException> delete_leaf(Leaf leaf);

    std::size_t index_pointer_count(Index const& index);
    Pointer index_pointer(Index const& index, std::size_t i);
    void index_update_pointer(Index& index, std::size_t i, Pointer p);
    Key const& index_key_before(Index const& index, std::size_t i);
    void index_update_key_before(Index& index, std::size_t i, Key k);
    void index_erase_before(Index& index, std::size_t i);
    void index_insert_after(Index& index, std::size_t i, Key k, Pointer p);
    std::size_t index_level(Index const& index);
    void set_index_level(Index& index, std::size_t index_level);

    std::size_t leaf_element_count(Leaf const& leaf);
    Key const& leaf_key(Leaf const& leaf, std::size_t i);
    Data const& leaf_data(Leaf const& leaf, std::size_t i);
    void leaf_insert(Leaf& leaf, std::size_t i, Key k, Data d);
    void leaf_erase(Leaf& leaf, std::size_t i);
    std::optional<Pointer> next_leaf(Leaf const& leaf);
    void set_next_leaf(Leaf& leaf, std::optional<Pointer> n);

    BTreeDatabase& parent;
  };

  [[nodiscard]] std::expected<void, StarException> read_block(BlockIndex block_index, std::size_t block_offset, std::span<char> block) const;
  [[nodiscard]] std::expected<ByteArray, StarException> read_block(BlockIndex block_index) const;
  [[nodiscard]] std::expected<void, StarException> update_block(BlockIndex block_index, ByteArray const& block);
  [[nodiscard]] std::expected<void, StarException> update_block(BlockIndex block_index, ByteArray&& block);

  [[nodiscard]] std::expected<void, StarException> raw_read_block(BlockIndex block_index, std::size_t block_offset, std::span<char> block) const;
  [[nodiscard]] std::expected<void, StarException> raw_write_block(BlockIndex block_index, std::size_t block_offset, std::span<char const> block);

  [[nodiscard]] std::expected<FreeIndexBlock, StarException> read_free_index_block(BlockIndex block_index);
  [[nodiscard]] std::expected<void, StarException> write_free_index_block(BlockIndex block_index, FreeIndexBlock const& index_block);

  std::uint32_t leaf_size(std::shared_ptr<LeafNode> const& leaf) const;
  std::uint32_t max_index_pointers() const;

  std::uint32_t data_size(ByteArray const& d) const;
  [[nodiscard]] std::expected<List<BlockIndex>, StarException> leaf_tail_blocks(BlockIndex leaf_pointer);

  void free_block(BlockIndex b);
  [[nodiscard]] std::expected<BlockIndex, StarException> reserve_block();
  [[nodiscard]] std::expected<BlockIndex, StarException> make_end_block();

  [[nodiscard]] std::expected<void, StarException> write_root();
  [[nodiscard]] std::expected<void, StarException> read_root();
  [[nodiscard]] std::expected<void, StarException> do_commit();
  [[nodiscard]] std::expected<void, StarException> commit_writes();
  [[nodiscard]] std::expected<bool, StarException> try_flatten();
  [[nodiscard]] std::expected<bool, StarException> flatten_visitor(BTreeImpl::Index& index, BlockIndex& count);

  [[nodiscard]] std::expected<void, StarException> check_if_open(std::string_view method_name, bool should_be_open) const;
  [[nodiscard]] std::expected<void, StarException> check_block_index(std::size_t block_index) const;
  [[nodiscard]] std::expected<void, StarException> check_key_size(ByteArray const& k) const;
  std::uint32_t max_free_index_length() const;

  Logger& m_logger;
  mutable ReadersWriterMutex m_lock;

  BTree<BTreeImpl> m_impl;

  std::shared_ptr<IODevice> m_device;
  bool m_open;

  std::uint32_t m_block_size;
  String m_content_identifier;
  std::uint32_t m_key_size;

  bool m_auto_commit;

  // Reading values can mutate the index cache, so the index cache is kept
  // using a different lock.  It is only necessary to acquire this lock when
  // NOT holding the main writer lock. Cache settings take the main lock first,
  // then this lock, so setters cannot race writer-only cache mutations.
  mutable SpinLock m_index_cache_spin_lock;
  LruCache<BlockIndex, std::shared_ptr<IndexNode>> m_index_cache;

  BlockIndex m_head_free_index_block;
  StreamOffset m_device_size;
  BlockIndex m_root;
  bool m_root_is_leaf;
  bool m_using_alt_root;

  // Blocks that can be freely allocated and written to without violating
  // atomic consistency.
  Set<BlockIndex> m_available_blocks;

  // Blocks that have been written in uncommitted portions of the tree.
  Set<BlockIndex> m_uncommitted;

  // Temporarily holds written data so that it can be rolled back.
  mutable Map<BlockIndex, ByteArray> m_uncommitted_writes;
};

// Version of BTreeDatabase that hashes keys with SHA-256 to produce a unique
// constant size key.
class BTreeSha256Database : private BTreeDatabase {
public:
  explicit BTreeSha256Database(Logger& logger);
  // Creates a closed database with a checked, nonempty content identifier.
  [[nodiscard]] static std::expected<std::unique_ptr<BTreeSha256Database>, StarException> create(Logger& logger, String const& content_identifier);

  // Keys can be arbitrary size, actual key is the SHA-256 checksum of the key.
  [[nodiscard]] std::expected<bool, StarException> contains(ByteArray const& key);
  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(ByteArray const& key);
  [[nodiscard]] std::expected<bool, StarException> insert(ByteArray const& key, ByteArray const& value);
  [[nodiscard]] std::expected<bool, StarException> erase(ByteArray const& key);

  // Convenience string versions of access methods.  Equivalent to the utf8
  // bytes of the string minus the null terminator.
  [[nodiscard]] std::expected<bool, StarException> contains(String const& key);
  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> find(String const& key);
  [[nodiscard]] std::expected<bool, StarException> insert(String const& key, ByteArray const& value);
  [[nodiscard]] std::expected<bool, StarException> erase(String const& key);

  using BTreeDatabase::content_identifier_string_size;
  using BTreeDatabase::block_size;
  using BTreeDatabase::set_block_size;
  using BTreeDatabase::key_size;
  [[nodiscard]] std::expected<void, StarException> reset_key_size() { return set_key_size(32); }
  using BTreeDatabase::content_identifier;
  using BTreeDatabase::set_content_identifier;
  using BTreeDatabase::index_cache_size;
  using BTreeDatabase::set_index_cache_size;
  using BTreeDatabase::auto_commit;
  using BTreeDatabase::set_auto_commit;
  using BTreeDatabase::io_device;
  using BTreeDatabase::set_io_device;
  using BTreeDatabase::open;
  using BTreeDatabase::is_open;
  using BTreeDatabase::record_count;
  using BTreeDatabase::index_levels;
  using BTreeDatabase::total_block_count;
  using BTreeDatabase::free_block_count;
  using BTreeDatabase::index_block_count;
  using BTreeDatabase::leaf_block_count;
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
