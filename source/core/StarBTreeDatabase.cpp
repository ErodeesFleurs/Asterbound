#include <stdio.h>

import std;
import star.string;
import star.list;
import star.io_device;
import star.memory;
import star.format;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.logging;

import star.sha256;

import star.vlq;

import star.byte_array;
import star.algorithm;
namespace Star {

  BTreeDatabase::BTreeDatabase(Logger& logger) : m_logger(logger), m_impl(std::in_place, *this) {
    m_open = false;
    m_deviceSize = 0;
    m_blockSize = 2048;
    m_headFreeIndexBlock = InvalidBlockIndex;
    m_keySize = 0;
    m_autoCommit = true;
    m_indexCache.set_capacity(64);
    m_root = InvalidBlockIndex;
    m_rootIsLeaf = false;
    m_usingAltRoot = false;
  }

  BTreeDatabase::BTreeDatabase(Logger& logger, String const& content_identifier, std::size_t key_size)
  : BTreeDatabase(logger) {
    m_contentIdentifier = content_identifier;
    m_keySize = key_size;
  }

  BTreeDatabase::~BTreeDatabase() noexcept {
    // Destruction cannot report a failed transaction to its caller.  In
    // particular, never retry a failed close/commit while unwinding.
    if (m_open && (!m_availableBlocks.empty() || !m_uncommitted.empty() || !m_uncommittedWrites.empty())) {
      if (auto written = Star::print(stderr, "BTreeDatabase '{}' destroyed with uncommitted changes; call commit() or close() explicitly\n",
    m_contentIdentifier.utf8_data()); !written)
        std::_Exit(1);
    }
  }

  std::uint32_t BTreeDatabase::block_size() const {
    std::shared_lock readLocker(m_lock);
    return m_blockSize;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_block_size(std::uint32_t block_size) {
    std::unique_lock writeLocker(m_lock);
    if (auto state_checked = check_if_open("set_block_size", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_blockSize = block_size;
    return {};
  }

  std::uint32_t BTreeDatabase::key_size() const {
    std::shared_lock readLocker(m_lock);
    return m_keySize;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_key_size(std::uint32_t key_size) {
    std::unique_lock writeLocker(m_lock);
    if (auto state_checked = check_if_open("set_key_size", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_keySize = key_size;
    return {};
  }

  String BTreeDatabase::content_identifier() const {
    std::shared_lock readLocker(m_lock);
    return m_contentIdentifier;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_content_identifier(String content_identifier) {
    std::unique_lock writeLocker(m_lock);
    if (auto state_checked = check_if_open("set_content_identifier", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_contentIdentifier = std::move(content_identifier);
    return {};
  }

  std::uint32_t BTreeDatabase::index_cache_size() const {
    std::shared_lock readLocker(m_lock);
    std::lock_guard lock(m_indexCacheSpinLock);
    return m_indexCache.capacity();
  }

  void BTreeDatabase::set_index_cache_size(std::uint32_t index_cache_size) {
    std::unique_lock writeLocker(m_lock);
    std::lock_guard lock(m_indexCacheSpinLock);
    m_indexCache.set_capacity(index_cache_size);
  }

  bool BTreeDatabase::auto_commit() const {
    std::shared_lock readLocker(m_lock);
    return m_autoCommit;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_auto_commit(bool auto_commit) {
    std::unique_lock writeLocker(m_lock);
    m_autoCommit = auto_commit;
    if (m_autoCommit)
      return do_commit();
    return {};
  }

  std::shared_ptr<IODevice> BTreeDatabase::io_device() const {
    std::shared_lock readLocker(m_lock);
    return m_device;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_io_device(std::shared_ptr<IODevice> device) {
    std::unique_lock writeLocker(m_lock);
    if (auto state_checked = check_if_open("set_io_device", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_device = std::move(device);
    return {};
  }

  bool BTreeDatabase::is_open() const {
    std::shared_lock readLocker(m_lock);
    return m_open;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::open() {
    std::unique_lock writeLocker(m_lock);
    if (m_open) {
      return false;
    }
    auto oldBlockSize = m_blockSize;
    auto oldKeySize = m_keySize;
    auto oldIdentifier = m_contentIdentifier;
    bool openedDevice = m_device && !m_device->is_open();
    std::optional<StreamOffset> initialDeviceSize;
    auto initialize = [&]() -> std::expected<bool, StarException> {

      if (!m_device) {
        return std::unexpected(DBException("BlockStorage::open called with no IODevice set"));
      }

      if (!m_device->is_open()) {
        if (auto opened = m_device->open(IOMode::ReadWrite); !opened)
          return std::unexpected(std::move(opened.error()));
      }

      auto device_size = m_device->size();
      if (!device_size) return std::unexpected(std::move(device_size.error()));
      initialDeviceSize = *device_size;
      if ((*device_size) > 0) {

        DataStreamIODevice ds(*m_device);
        if (auto positioned = ds.seek(0); !positioned)
          return std::unexpected(std::move(positioned.error()));

        auto version_magic = ds.read_bytes(VersionMagicSize);
        if (!version_magic) return std::unexpected(std::move(version_magic.error()));
        auto magic = std::move(*version_magic);
        if (magic != ByteArray(std::span(VersionMagic))) {
          return std::unexpected(DBException("Device is not a valid BTreeDatabase file"));
        }

        auto block_size = ds.read<std::uint32_t>();
        if (!block_size) return std::unexpected(std::move(block_size.error()));
        m_blockSize = (*block_size);
        if (m_blockSize < 44) {
          return std::unexpected(DBException("Invalid database block size"));
        }

        auto identifier_bytes = ds.read_bytes(ContentIdentifierStringSize);
        if (!identifier_bytes) return std::unexpected(std::move(identifier_bytes.error()));
        auto content_identifier = std::move(*identifier_bytes);
        auto identifierBytes = content_identifier.span();
        m_contentIdentifier = String(std::string_view(identifierBytes.begin(), std::ranges::find(identifierBytes, '\0')));
        auto key_size = ds.read<std::uint32_t>();
        if (!key_size) return std::unexpected(std::move(key_size.error()));
        m_keySize = (*key_size);
        if (m_keySize == 0 || m_keySize > m_blockSize - 11) {
          return std::unexpected(DBException("Invalid database key size"));
        }

        if (auto root_read = read_root(); !root_read)
          return std::unexpected(std::move(root_read.error()));

        if (m_device->is_writable()) {
          if (auto resized = m_device->resize(m_deviceSize); !resized)
            return std::unexpected(std::move(resized.error()));
        }

        return false;
      } else {

        if (m_contentIdentifier.empty()) {
          return std::unexpected(DBException("Opening new database and no content identifier set!"));
        }
        if (m_contentIdentifier.utf8_size() > ContentIdentifierStringSize) {
          return std::unexpected(DBException("content_identifier in BTreeDatabase implementation is greater than maximum identifier length"));
        }
        if (m_keySize == 0 || m_blockSize < 44 || m_keySize > m_blockSize - 11) {
          return std::unexpected(DBException("Invalid key or block size opening a new BTreeDatabase"));
        }
        m_deviceSize = HeaderSize;
        if (auto resized = m_device->resize(m_deviceSize); !resized)
          return std::unexpected(std::move(resized.error()));

        m_headFreeIndexBlock = InvalidBlockIndex;

        DataStreamIODevice ds(*m_device);
        if (auto positioned = ds.seek(0); !positioned)
          return std::unexpected(std::move(positioned.error()));

        if (auto magic_written = ds.write_data(VersionMagic); !magic_written)
          return std::unexpected(std::move(magic_written.error()));

        if (auto block_size_written = ds.write<std::uint32_t>(m_blockSize); !block_size_written)
          return std::unexpected(std::move(block_size_written.error()));

        std::array<char, ContentIdentifierStringSize> content_identifier{};
        std::ranges::copy(m_contentIdentifier.utf8(), content_identifier.begin());
        if (auto identifier_written = ds.write_data(std::span<char const>(content_identifier)); !identifier_written)
          return std::unexpected(std::move(identifier_written.error()));

        if (auto key_size_written = ds.write(m_keySize); !key_size_written)
          return std::unexpected(std::move(key_size_written.error()));

        auto createdRoot = m_impl.create_new_root();
        if (!createdRoot) return std::unexpected(std::move(createdRoot.error()));
        if (auto committed = do_commit(); !committed)
          return std::unexpected(std::move(committed.error()));

        return true;
      }
    };
    auto initialized = initialize();
    if (!initialized) {

      auto failure = std::move(initialized.error());
      if (initialDeviceSize == 0 && m_device && m_device->is_open() && m_device->is_writable()) {
        auto truncated = m_device->resize(0);
        if (!truncated)
          failure = DBException(Star::format("Opening database failed; removing the partial new database also failed: {}", truncated.error().what()), ExceptionDiagnostic(failure));
      }
      m_open = false;
      m_blockSize = oldBlockSize;
      m_keySize = oldKeySize;
      m_contentIdentifier = std::move(oldIdentifier);
      m_deviceSize = 0;
      m_root = InvalidBlockIndex;
      m_headFreeIndexBlock = InvalidBlockIndex;
      m_rootIsLeaf = false;
      m_usingAltRoot = false;
      m_availableBlocks.clear();
      m_uncommitted.clear();
      m_uncommittedWrites.clear();
      m_indexCache.clear();
      if (openedDevice && m_device->is_open()) {
        auto closed = m_device->close();
        if (!closed)
          failure = DBException(Star::format("Opening database failed; closing its device also failed: {}", closed.error().what()), ExceptionDiagnostic(failure));
      }
      return std::unexpected(std::move(failure));
    }
    m_open = true;
    return initialized;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::contains(ByteArray const& k) {
    std::shared_lock readLocker(m_lock);
    if (auto keyChecked = check_key_size(k); !keyChecked)
      return std::unexpected(std::move(keyChecked.error()));

    return m_impl.contains(k);
  }

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> BTreeDatabase::find(ByteArray const& k) {
    std::shared_lock readLocker(m_lock);
    if (auto keyChecked = check_key_size(k); !keyChecked)
      return std::unexpected(std::move(keyChecked.error()));

    return m_impl.find(k);
  }

  [[nodiscard]] std::expected<List<std::pair<ByteArray, ByteArray>>, StarException> BTreeDatabase::find(ByteArray const& lower, ByteArray const& upper) {
    std::shared_lock readLocker(m_lock);
    if (auto lowerKeyChecked = check_key_size(lower); !lowerKeyChecked)
      return std::unexpected(std::move(lowerKeyChecked.error()));

    if (auto upperKeyChecked = check_key_size(upper); !upperKeyChecked)
      return std::unexpected(std::move(upperKeyChecked.error()));

    return m_impl.find(lower, upper);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::for_each(ByteArray const& lower, ByteArray const& upper, std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto lowerKeyChecked = check_key_size(lower); !lowerKeyChecked)
      return std::unexpected(std::move(lowerKeyChecked.error()));

    if (auto upperKeyChecked = check_key_size(upper); !upperKeyChecked)
      return std::unexpected(std::move(upperKeyChecked.error()));

    return m_impl.for_each(lower, upper, std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::for_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = check_if_open("for_all", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.for_all(std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::recover_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v, std::function<std::expected<void, StarException>(String const&, ExceptionDiagnostic const&)> e) {
    if (!v || !e) {
      return std::unexpected(DBException("Missing database recovery callback"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = check_if_open("recover_all", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.recover_all(std::move(v), std::move(e));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::insert(ByteArray const& k, ByteArray const& data) {
    std::unique_lock writeLocker(m_lock);
    auto keyChecked = check_key_size(k);
    if (!keyChecked) return std::unexpected(std::move(keyChecked.error()));
    auto modified = m_impl.insert(k, data);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_autoCommit) {
      auto committed = do_commit();
      if (!committed) return std::unexpected(std::move(committed.error()));
    }
    return modified;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::erase(ByteArray const& k) {
    std::unique_lock writeLocker(m_lock);
    auto keyChecked = check_key_size(k);
    if (!keyChecked) return std::unexpected(std::move(keyChecked.error()));
    auto modified = m_impl.erase(k);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_autoCommit) {
      auto committed = do_commit();
      if (!committed) return std::unexpected(std::move(committed.error()));
    }
    return modified;
  }

  [[nodiscard]] std::expected<List<ByteArray>, StarException> BTreeDatabase::erase(ByteArray const& lower, ByteArray const& upper) {
    List<ByteArray> keys;
    auto collected = for_each(lower, upper, [&keys](ByteArray const& key, ByteArray const&) -> std::expected<void, StarException> {
      keys.push_back(key);
      return {};
    });
    if (!collected) return std::unexpected(std::move(collected.error()));
    List<ByteArray> removed;
    removed.reserve(keys.size());
    for (auto& key : keys) {
      auto erased = erase(key);
      if (!erased) return std::unexpected(std::move(erased.error()));
      if (*erased)
        removed.push_back(std::move(key));
    }
    return removed;
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> BTreeDatabase::record_count() {
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = check_if_open("record_count", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.record_count();
  }

  [[nodiscard]] std::expected<std::uint8_t, StarException> BTreeDatabase::index_levels() {
    std::shared_lock readLocker(m_lock);
    if (auto state_checked = check_if_open("index_levels", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    auto levels = m_impl.index_levels();
    if (!levels) return std::unexpected(std::move(levels.error()));
    return std::move(*levels);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::total_block_count() {
    std::shared_lock readLocker(m_lock);
    if (auto state_checked = check_if_open("total_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    auto device_size = m_device->size();
    if (!device_size) return std::unexpected(std::move(device_size.error()));
    return ((*device_size) - HeaderSize) / m_blockSize;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::free_block_count() {
    std::shared_lock readLocker(m_lock);
    if (auto state_checked = check_if_open("free_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    // Go through every FreeIndexBlock in the chain and count all of the tracked
    // free blocks.
    BlockIndex count = 0;
    BlockIndex indexBlockIndex = m_headFreeIndexBlock;
    while (indexBlockIndex != InvalidBlockIndex) {

      auto free_index = read_free_index_block(indexBlockIndex);
      if (!free_index) return std::unexpected(std::move(free_index.error()));
      FreeIndexBlock indexBlock = std::move(*free_index);
      count += 1 + indexBlock.freeBlocks.size();
      indexBlockIndex = indexBlock.nextFreeBlock;
    }

    count += m_availableBlocks.size();

    // Include untracked blocks at the end of the file in the free count.
    auto device_size = m_device->size();
    if (!device_size) return std::unexpected(std::move(device_size.error()));
    count += ((*device_size) - m_deviceSize) / m_blockSize;

    return count;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::index_block_count() {
    std::shared_lock readLocker(m_lock);
    if (auto state_checked = check_if_open("index_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    // Indexes are simply one index per block
    auto index_count = m_impl.index_count();
    if (!index_count) return std::unexpected(std::move(index_count.error()));
    return (*index_count);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::leaf_block_count() {
    std::unique_lock writeLocker(m_lock);
    if (auto state_checked = check_if_open("leaf_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    struct LeafBlocksVisitor {
      [[nodiscard]] std::expected<bool, StarException> operator()(std::shared_ptr<IndexNode> const&) {
        return true;
      }

      [[nodiscard]] std::expected<bool, StarException> operator()(std::shared_ptr<LeafNode> const& leaf) {
        auto tail_blocks = parent.leaf_tail_blocks(leaf->self);
        if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
        leaf_block_count += 1 + (*tail_blocks).size();
        return true;
      }

      BTreeDatabase& parent;
      BlockIndex leaf_block_count = 0;
    };

    LeafBlocksVisitor visitor{.parent = *this};
    if (auto visited = m_impl.for_all_nodes(visitor); !visited)
      return std::unexpected(std::move(visited.error()));

    return visitor.leaf_block_count;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::commit() {
    std::unique_lock writeLocker(m_lock);
    return do_commit();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::rollback() {
    std::unique_lock writeLocker(m_lock);
    auto stateChecked = check_if_open("rollback", true);
    if (!stateChecked) return std::unexpected(std::move(stateChecked.error()));

    m_availableBlocks.clear();
    m_indexCache.clear();
    m_uncommittedWrites.clear();
    m_uncommitted.clear();

    if (auto rootRead = read_root(); !rootRead)
      return std::unexpected(std::move(rootRead.error()));

    if (m_device->is_writable())
      return m_device->resize(m_deviceSize);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::close(bool closeDevice) {
    std::unique_lock writeLocker(m_lock);
    if (m_open) {

      auto flattened = try_flatten();
      if (!flattened) return std::unexpected(std::move(flattened.error()));
      if (!*flattened) {
        if (auto committed = do_commit(); !committed)
          return std::unexpected(std::move(committed.error()));
      }

      m_indexCache.clear();

      m_open = false;
      if (closeDevice && m_device && m_device->is_open())
        return m_device->close();
    }
    return {};
  }

  BTreeDatabase::BlockIndex const BTreeDatabase::InvalidBlockIndex;
  std::uint32_t const BTreeDatabase::HeaderSize;
  std::array<char, 8> const BTreeDatabase::VersionMagic = {'B', 'T', 'r', 'e', 'e', 'D', 'B', '5'};
  std::uint32_t const BTreeDatabase::VersionMagicSize;
  std::array<char, 2> const BTreeDatabase::IndexMagic = {'I', 'I'};
  std::array<char, 2> const BTreeDatabase::LeafMagic = {'L', 'L'};
  std::array<char, 2> const BTreeDatabase::FreeIndexMagic = {'F', 'F'};
  std::size_t const BTreeDatabase::BTreeRootSelectorBit;
  std::size_t const BTreeDatabase::BTreeRootInfoStart;
  std::size_t const BTreeDatabase::BTreeRootInfoSize;

  std::size_t BTreeDatabase::IndexNode::pointer_count() const {
    // If no begin pointer is set then the index is simply uninitialized.
    if (!beginPointer)
    return 0;
    else
    return pointers.size() + 1;
  }

  auto BTreeDatabase::IndexNode::pointer(std::size_t i) const -> BlockIndex {
    if (i == 0)
    return *beginPointer;
    else
    return pointers[i - 1].pointer;
  }

  void BTreeDatabase::IndexNode::update_pointer(std::size_t i, BlockIndex p) {
    if (i == 0)
    *beginPointer = p;
    else
    pointers[i - 1].pointer = p;
  }

  ByteArray const& BTreeDatabase::IndexNode::key_before(std::size_t i) const {
    return pointers[i - 1].key;
  }

  void BTreeDatabase::IndexNode::update_key_before(std::size_t i, ByteArray k) { pointers[i - 1].key = std::move(k); }

  void BTreeDatabase::IndexNode::erase_before(std::size_t i) {
    if (i == 0) {
      beginPointer = pointers[0].pointer;
      pointers.erase_at(0);
    } else {
      pointers.erase_at(i - 1);
    }
  }

  void BTreeDatabase::IndexNode::insert_after(std::size_t i, ByteArray k, BlockIndex p) {
    pointers.insert_at(i, Element{.key = std::move(k), .pointer = p});
  }

  std::uint8_t BTreeDatabase::IndexNode::index_level() const {
    return level;
  }

  void BTreeDatabase::IndexNode::set_index_level(std::uint8_t index_level) {
    level = index_level;
  }

  void BTreeDatabase::IndexNode::shift_left(ByteArray const& mid, IndexNode& right, std::size_t count) {
    count = std::min(right.pointer_count(), count);

    if (count == 0)
    return;

    pointers.push_back(Element{.key = mid, .pointer = *right.beginPointer});

    ElementList::iterator s = right.pointers.begin();
    std::advance(s, count - 1);
    pointers.insert(pointers.end(), std::make_move_iterator(right.pointers.begin()), std::make_move_iterator(s));

    right.pointers.erase(right.pointers.begin(), s);
    if (right.pointers.size() != 0) {
      right.beginPointer = right.pointers[0].pointer;
      right.pointers.erase_at(0);
    } else {
      right.beginPointer.reset();
    }
  }

  void BTreeDatabase::IndexNode::shift_right(ByteArray const& mid, IndexNode& left, std::size_t count) {
    count = std::min(left.pointer_count(), count);

    if (count == 0)
    return;
    --count;

    pointers.insert(pointers.begin(), Element{.key = mid, .pointer = *beginPointer});

    ElementList::iterator s = left.pointers.begin();
    std::advance(s, left.pointers.size() - count);
    pointers.insert(pointers.begin(), std::make_move_iterator(s), std::make_move_iterator(left.pointers.end()));

    left.pointers.erase(s, left.pointers.end());
    if (left.pointers.size() != 0) {
      beginPointer = left.pointers[left.pointers.size() - 1].pointer;
      left.pointers.erase_at(left.pointers.size() - 1);
    } else {
      beginPointer = std::exchange(left.beginPointer, std::nullopt);
    }
  }

  ByteArray BTreeDatabase::IndexNode::split(IndexNode& right, std::size_t i) {
    ElementList::iterator s = pointers.begin();
    std::advance(s, i - 1);

    right.beginPointer = s->pointer;
    ByteArray midKey = std::move(s->key);
    right.level = level;
    ++s;

    right.pointers.insert(right.pointers.begin(), std::make_move_iterator(s), std::make_move_iterator(pointers.end()));
    --s;

    pointers.erase(s, pointers.end());

    return midKey;
  }

  std::size_t BTreeDatabase::LeafNode::count() const {
    return elements.size();
  }

  ByteArray const& BTreeDatabase::LeafNode::key(std::size_t i) const {
    return elements[i].key;
  }

  ByteArray const& BTreeDatabase::LeafNode::data(std::size_t i) const {
    return elements[i].data;
  }

  std::uint32_t BTreeDatabase::LeafNode::element_size(Element const& element) {
    return element.key.size() + vlqUSize(element.data.size()) + element.data.size();
  }

  void BTreeDatabase::LeafNode::insert(std::size_t i, ByteArray k, ByteArray d) {
    elements.insert_at(i, Element{.key = std::move(k), .data = std::move(d)});
    encoded_size += element_size(elements[i]);
  }

  void BTreeDatabase::LeafNode::erase(std::size_t i) {
    encoded_size -= element_size(elements[i]);
    elements.erase_at(i);
  }

  void BTreeDatabase::LeafNode::shift_left(LeafNode& right, std::size_t count) {
    count = std::min(right.count(), count);

    if (count == 0)
    return;

    ElementList::iterator s = right.elements.begin();
    std::advance(s, count);

    std::uint32_t transferred_size = right.encoded_size - 6;
    if (count != right.count()) {
      transferred_size = 0;
      for (auto current = right.elements.begin(); current != s; ++current)
        transferred_size += element_size(*current);
    }
    elements.insert(elements.end(), std::make_move_iterator(right.elements.begin()), std::make_move_iterator(s));
    encoded_size += transferred_size;
    right.encoded_size -= transferred_size;
    right.elements.erase(right.elements.begin(), s);
  }

  void BTreeDatabase::LeafNode::shift_right(LeafNode& left, std::size_t count) {
    count = std::min(left.count(), count);

    if (count == 0)
    return;

    ElementList::iterator s = left.elements.begin();
    std::advance(s, left.elements.size() - count);

    std::uint32_t transferred_size = left.encoded_size - 6;
    if (count != left.count()) {
      transferred_size = 0;
      for (auto current = s; current != left.elements.end(); ++current)
        transferred_size += element_size(*current);
    }
    elements.insert(elements.begin(), std::make_move_iterator(s), std::make_move_iterator(left.elements.end()));
    encoded_size += transferred_size;
    left.encoded_size -= transferred_size;
    left.elements.erase(s, left.elements.end());
  }

  void BTreeDatabase::LeafNode::split(LeafNode& right, std::size_t i) {
    ElementList::iterator s = elements.begin();
    std::advance(s, i);

    std::uint32_t transferred_size = 0;
    for (auto current = s; current != elements.end(); ++current)
      transferred_size += element_size(*current);
    right.elements.insert(right.elements.begin(), std::make_move_iterator(s), std::make_move_iterator(elements.end()));
    encoded_size -= transferred_size;
    right.encoded_size += transferred_size;
    elements.erase(s, elements.end());
  }

  auto BTreeDatabase::BTreeImpl::root_pointer() -> Pointer {
    return parent.m_root;
  }

  bool BTreeDatabase::BTreeImpl::root_is_leaf() {
    return parent.m_rootIsLeaf;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::set_new_root(Pointer pointer, bool isLeaf) {
    parent.m_root = pointer;
    parent.m_rootIsLeaf = isLeaf;
    return {};
  }

  auto BTreeDatabase::BTreeImpl::create_index(Pointer beginPointer) -> Index {
    auto index = std::make_shared<IndexNode>();
    index->self = InvalidBlockIndex;
    index->level = 0;
    index->beginPointer = beginPointer;
    return index;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::load_index(Pointer pointer) -> std::expected<Index, StarException> {
    std::unique_lock lock(parent.m_indexCacheSpinLock);
    if (auto index = parent.m_indexCache.find_ref(pointer)) {
      return index->get();
    }
    lock.unlock();

    auto index = std::make_shared<IndexNode>();

    auto block_bytes = parent.read_block(pointer);
    if (!block_bytes) return std::unexpected(std::move(block_bytes.error()));
    DataStreamBuffer buffer(std::move(*block_bytes));

    auto magic = buffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if ((*magic) != ByteArray(std::span(IndexMagic))) {
      return std::unexpected(DBException("Error, incorrect index block signature."));
    }

    index->self = pointer;

    auto level = buffer.read<std::uint8_t>();
    if (!level) return std::unexpected(std::move(level.error()));
    index->level = (*level);
    auto pointer_count = buffer.read<std::uint32_t>();
    if (!pointer_count) return std::unexpected(std::move(pointer_count.error()));
    std::uint32_t s = (*pointer_count);
    auto begin_pointer = buffer.read<BlockIndex>();
    if (!begin_pointer) return std::unexpected(std::move(begin_pointer.error()));
    index->beginPointer = (*begin_pointer);
    if (s >= parent.max_index_pointers()) return std::unexpected(DBException("Invalid index node pointer count"));
    index->pointers.resize(s);
    for (std::uint32_t i = 0; i < s; ++i) {

      auto& e = index->pointers[i];
      auto key_bytes = buffer.read_bytes(parent.m_keySize);
      if (!key_bytes) return std::unexpected(std::move(key_bytes.error()));
      e.key = std::move(*key_bytes);
      auto child_pointer = buffer.read<BlockIndex>();
      if (!child_pointer) return std::unexpected(std::move(child_pointer.error()));
      e.pointer = (*child_pointer);
    }

    lock.lock();
    // Decode without the spinlock; only the cheap shared-owner publication is locked.
    auto cached = parent.m_indexCache.get(pointer, [&index](BlockIndex const&) { return std::move(index); });
    if (!cached) return std::unexpected(std::move(cached.error()));
    return cached->get();
  }

  bool BTreeDatabase::BTreeImpl::index_needs_shift(Index const& index) {
    return index->pointer_count() < (parent.max_index_pointers() + 1) / 2;
  }

  bool BTreeDatabase::BTreeImpl::index_shift(Index const& left, Key const& mid, Index const& right) {
    if (left->pointer_count() + right->pointer_count() <= parent.max_index_pointers()) {
      left->shift_left(mid, *right, right->pointer_count());
      return true;
    } else {
      if (index_needs_shift(right)) {
        right->shift_right(mid, *left, 1);
        return true;
      } else if (index_needs_shift(left)) {
        left->shift_left(mid, *right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  auto BTreeDatabase::BTreeImpl::index_split(Index const& index) -> std::optional<std::pair<Key, Index>> {
    if (index->pointer_count() <= parent.max_index_pointers())
    return {};

    auto right = std::make_shared<IndexNode>();
    right->self = InvalidBlockIndex;
    Key k = index->split(*right, (index->pointer_count() + 1) / 2);
    return std::make_pair(std::move(k), std::move(right));
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::store_index(Index index) -> std::expected<Pointer, StarException> {
    if (index->self != InvalidBlockIndex) {

      if (!parent.m_uncommitted.contains(index->self)) {

        parent.free_block(index->self);
        parent.m_indexCache.erase(index->self);
        index->self = InvalidBlockIndex;
      }
    }

    if (index->self == InvalidBlockIndex) {
      auto reserved = parent.reserve_block();
      if (!reserved) return std::unexpected(std::move(reserved.error()));
      index->self = (*reserved);
    }

    DataStreamBuffer buffer(parent.m_blockSize);
    if (auto magic_written = buffer.write_data(IndexMagic); !magic_written)
      return std::unexpected(std::move(magic_written.error()));

    if (auto level_written = buffer.write<std::uint8_t>(index->level); !level_written)
      return std::unexpected(std::move(level_written.error()));

    if (auto pointer_count_written = buffer.write<std::uint32_t>(index->pointers.size()); !pointer_count_written)
      return std::unexpected(std::move(pointer_count_written.error()));

    if (auto begin_pointer_written = buffer.write<BlockIndex>(*index->beginPointer); !begin_pointer_written)
      return std::unexpected(std::move(begin_pointer_written.error()));

    for (auto i = index->pointers.begin(); i != index->pointers.end(); ++i) {

      if (auto key_written = buffer.write_bytes(i->key); !key_written)
        return std::unexpected(std::move(key_written.error()));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((i->key.size() == parent.m_keySize));
      }
      if (auto pointer_written = buffer.write<BlockIndex>(i->pointer); !pointer_written)
        return std::unexpected(std::move(pointer_written.error()));

    }

    if (auto staged = parent.update_block(index->self, buffer.take_data()); !staged)
      return std::unexpected(std::move(staged.error()));

    auto cached = parent.m_indexCache.get(index->self, [&index](BlockIndex const&) { return index; });
    if (!cached) return std::unexpected(std::move(cached.error()));
    cached->get() = std::move(index);
    return cached->get()->self;
  }

  void BTreeDatabase::BTreeImpl::delete_index(Index index) {
    parent.m_indexCache.erase(index->self);
    parent.free_block(index->self);
  }

  auto BTreeDatabase::BTreeImpl::create_leaf() -> Leaf {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = InvalidBlockIndex;
    return leaf;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::load_leaf(Pointer pointer) -> std::expected<Leaf, StarException> {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = pointer;

    BlockIndex currentLeafBlock = leaf->self;
    ByteArray leaf_bytes;
    leaf_bytes.resize(parent.m_blockSize);
    DataStreamBuffer leafBuffer(std::move(leaf_bytes));
    if (auto read = parent.read_block(currentLeafBlock, 0, leafBuffer.span()); !read)
      return std::unexpected(std::move(read.error()));

    auto magic = leafBuffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if ((*magic) != ByteArray(std::span(LeafMagic))) {
      return std::unexpected(DBException("Error, incorrect leaf block signature."));
    }

    DataStreamFunctions leafInput([&](std::span<char> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (left > 0) {

        auto position = leafBuffer.pos();
        if (!position) return std::unexpected(std::move(position.error()));
        if ((*position) + left < parent.m_blockSize - sizeof(BlockIndex)) {

          if (auto read = leafBuffer.read_data(data.subspan(pos, left)); !read)
            return std::unexpected(std::move(read.error()));

          pos += left;
          left = 0;
        } else {

          auto tail_position = leafBuffer.pos();
          if (!tail_position) return std::unexpected(std::move(tail_position.error()));
          std::size_t toRead = parent.m_blockSize - sizeof(BlockIndex) - (*tail_position);
          if (auto read = leafBuffer.read_data(data.subspan(pos, toRead)); !read)
            return std::unexpected(std::move(read.error()));

          pos += toRead;
          left -= toRead;
        }

        auto next_position = leafBuffer.pos();
        if (!next_position) return std::unexpected(std::move(next_position.error()));
        if ((*next_position) == (parent.m_blockSize - sizeof(BlockIndex)) && left > 0) {

          auto next_block = leafBuffer.read<BlockIndex>();
          if (!next_block) return std::unexpected(std::move(next_block.error()));
          currentLeafBlock = (*next_block);
          if (currentLeafBlock != InvalidBlockIndex) {

            if (auto positioned = leafBuffer.seek(0); !positioned)
              return std::unexpected(std::move(positioned.error()));
            if (auto read = parent.read_block(currentLeafBlock, 0, leafBuffer.span()); !read)
              return std::unexpected(std::move(read.error()));

            auto magic = leafBuffer.read_bytes(2);
            if (!magic) return std::unexpected(std::move(magic.error()));
            if ((*magic) != ByteArray(std::span(LeafMagic))) {
              return std::unexpected(DBException("Error, incorrect leaf block signature."));
            }
          } else {

            return std::unexpected(DBException("Leaf read off end of Leaf list."));
          }
        }
      }

      return data.size();
    }, {});

    auto element_count = leafInput.read<std::uint32_t>();
    if (!element_count) return std::unexpected(std::move(element_count.error()));
    std::uint32_t count = (*element_count);
    if (count > static_cast<std::uint64_t>(parent.m_deviceSize - HeaderSize) / (parent.m_keySize + 1))
    return std::unexpected(DBException("Invalid leaf element count"));
    leaf->elements.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {

      auto& element = leaf->elements[i];
      auto key_bytes = leafInput.read_bytes(parent.m_keySize);
      if (!key_bytes) return std::unexpected(std::move(key_bytes.error()));
      element.key = std::move(*key_bytes);
      auto data_bytes = leafInput.read<ByteArray>();
      if (!data_bytes) return std::unexpected(std::move(data_bytes.error()));
      element.data = std::move(*data_bytes);
      leaf->encoded_size += LeafNode::element_size(element);
    }

    return leaf;
  }

  bool BTreeDatabase::BTreeImpl::leaf_needs_shift(Leaf const& l) {
    return parent.leaf_size(l) < parent.m_blockSize / 2;
  }

  bool BTreeDatabase::BTreeImpl::leaf_shift(Leaf& left, Leaf& right) {
    if (left->count() == 0) {
      left->shift_left(*right, right->count());
      return true;
    }

    if (right->count() == 0)
    return true;

    std::uint32_t leftSize = parent.leaf_size(left);
    std::uint32_t rightSize = parent.leaf_size(right);
    if (leftSize + rightSize < parent.m_blockSize) {
      left->shift_left(*right, right->count());
      return true;
    }

    // TODO: Shifting algorithm is bad, could potentially want to shift more
    // than one element here.
    std::uint32_t rightBeginSize = parent.m_keySize + parent.data_size(right->elements[0].data);
    std::uint32_t leftEndSize = parent.m_keySize + parent.data_size(left->elements[left->elements.size() - 1].data);
    if (leftSize < rightSize - rightBeginSize && leftSize + rightBeginSize < parent.m_blockSize) {
      left->shift_left(*right, 1);
      return true;
    } else if (rightSize < leftSize - leftEndSize && rightSize + leftEndSize < parent.m_blockSize) {
      right->shift_right(*left, 1);
      return true;
    }

    return false;
  }

  auto BTreeDatabase::BTreeImpl::leaf_split(Leaf& leaf) -> std::optional<Leaf> {
    if (leaf->elements.size() < 2)
    return {};

    std::uint32_t size = leaf->encoded_size;
    if (size < parent.m_blockSize * 2 - 2 * sizeof(BlockIndex) - 4)
      return {};
    std::uint32_t prefix_size = 6;
    std::uint32_t boundary = 0;
    for (std::uint32_t i = 0; i < leaf->elements.size(); ++i) {
      prefix_size += parent.m_keySize;
      prefix_size += parent.data_size(leaf->elements[i].data);
      if (prefix_size > parent.m_blockSize - sizeof(BlockIndex)) {
        boundary = i;
        break;
      }
    }
    if (boundary == 0)
    boundary = 1;

    auto right = std::make_shared<LeafNode>();
    right->self = InvalidBlockIndex;
    leaf->split(*right, boundary);
    return right;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::store_leaf(Leaf leaf) -> std::expected<Pointer, StarException> {
    if (leaf->self != InvalidBlockIndex) {

      auto tail_blocks = parent.leaf_tail_blocks(leaf->self);
      if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
      List<BlockIndex> tailBlocks = std::move(*tail_blocks);
      for (std::uint32_t i = 0; i < tailBlocks.size(); ++i) {
        parent.free_block(tailBlocks[i]);
      }

      if (!parent.m_uncommitted.contains(leaf->self)) {

        parent.free_block(leaf->self);
        leaf->self = InvalidBlockIndex;
      }
    }

    if (leaf->self == InvalidBlockIndex) {
      auto reserved = parent.reserve_block();
      if (!reserved) return std::unexpected(std::move(reserved.error()));
      leaf->self = (*reserved);
    }

    BlockIndex currentLeafBlock = leaf->self;
    DataStreamBuffer leafBuffer;
    leafBuffer.reset(parent.m_blockSize);
    if (auto magic_written = leafBuffer.write_data(LeafMagic); !magic_written)
      return std::unexpected(std::move(magic_written.error()));

    DataStreamFunctions leafOutput({}, [&](std::span<char const> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (true) {
        auto bufferPosition = leafBuffer.pos();
        if (!bufferPosition) return std::unexpected(std::move(bufferPosition.error()));
        std::size_t toWrite = std::min(left, parent.m_blockSize - *bufferPosition - sizeof(BlockIndex));

        if (toWrite != 0) {
          auto written = leafBuffer.write_data(data.subspan(pos, toWrite));
          if (!written) return std::unexpected(std::move(written.error()));
          left -= toWrite;
          pos += toWrite;
        }

        if (left == 0)
          break;

        if (*bufferPosition + toWrite == parent.m_blockSize - sizeof(BlockIndex)) {
          auto nextBlock = parent.reserve_block();
          if (!nextBlock) return std::unexpected(std::move(nextBlock.error()));
          auto linked = leafBuffer.write<BlockIndex>(*nextBlock);
          if (!linked) return std::unexpected(std::move(linked.error()));
          auto updated = parent.update_block(currentLeafBlock, leafBuffer.data());
          if (!updated) return std::unexpected(std::move(updated.error()));
          currentLeafBlock = *nextBlock;
          leafBuffer.reset(parent.m_blockSize);
          auto marked = leafBuffer.write_data(LeafMagic);
          if (!marked) return std::unexpected(std::move(marked.error()));
        }
      }

      return data.size();
    });

    if (auto count_written = leafOutput.write<std::uint32_t>(leaf->elements.size()); !count_written)
      return std::unexpected(std::move(count_written.error()));

    for (LeafNode::ElementList::iterator i = leaf->elements.begin(); i != leaf->elements.end(); ++i) {

      if (auto key_written = leafOutput.write_bytes(i->key); !key_written)
        return std::unexpected(std::move(key_written.error()));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((i->key.size() == parent.m_keySize));
      }
      if (auto data_written = leafOutput.write(i->data); !data_written)
        return std::unexpected(std::move(data_written.error()));

    }

    if (auto tail_positioned = leafBuffer.seek(parent.m_blockSize - sizeof(BlockIndex)); !tail_positioned)
      return std::unexpected(std::move(tail_positioned.error()));

    if (auto tail_terminated = leafBuffer.write<BlockIndex>(InvalidBlockIndex); !tail_terminated)
      return std::unexpected(std::move(tail_terminated.error()));

    if (auto staged = parent.update_block(currentLeafBlock, leafBuffer.data()); !staged)
      return std::unexpected(std::move(staged.error()));

    return leaf->self;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::delete_leaf(Leaf leaf) {
    auto tail_blocks = parent.leaf_tail_blocks(leaf->self);
    if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
    List<BlockIndex> tailBlocks = std::move(*tail_blocks);
    for (std::uint32_t i = 0; i < tailBlocks.size(); ++i) {
      parent.free_block(tailBlocks[i]);
    }

    parent.free_block(leaf->self);
    return {};
  }

  std::size_t BTreeDatabase::BTreeImpl::index_pointer_count(Index const& index) {
    return index->pointer_count();
  }

  auto BTreeDatabase::BTreeImpl::index_pointer(Index const& index, std::size_t i) -> Pointer {
    return index->pointer(i);
  }

  void BTreeDatabase::BTreeImpl::index_update_pointer(Index& index, std::size_t i, Pointer p) {
    index->update_pointer(i, p);
  }

  auto BTreeDatabase::BTreeImpl::index_key_before(Index const& index, std::size_t i) -> Key const& {
    return index->key_before(i);
  }

  void BTreeDatabase::BTreeImpl::index_update_key_before(Index& index, std::size_t i, Key k) {
    index->update_key_before(i, std::move(k));
  }

  void BTreeDatabase::BTreeImpl::index_erase_before(Index& index, std::size_t i) {
    index->erase_before(i);
  }

  void BTreeDatabase::BTreeImpl::index_insert_after(Index& index, std::size_t i, Key k, Pointer p) {
    index->insert_after(i, std::move(k), p);
  }

  std::size_t BTreeDatabase::BTreeImpl::index_level(Index const& index) {
    return index->index_level();
  }

  void BTreeDatabase::BTreeImpl::set_index_level(Index& index, std::size_t index_level) {
    index->set_index_level(index_level);
  }

  std::size_t BTreeDatabase::BTreeImpl::leaf_element_count(Leaf const& leaf) {
    return leaf->count();
  }

  auto BTreeDatabase::BTreeImpl::leaf_key(Leaf const& leaf, std::size_t i) -> Key const& {
    return leaf->key(i);
  }

  auto BTreeDatabase::BTreeImpl::leaf_data(Leaf const& leaf, std::size_t i) -> Data const& {
    return leaf->data(i);
  }

  void BTreeDatabase::BTreeImpl::leaf_insert(Leaf& leaf, std::size_t i, Key k, Data d) {
    leaf->insert(i, std::move(k), std::move(d));
  }

  void BTreeDatabase::BTreeImpl::leaf_erase(Leaf& leaf, std::size_t i) {
    leaf->erase(i);
  }

  auto BTreeDatabase::BTreeImpl::next_leaf(Leaf const&) -> std::optional<Pointer> {
    return {};
  }

  void BTreeDatabase::BTreeImpl::set_next_leaf(Leaf&, std::optional<Pointer>) {}

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::read_block(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const {
    if (auto blockChecked = check_block_index(blockIndex); !blockChecked)
      return std::unexpected(std::move(blockChecked.error()));

    return raw_read_block(blockIndex, blockOffset, block);
  }

  [[nodiscard]] std::expected<ByteArray, StarException> BTreeDatabase::read_block(BlockIndex blockIndex) const {
    ByteArray block;
    block.resize(m_blockSize);
    if (auto read = read_block(blockIndex, 0, block.span()); !read)
      return std::unexpected(std::move(read.error()));

    return block;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::update_block(BlockIndex blockIndex, ByteArray const& block) {
    if (auto blockChecked = check_block_index(blockIndex); !blockChecked)
      return std::unexpected(std::move(blockChecked.error()));

    return raw_write_block(blockIndex, 0, block.span());
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::update_block(BlockIndex blockIndex, ByteArray&& block) {
    if (auto blockChecked = check_block_index(blockIndex); !blockChecked)
      return std::unexpected(std::move(blockChecked.error()));
    if (block.size() != m_blockSize)
      return raw_write_block(blockIndex, 0, block.span());
    m_uncommittedWrites.insert_or_assign(blockIndex, std::move(block));
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::raw_read_block(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const {
    auto size = block.size();
    if (blockOffset > m_blockSize || size > m_blockSize - blockOffset) {
      return std::unexpected(DBException::format("Read past end of block, offset: {} size {}", blockOffset, size));
    }

    if (size <= 0) {
      return {};
    }

    if (auto buffer = m_uncommittedWrites.find_ref(blockIndex)) {
      buffer->get().copy_to(block, blockOffset);
    } else {
      return m_device->read_full_absolute(
        HeaderSize + blockIndex * static_cast<StreamOffset>(m_blockSize) + blockOffset, block);
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::raw_write_block(BlockIndex blockIndex, std::size_t blockOffset, std::span<char const> block) {
    auto size = block.size();
    if (blockOffset > m_blockSize || size > m_blockSize - blockOffset) {
      return std::unexpected(DBException::format("Write past end of block, offset: {} size {}", blockOffset, size));
    }

    if (size <= 0) {
      return {};
    }

    if (blockOffset == 0 && size == m_blockSize) {
      if (auto staged = m_uncommittedWrites.find_ref(blockIndex)) {
        auto destination = staged->get().span();
        if (destination.data() != block.data())
          std::memmove(destination.data(), block.data(), size);
      } else {
        m_uncommittedWrites.emplace(blockIndex, ByteArray(block));
      }
      return {};
    }

    StreamOffset blockStart = HeaderSize + blockIndex * static_cast<StreamOffset>(m_blockSize);
    auto buffer = m_uncommittedWrites.find(blockIndex);
    if (buffer == m_uncommittedWrites.end()) {
      auto original_block = m_device->read_bytes_absolute(blockStart, m_blockSize);
      if (!original_block) return std::unexpected(std::move(original_block.error()));
      buffer = m_uncommittedWrites.emplace(blockIndex, std::move(*original_block)).first;
    }

    buffer->second.write_from(block, blockOffset);
    return {};
  }

  [[nodiscard]] auto BTreeDatabase::read_free_index_block(BlockIndex blockIndex) -> std::expected<FreeIndexBlock, StarException> {
    auto block = read_block(blockIndex);
    if (!block) return std::unexpected(std::move(block.error()));
    DataStreamReader buffer(block->span());
    auto magic = buffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if (*magic != ByteArray(std::span(FreeIndexMagic)))
      return std::unexpected(DBException::format("Internal exception! block {} missing free index block marker!", blockIndex));

    FreeIndexBlock free_index_block;
    auto next = buffer.read<BlockIndex>();
    if (!next) return std::unexpected(std::move(next.error()));
    free_index_block.nextFreeBlock = *next;
    auto count = buffer.read<std::uint32_t>();
    if (!count) return std::unexpected(std::move(count.error()));
    for (std::uint32_t i = 0; i < *count; ++i) {
      // Keep the checked block-bound diagnostic of the former field reads.
      std::size_t offset = 6 + sizeof(BlockIndex) + sizeof(BlockIndex) * static_cast<std::size_t>(i);
      if (offset > m_blockSize || sizeof(BlockIndex) > m_blockSize - offset)
        return std::unexpected(DBException::format("Read past end of block, offset: {} size {}", offset, sizeof(BlockIndex)));
      auto free_block = buffer.read<BlockIndex>();
      if (!free_block) return std::unexpected(std::move(free_block.error()));
      free_index_block.freeBlocks.push_back(*free_block);
    }
    return free_index_block;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::write_free_index_block(BlockIndex blockIndex, FreeIndexBlock indexBlock) {
    // Preserve old padding and acquire the complete old payload before staging
    // any fields, exactly as the first partial write previously did.
    auto block = read_block(blockIndex);
    if (!block) return std::unexpected(std::move(block.error()));
    DataStreamBuffer buffer(std::move(*block));
    if (auto magic = buffer.write_data(FreeIndexMagic); !magic)
      return std::unexpected(std::move(magic.error()));
    if (auto next = buffer.write<BlockIndex>(indexBlock.nextFreeBlock); !next)
      return std::unexpected(std::move(next.error()));
    if (auto count = buffer.write<std::uint32_t>(indexBlock.freeBlocks.size()); !count)
      return std::unexpected(std::move(count.error()));
    for (auto free_block : indexBlock.freeBlocks) {
      if (auto written = buffer.write<BlockIndex>(free_block); !written)
        return std::unexpected(std::move(written.error()));
    }
    return update_block(blockIndex, buffer.take_data());
  }

  std::uint32_t BTreeDatabase::leaf_size(std::shared_ptr<LeafNode> const& leaf) const {
    return leaf->encoded_size;
  }

  std::uint32_t BTreeDatabase::max_index_pointers() const {
    // 2 for magic, 1 byte for level, sizeof(BlockIndex) for beginPointer, 4
    // for size.
    return (m_blockSize - 2 - 1 - sizeof(BlockIndex) - 4) / (m_keySize + sizeof(BlockIndex)) + 1;
  }

  std::uint32_t BTreeDatabase::data_size(ByteArray const& d) const {
    return vlqUSize(d.size()) + d.size();
  }

  [[nodiscard]] auto BTreeDatabase::leaf_tail_blocks(BlockIndex leafPointer) -> std::expected<List<BlockIndex>, StarException> {
    List<BlockIndex> tailBlocks;
    DataStreamBuffer pointerBuffer(sizeof(BlockIndex));
    while (leafPointer != InvalidBlockIndex) {

      if (auto tail_read = read_block(leafPointer, m_blockSize - sizeof(BlockIndex), pointerBuffer.span().first(sizeof(BlockIndex))); !tail_read)
        return std::unexpected(std::move(tail_read.error()));

      if (auto positioned = pointerBuffer.seek(0); !positioned)
        return std::unexpected(std::move(positioned.error()));

      auto next_block = pointerBuffer.read<BlockIndex>();
      if (!next_block) return std::unexpected(std::move(next_block.error()));
      leafPointer = (*next_block);
      if (leafPointer != InvalidBlockIndex) {
        tailBlocks.push_back(leafPointer);
      }
    }
    return tailBlocks;
  }

  void BTreeDatabase::free_block(BlockIndex b) {
    m_uncommitted.remove(b);
    if (m_uncommittedWrites.contains(b))
    m_uncommittedWrites.remove(b);

    m_availableBlocks.add(b);
  }

  [[nodiscard]] auto BTreeDatabase::reserve_block() -> std::expected<BlockIndex, StarException> {
    if (m_availableBlocks.empty()) {

      if (m_headFreeIndexBlock != InvalidBlockIndex) {

        // If available, make available all the blocks in the first free index
        // block.
        auto free_index = read_free_index_block(m_headFreeIndexBlock);
        if (!free_index) return std::unexpected(std::move(free_index.error()));
        FreeIndexBlock indexBlock = std::move(*free_index);
        for (auto const& b : indexBlock.freeBlocks) {
          m_availableBlocks.add(b);
        }
        m_availableBlocks.add(m_headFreeIndexBlock);
        m_headFreeIndexBlock = indexBlock.nextFreeBlock;
      }

      if (m_availableBlocks.empty()) {

        // If we still don't have any available blocks, just add a block to the
        // end of the file.
        auto appended = make_end_block();
        if (!appended) return std::unexpected(std::move(appended.error()));
        m_availableBlocks.add((*appended));
      }
    }

    auto available = m_availableBlocks.take_first();
    if (!available) return std::unexpected(std::move(available.error()));
    BlockIndex block = (*available);
    m_uncommitted.add(block);
    return block;
  }

  [[nodiscard]] auto BTreeDatabase::make_end_block() -> std::expected<BlockIndex, StarException> {
    BlockIndex blockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    if (auto resized = m_device->resize(m_deviceSize + m_blockSize); !resized)
      return std::unexpected(std::move(resized.error()));

    m_deviceSize += m_blockSize;
    return blockCount;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::write_root() {
    DataStreamIODevice ds(*m_device);
    // commit_writes() has already synchronized staged blocks. Serialize only the
    // inactive root's 4 + 8 + 4 + 1 bytes; the other root and reserved header
    // padding must not be rewritten. Keep the existing DataStream encodings.
    if (auto rootPositioned = ds.seek(BTreeRootInfoStart + (m_usingAltRoot ? 0 : BTreeRootInfoSize)); !rootPositioned)
      return std::unexpected(std::move(rootPositioned.error()));

    static_assert(2 * sizeof(BlockIndex) + sizeof(StreamOffset) + 1 == BTreeRootInfoSize);
    std::array<char, BTreeRootInfoSize> root_info;
    std::span<char> remaining(root_info);
    DataStreamFunctions fields({},
                               [&remaining](std::span<char const> data) -> std::expected<std::size_t, StarException> {
                                 if (data.size() > remaining.size())
                                   return std::unexpected(IOException("BTree root info exceeds reserved size"));
                                 std::ranges::copy(data, remaining.begin());
                                 remaining = remaining.subspan(data.size());
                                 return data.size();
                               });

    if (auto freeIndexWritten = fields.write<BlockIndex>(m_headFreeIndexBlock); !freeIndexWritten)
      return std::unexpected(std::move(freeIndexWritten.error()));

    if (auto sizeWritten = fields.write<StreamOffset>(m_deviceSize); !sizeWritten)
      return std::unexpected(std::move(sizeWritten.error()));

    if (auto rootWritten = fields.write<BlockIndex>(m_root); !rootWritten)
      return std::unexpected(std::move(rootWritten.error()));

    if (auto leafFlagWritten = fields.write<bool>(m_rootIsLeaf); !leafFlagWritten)
      return std::unexpected(std::move(leafFlagWritten.error()));

    if (auto rootWritten = ds.write_data(root_info); !rootWritten)
      return std::unexpected(std::move(rootWritten.error()));

    // Barrier 2: the complete inactive root must be durable before selecting it.
    if (auto changesSynced = m_device->sync(); !changesSynced)
      return std::unexpected(std::move(changesSynced.error()));

    // Then switch headers by writing the single bit that switches them
    bool usingAltRoot = !m_usingAltRoot;
    if (auto selectorPositioned = ds.seek(BTreeRootSelectorBit); !selectorPositioned)
      return std::unexpected(std::move(selectorPositioned.error()));

    if (auto selectorWritten = ds.write(usingAltRoot); !selectorWritten)
      return std::unexpected(std::move(selectorWritten.error()));

    m_usingAltRoot = usingAltRoot;

    // Barrier 3: acknowledge the selector only after its own synchronization.
    // Preserve the selector update even on sync failure: durability is uncertain.
    return m_device->sync();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::read_root() {
    DataStreamIODevice ds(*m_device);
    if (auto selector_positioned = ds.seek(BTreeRootSelectorBit); !selector_positioned)
      return std::unexpected(std::move(selector_positioned.error()));

    auto selector = ds.read<bool>();
    if (!selector) return std::unexpected(std::move(selector.error()));
    auto usingAltRoot = std::move(*selector);
    if (auto root_positioned = ds.seek(BTreeRootInfoStart + (usingAltRoot ? BTreeRootInfoSize : 0)); !root_positioned)
      return std::unexpected(std::move(root_positioned.error()));

    auto free_head = ds.read<BlockIndex>();
    if (!free_head) return std::unexpected(std::move(free_head.error()));
    auto head = std::move(*free_head);
    auto committed_size = ds.read<StreamOffset>();
    if (!committed_size) return std::unexpected(std::move(committed_size.error()));
    auto size = std::move(*committed_size);
    auto root_pointer = ds.read<BlockIndex>();
    if (!root_pointer) return std::unexpected(std::move(root_pointer.error()));
    auto root = std::move(*root_pointer);
    auto leaf_flag = ds.read<bool>();
    if (!leaf_flag) return std::unexpected(std::move(leaf_flag.error()));
    auto root_is_leaf = std::move(*leaf_flag);
    auto device_size = m_device->size();
    if (!device_size) return std::unexpected(std::move(device_size.error()));
    auto deviceSize = std::move(*device_size);
    if (size < HeaderSize || size > deviceSize || (size - HeaderSize) % m_blockSize != 0) {
      return std::unexpected(DBException("Invalid database root size"));
    }
    auto blockCount = (size - HeaderSize) / m_blockSize;
    if (root >= blockCount || (head != InvalidBlockIndex && head >= blockCount)) {
      return std::unexpected(DBException("Invalid database root block"));
    }
    m_usingAltRoot = usingAltRoot;
    m_headFreeIndexBlock = head;
    m_deviceSize = size;
    m_root = root;
    m_rootIsLeaf = root_is_leaf;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::do_commit() {
    if (m_availableBlocks.empty() && m_uncommitted.empty()) {
      return {};
    }

    if (!m_availableBlocks.empty()) {

      // First, read the existing head FreeIndexBlock, if it exists
      FreeIndexBlock indexBlock = FreeIndexBlock{.nextFreeBlock = InvalidBlockIndex, .freeBlocks = {}};

      auto newBlock = [&]() -> std::expected<BlockIndex, StarException> {
        if (!m_availableBlocks.empty())
          return m_availableBlocks.take_first();
        return make_end_block();
      };

      if (m_headFreeIndexBlock != InvalidBlockIndex) {
        auto free_index = read_free_index_block(m_headFreeIndexBlock);
        if (!free_index) return std::unexpected(std::move(free_index.error()));
        indexBlock = (*free_index);
      } else {
        auto head_reserved = newBlock();
        if (!head_reserved) return std::unexpected(std::move(head_reserved.error()));
        m_headFreeIndexBlock = (*head_reserved);
      }

      // Then, we need to write all the available blocks to the FreeIndexBlock chain.
      while (true) {

        // If we have room on our current FreeIndexBlock, just add a block to it.
        if (!m_availableBlocks.empty() && indexBlock.freeBlocks.size() < max_free_index_length()) {

          auto available = m_availableBlocks.take_first();
          if (!available) return std::unexpected(std::move(available.error()));
          BlockIndex toAdd = (*available);
          indexBlock.freeBlocks.push_back(toAdd);
        } else {

          // Update the current head free index block.
          if (auto free_index_written = write_free_index_block(m_headFreeIndexBlock, indexBlock); !free_index_written)
            return std::unexpected(std::move(free_index_written.error()));

          // If we're out of blocks to free, then we're done
          if (m_availableBlocks.empty()) {
            break;
          }

          // If our head free index block is full, then
          // need to write a new head free index block.
          if (indexBlock.freeBlocks.size() >= max_free_index_length()) {

            indexBlock.nextFreeBlock = m_headFreeIndexBlock;
            indexBlock.freeBlocks.clear();

            auto head_reserved = newBlock();
            if (!head_reserved) return std::unexpected(std::move(head_reserved.error()));
            m_headFreeIndexBlock = (*head_reserved);
            if (auto free_index_written = write_free_index_block(m_headFreeIndexBlock, indexBlock); !free_index_written)
              return std::unexpected(std::move(free_index_written.error()));

          }
        }
      }
    }

    if (auto blocks_committed = commit_writes(); !blocks_committed)
      return std::unexpected(std::move(blocks_committed.error()));

    if (auto root_published = write_root(); !root_published)
      return std::unexpected(std::move(root_published.error()));

    m_uncommitted.clear();
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::commit_writes() {
    for (auto& write : m_uncommittedWrites) {
      if (auto written = m_device->write_full_absolute(HeaderSize + write.first * static_cast<StreamOffset>(m_blockSize),
                                                     write.second.span().first(m_blockSize)); !written)
        return std::unexpected(std::move(written.error()));

    }

    // Barrier 1: keep staged bytes until every block write and this sync succeed.
    // Root publication must not cross this barrier, even on a retry.
    if (auto synced = m_device->sync(); !synced)
      return std::unexpected(std::move(synced.error()));

    m_uncommittedWrites.clear();
    return {};
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::try_flatten() {
    if (m_headFreeIndexBlock == InvalidBlockIndex || m_rootIsLeaf || !m_device->is_writable()) {
      return false;
    }

    BlockIndex free_block_count = 0;
    BlockIndex indexBlockIndex = m_headFreeIndexBlock;
    while (indexBlockIndex != InvalidBlockIndex) {

      auto free_index = read_free_index_block(indexBlockIndex);
      if (!free_index) return std::unexpected(std::move(free_index.error()));
      FreeIndexBlock indexBlock = std::move(*free_index);
      free_block_count += 1 + indexBlock.freeBlocks.size();
      indexBlockIndex = indexBlock.nextFreeBlock;
    }

    BlockIndex expectedBlockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    float free = float(free_block_count) / float(expectedBlockCount);
    if (free < 0.05f) {
      return false;
    }

    auto initial_device_name = m_device->device_name();
    if (!initial_device_name) return std::unexpected(std::move(initial_device_name.error()));
    auto flattenLogged = m_logger.info("[BTreeDatabase] File '{}' is {:.2f}% free space, flattening", *initial_device_name, free * 100.f);
    if (!flattenLogged) return std::unexpected(std::move(flattenLogged.error()));

    indexBlockIndex = m_headFreeIndexBlock;
    {

      List<BlockIndex> availableBlocksList;
      do {
        auto freeIndex = read_free_index_block(indexBlockIndex);
        if (!freeIndex) return std::unexpected(std::move(freeIndex.error()));
        FreeIndexBlock indexBlock = std::move(*freeIndex);
        availableBlocksList.append_range(indexBlock.freeBlocks);
        availableBlocksList.push_back(indexBlockIndex);
        indexBlockIndex = indexBlock.nextFreeBlock;
      } while (indexBlockIndex != InvalidBlockIndex);
      m_headFreeIndexBlock = InvalidBlockIndex;

      std::sort(availableBlocksList.begin(), availableBlocksList.end(), std::less<BlockIndex>());
      for (auto& availableBlock : availableBlocksList) {
        m_availableBlocks.insert(m_availableBlocks.end(), availableBlock);
      }
    }

    BlockIndex count = 1; // 1 to include root index

    auto start = std::chrono::steady_clock::now();
    BTreeImpl storage(*this);
    auto root_index = storage.load_index(storage.root_pointer());
    if (!root_index) return std::unexpected(std::move(root_index.error()));
    auto index = std::move(*root_index);
    auto relocated = flatten_visitor(index, count);
    if (!relocated) return std::unexpected(std::move(relocated.error()));
    if ((*relocated)) {

      storage.delete_index(index);
      index->self = InvalidBlockIndex;
      auto stored_root = storage.store_index(index);
      if (!stored_root) return std::unexpected(std::move(stored_root.error()));
      m_root = (*stored_root);
    }

    m_availableBlocks.clear();
    auto flattenedSize = HeaderSize + static_cast<StreamOffset>(m_blockSize) * count;
    m_deviceSize = flattenedSize;

    m_indexCache.clear();
    if (auto blocks_committed = commit_writes(); !blocks_committed)
      return std::unexpected(std::move(blocks_committed.error()));

    if (auto root_published = write_root(); !root_published)
      return std::unexpected(std::move(root_published.error()));

    m_uncommitted.clear();

    if (auto resized = m_device->resize(flattenedSize); !resized)
      return std::unexpected(std::move(resized.error()));

    auto finished_device_name = m_device->device_name();
    if (!finished_device_name) return std::unexpected(std::move(finished_device_name.error()));
    auto finishedLogged = m_logger.info("[BTreeDatabase] Finished flattening '{}' in {:.2f} milliseconds", *finished_device_name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    if (!finishedLogged) return std::unexpected(std::move(finishedLogged.error()));
    return true;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::flatten_visitor(BTreeImpl::Index& index, BlockIndex& count) {
    BTreeImpl storage(*this);
    auto pointer_count = index->pointer_count();
    count += pointer_count;
    bool canStore = !m_availableBlocks.empty();

    bool needsStore = false;
    if (storage.index_level(index) == 0) {

      for (std::size_t i = 0; i != pointer_count; ++i) {

        auto index_pointer = index->pointer(i);
        auto tail_blocks = leaf_tail_blocks(index_pointer);
        if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
        auto tailBlocks = std::move(*tail_blocks);
        if (canStore) {

          bool leafNeedsStore = *m_availableBlocks.begin() < index_pointer;

          if (!leafNeedsStore) {
            for (std::size_t i = 0; !leafNeedsStore && i != tailBlocks.size(); ++i) {
              if (*m_availableBlocks.begin() < tailBlocks[i]) {
                leafNeedsStore = true;
              }
            }
          }

          if (leafNeedsStore) {

            auto loaded_leaf = storage.load_leaf(index_pointer);
            if (!loaded_leaf) return std::unexpected(std::move(loaded_leaf.error()));
            auto leaf = std::move(*loaded_leaf);
            if (auto deleted_leaf = storage.delete_leaf(leaf); !deleted_leaf)
              return std::unexpected(std::move(deleted_leaf.error()));

            leaf->self = InvalidBlockIndex;
            auto stored_leaf = storage.store_leaf(leaf);
            if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
            index->update_pointer(i, (*stored_leaf));
            auto new_tail_blocks = leaf_tail_blocks(leaf->self);
            if (!new_tail_blocks) return std::unexpected(std::move(new_tail_blocks.error()));
            tailBlocks = (*new_tail_blocks);
            needsStore = true;
          }
          canStore = !m_availableBlocks.empty();
        }
        count += tailBlocks.size();
      }
    } else {

      for (std::size_t i = 0; i != pointer_count; ++i) {

        auto loaded_child = storage.load_index(index->pointer(i));
        if (!loaded_child) return std::unexpected(std::move(loaded_child.error()));
        auto childIndex = std::move(*loaded_child);
        if (canStore) {

          auto relocated = flatten_visitor(childIndex, count);
          if (!relocated) return std::unexpected(std::move(relocated.error()));
          if ((*relocated)) {

            storage.delete_index(childIndex);
            childIndex->self = InvalidBlockIndex;
            auto stored_child = storage.store_index(childIndex);
            if (!stored_child) return std::unexpected(std::move(stored_child.error()));
            index->update_pointer(i, (*stored_child));
            canStore = !m_availableBlocks.empty();
            needsStore = true;
          }
        }
      }
    }
    return needsStore || (canStore && *m_availableBlocks.begin() < index->self);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_if_open(std::string_view methodName, bool shouldBeOpen) const {
    if (shouldBeOpen && !m_open) {
      return std::unexpected(DBException::format("BTreeDatabase method '{}' called when not open, must be open.", methodName));
    } else {
      if (!shouldBeOpen && m_open) {
        return std::unexpected(DBException::format("BTreeDatabase method '{}' called when open, cannot call when open.", methodName));
      }
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_block_index(std::size_t blockIndex) const {
    BlockIndex blockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    if (blockIndex >= blockCount) {
      return std::unexpected(DBException::format("blockIndex: {} out of block range", blockIndex));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_key_size(ByteArray const& k) const {
    if (auto state_checked = check_if_open("key access", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    if (k.size() != m_keySize) {
      return std::unexpected(DBException::format("Wrong key size {}", k.size()));
    }
    return {};
  }

  std::uint32_t BTreeDatabase::max_free_index_length() const {
    return (m_blockSize / sizeof(BlockIndex)) - 2 - sizeof(BlockIndex) - 4;
  }

  BTreeSha256Database::BTreeSha256Database(Logger& logger) : BTreeDatabase(logger, {}, 32) {}

  [[nodiscard]] std::expected<std::unique_ptr<BTreeSha256Database>, StarException> BTreeSha256Database::create(Logger& logger, String const& content_identifier) {
    auto database = std::make_unique<BTreeSha256Database>(logger);
    if (content_identifier.empty())
      return std::unexpected(DBException("Opening new database and no content identifier set!"));
    if (content_identifier.utf8_size() > database->ContentIdentifierStringSize)
      return std::unexpected(DBException("content_identifier in BTreeDatabase implementation is greater than maximum identifier length"));
    auto configured = database->set_content_identifier(content_identifier);
    if (!configured) return std::unexpected(std::move(configured.error()));
    return database;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::contains(ByteArray const& key) {
    return BTreeDatabase::contains(sha256(key));
  }

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> BTreeSha256Database::find(ByteArray const& key) {
    return BTreeDatabase::find(sha256(key));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::insert(ByteArray const& key, ByteArray const& value) {
    return BTreeDatabase::insert(sha256(key), value);
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::erase(ByteArray const& key) {
    return BTreeDatabase::erase(sha256(key));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::contains(String const& key) {
    return BTreeDatabase::contains(sha256(key));
  }

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> BTreeSha256Database::find(String const& key) {
    return BTreeDatabase::find(sha256(key));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::insert(String const& key, ByteArray const& value) {
    return BTreeDatabase::insert(sha256(key), value);
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::erase(String const& key) {
    return BTreeDatabase::erase(sha256(key));
  }

}
