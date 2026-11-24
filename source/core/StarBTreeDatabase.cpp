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
    m_device_size = 0;
    m_block_size = 2048;
    m_head_free_index_block = invalid_block_index;
    m_key_size = 0;
    m_auto_commit = true;
    m_index_cache.set_capacity(64);
    m_root = invalid_block_index;
    m_root_is_leaf = false;
    m_using_alt_root = false;
  }

  BTreeDatabase::BTreeDatabase(Logger& logger, String const& content_identifier, std::size_t key_size)
  : BTreeDatabase(logger) {
    m_content_identifier = content_identifier;
    m_key_size = key_size;
  }

  BTreeDatabase::~BTreeDatabase() noexcept {
    // Destruction cannot report a failed transaction to its caller.  In
    // particular, never retry a failed close/commit while unwinding.
    if (m_open && (!m_available_blocks.empty() || !m_uncommitted.empty() || !m_uncommitted_writes.empty())) {
      if (auto written = Star::print(*stderr, "BTreeDatabase '{}' destroyed with uncommitted changes; call commit() or close() explicitly\n",
    m_content_identifier.utf8().c_str()); !written)
        std::_Exit(1);
    }
  }

  std::uint32_t BTreeDatabase::block_size() const {
    std::shared_lock read_lock(m_lock);
    return m_block_size;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_block_size(std::uint32_t block_size) {
    std::unique_lock write_lock(m_lock);
    if (auto state_checked = check_if_open("set_block_size", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_block_size = block_size;
    return {};
  }

  std::uint32_t BTreeDatabase::key_size() const {
    std::shared_lock read_lock(m_lock);
    return m_key_size;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_key_size(std::uint32_t key_size) {
    std::unique_lock write_lock(m_lock);
    if (auto state_checked = check_if_open("set_key_size", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_key_size = key_size;
    return {};
  }

  String BTreeDatabase::content_identifier() const {
    std::shared_lock read_lock(m_lock);
    return m_content_identifier;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_content_identifier(String content_identifier) {
    std::unique_lock write_lock(m_lock);
    if (auto state_checked = check_if_open("set_content_identifier", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_content_identifier = std::move(content_identifier);
    return {};
  }

  std::uint32_t BTreeDatabase::index_cache_size() const {
    std::shared_lock read_lock(m_lock);
    std::lock_guard lock(m_index_cache_spin_lock);
    return m_index_cache.capacity();
  }

  void BTreeDatabase::set_index_cache_size(std::uint32_t index_cache_size) {
    std::unique_lock write_lock(m_lock);
    std::lock_guard lock(m_index_cache_spin_lock);
    m_index_cache.set_capacity(index_cache_size);
  }

  bool BTreeDatabase::auto_commit() const {
    std::shared_lock read_lock(m_lock);
    return m_auto_commit;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_auto_commit(bool auto_commit) {
    std::unique_lock write_lock(m_lock);
    m_auto_commit = auto_commit;
    if (m_auto_commit)
      return do_commit();
    return {};
  }

  std::shared_ptr<IODevice> BTreeDatabase::io_device() const {
    std::shared_lock read_lock(m_lock);
    return m_device;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::set_io_device(std::shared_ptr<IODevice> device) {
    std::unique_lock write_lock(m_lock);
    if (auto state_checked = check_if_open("set_io_device", false); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    m_device = std::move(device);
    return {};
  }

  bool BTreeDatabase::is_open() const {
    std::shared_lock read_lock(m_lock);
    return m_open;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::open() {
    std::unique_lock write_lock(m_lock);
    if (m_open) {
      return false;
    }
    auto old_block_size = m_block_size;
    auto old_key_size = m_key_size;
    auto old_identifier = m_content_identifier;
    bool opened_device = m_device && !m_device->is_open();
    std::optional<StreamOffset> initial_device_size;
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
      initial_device_size = *device_size;
      if ((*device_size) > 0) {

        DataStreamIODevice ds(*m_device);
        if (auto positioned = ds.seek(0); !positioned)
          return std::unexpected(std::move(positioned.error()));

        auto stored_magic = ds.read_bytes(version_magic.size());
        if (!stored_magic) return std::unexpected(std::move(stored_magic.error()));
        if (!std::ranges::equal(stored_magic->span(), version_magic)) {
          return std::unexpected(DBException("Device is not a valid BTreeDatabase file"));
        }

        auto block_size = ds.read<std::uint32_t>();
        if (!block_size) return std::unexpected(std::move(block_size.error()));
        m_block_size = (*block_size);
        if (m_block_size < 44) {
          return std::unexpected(DBException("Invalid database block size"));
        }

        auto identifier_bytes = ds.read_bytes(content_identifier_string_size);
        if (!identifier_bytes) return std::unexpected(std::move(identifier_bytes.error()));
        auto content_identifier = std::move(*identifier_bytes);
        auto identifier_view = content_identifier.span();
        m_content_identifier = String(std::string_view(identifier_view.begin(), std::ranges::find(identifier_view, '\0')));
        auto key_size = ds.read<std::uint32_t>();
        if (!key_size) return std::unexpected(std::move(key_size.error()));
        m_key_size = (*key_size);
        if (m_key_size == 0 || m_key_size > m_block_size - 11) {
          return std::unexpected(DBException("Invalid database key size"));
        }

        if (auto root_read = read_root(); !root_read)
          return std::unexpected(std::move(root_read.error()));

        if (m_device->is_writable()) {
          if (auto resized = m_device->resize(m_device_size); !resized)
            return std::unexpected(std::move(resized.error()));
        }

        return false;
      } else {

        if (m_content_identifier.empty()) {
          return std::unexpected(DBException("Opening new database and no content identifier set!"));
        }
        if (m_content_identifier.utf8_size() > content_identifier_string_size) {
          return std::unexpected(DBException("content_identifier in BTreeDatabase implementation is greater than maximum identifier length"));
        }
        if (m_key_size == 0 || m_block_size < 44 || m_key_size > m_block_size - 11) {
          return std::unexpected(DBException("Invalid key or block size opening a new BTreeDatabase"));
        }
        m_device_size = header_size;
        if (auto resized = m_device->resize(m_device_size); !resized)
          return std::unexpected(std::move(resized.error()));

        m_head_free_index_block = invalid_block_index;

        DataStreamIODevice ds(*m_device);
        if (auto positioned = ds.seek(0); !positioned)
          return std::unexpected(std::move(positioned.error()));

        if (auto magic_written = ds.write_data(version_magic); !magic_written)
          return std::unexpected(std::move(magic_written.error()));

        if (auto block_size_written = ds.write<std::uint32_t>(m_block_size); !block_size_written)
          return std::unexpected(std::move(block_size_written.error()));

        std::array<char, content_identifier_string_size> content_identifier{};
        std::ranges::copy(m_content_identifier.utf8(), content_identifier.begin());
        if (auto identifier_written = ds.write_data(std::span<char const>(content_identifier)); !identifier_written)
          return std::unexpected(std::move(identifier_written.error()));

        if (auto key_size_written = ds.write(m_key_size); !key_size_written)
          return std::unexpected(std::move(key_size_written.error()));

        auto created_root = m_impl.create_new_root();
        if (!created_root) return std::unexpected(std::move(created_root.error()));
        if (auto committed = do_commit(); !committed)
          return std::unexpected(std::move(committed.error()));

        return true;
      }
    };
    auto initialized = initialize();
    if (!initialized) {

      auto failure = std::move(initialized.error());
      if (initial_device_size == 0 && m_device && m_device->is_open() && m_device->is_writable()) {
        auto truncated = m_device->resize(0);
        if (!truncated)
          failure = DBException(Star::format("Opening database failed; removing the partial new database also failed: {}", truncated.error().what()), ExceptionDiagnostic(failure));
      }
      m_open = false;
      m_block_size = old_block_size;
      m_key_size = old_key_size;
      m_content_identifier = std::move(old_identifier);
      m_device_size = 0;
      m_root = invalid_block_index;
      m_head_free_index_block = invalid_block_index;
      m_root_is_leaf = false;
      m_using_alt_root = false;
      m_available_blocks.clear();
      m_uncommitted.clear();
      m_uncommitted_writes.clear();
      m_index_cache.clear();
      if (opened_device && m_device->is_open()) {
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
    std::shared_lock read_lock(m_lock);
    if (auto key_checked = check_key_size(k); !key_checked)
      return std::unexpected(std::move(key_checked.error()));

    return m_impl.contains(k);
  }

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> BTreeDatabase::find(ByteArray const& k) {
    std::shared_lock read_lock(m_lock);
    if (auto key_checked = check_key_size(k); !key_checked)
      return std::unexpected(std::move(key_checked.error()));

    return m_impl.find(k);
  }

  [[nodiscard]] std::expected<List<std::pair<ByteArray, ByteArray>>, StarException> BTreeDatabase::find(ByteArray const& lower, ByteArray const& upper) {
    std::shared_lock read_lock(m_lock);
    if (auto lower_key_checked = check_key_size(lower); !lower_key_checked)
      return std::unexpected(std::move(lower_key_checked.error()));

    if (auto upper_key_checked = check_key_size(upper); !upper_key_checked)
      return std::unexpected(std::move(upper_key_checked.error()));

    return m_impl.find(lower, upper);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::for_each(ByteArray const& lower, ByteArray const& upper, std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock read_lock(m_lock);
    if (auto lower_key_checked = check_key_size(lower); !lower_key_checked)
      return std::unexpected(std::move(lower_key_checked.error()));

    if (auto upper_key_checked = check_key_size(upper); !upper_key_checked)
      return std::unexpected(std::move(upper_key_checked.error()));

    return m_impl.for_each(lower, upper, std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::for_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("for_all", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    return m_impl.for_all(std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::recover_all(std::function<std::expected<void, StarException>(ByteArray const&, ByteArray const&)> v, std::function<std::expected<void, StarException>(String const&, ExceptionDiagnostic const&)> e) {
    if (!v || !e) {
      return std::unexpected(DBException("Missing database recovery callback"));
    }
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("recover_all", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    return m_impl.recover_all(std::move(v), std::move(e));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::insert(ByteArray const& k, ByteArray const& data) {
    std::unique_lock write_lock(m_lock);
    auto key_checked = check_key_size(k);
    if (!key_checked) return std::unexpected(std::move(key_checked.error()));
    auto modified = m_impl.insert(k, data);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_auto_commit) {
      auto committed = do_commit();
      if (!committed) return std::unexpected(std::move(committed.error()));
    }
    return modified;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::erase(ByteArray const& k) {
    std::unique_lock write_lock(m_lock);
    auto key_checked = check_key_size(k);
    if (!key_checked) return std::unexpected(std::move(key_checked.error()));
    auto modified = m_impl.erase(k);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_auto_commit) {
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
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("record_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    return m_impl.record_count();
  }

  [[nodiscard]] std::expected<std::uint8_t, StarException> BTreeDatabase::index_levels() {
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("index_levels", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    auto levels = m_impl.index_levels();
    if (!levels) return std::unexpected(std::move(levels.error()));
    return std::move(*levels);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::total_block_count() {
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("total_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    auto device_size = m_device->size();
    if (!device_size) return std::unexpected(std::move(device_size.error()));
    return ((*device_size) - header_size) / m_block_size;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::free_block_count() {
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("free_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    // Go through every FreeIndexBlock in the chain and count all of the tracked
    // free blocks.
    BlockIndex count = 0;
    BlockIndex index_block_index = m_head_free_index_block;
    while (index_block_index != invalid_block_index) {

      auto free_index = read_free_index_block(index_block_index);
      if (!free_index) return std::unexpected(std::move(free_index.error()));
      FreeIndexBlock index_block = std::move(*free_index);
      count += 1 + index_block.free_blocks.size();
      index_block_index = index_block.next_free_block;
    }

    count += m_available_blocks.size();

    // Include untracked blocks at the end of the file in the free count.
    auto device_size = m_device->size();
    if (!device_size) return std::unexpected(std::move(device_size.error()));
    count += ((*device_size) - m_device_size) / m_block_size;

    return count;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::index_block_count() {
    std::shared_lock read_lock(m_lock);
    if (auto state_checked = check_if_open("index_block_count", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    // Indexes are simply one index per block
    auto index_count = m_impl.index_count();
    if (!index_count) return std::unexpected(std::move(index_count.error()));
    return (*index_count);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::leaf_block_count() {
    std::unique_lock write_lock(m_lock);
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
    std::unique_lock write_lock(m_lock);
    return do_commit();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::rollback() {
    std::unique_lock write_lock(m_lock);
    auto state_checked = check_if_open("rollback", true);
    if (!state_checked) return std::unexpected(std::move(state_checked.error()));

    m_available_blocks.clear();
    m_index_cache.clear();
    m_uncommitted_writes.clear();
    m_uncommitted.clear();

    if (auto root_read = read_root(); !root_read)
      return std::unexpected(std::move(root_read.error()));

    if (m_device->is_writable())
      return m_device->resize(m_device_size);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::close(bool close_device) {
    std::unique_lock write_lock(m_lock);
    if (m_open) {

      auto flattened = try_flatten();
      if (!flattened) return std::unexpected(std::move(flattened.error()));
      if (!*flattened) {
        if (auto committed = do_commit(); !committed)
          return std::unexpected(std::move(committed.error()));
      }

      m_index_cache.clear();

      m_open = false;
      if (close_device && m_device && m_device->is_open())
        return m_device->close();
    }
    return {};
  }

  BTreeDatabase::BlockIndex const BTreeDatabase::invalid_block_index;
  std::uint32_t const BTreeDatabase::header_size;
  std::array<char, 8> const BTreeDatabase::version_magic = {'B', 'T', 'r', 'e', 'e', 'D', 'B', '5'};
  std::array<char, 2> const BTreeDatabase::index_magic = {'I', 'I'};
  std::array<char, 2> const BTreeDatabase::leaf_magic = {'L', 'L'};
  std::array<char, 2> const BTreeDatabase::free_index_magic = {'F', 'F'};
  std::size_t const BTreeDatabase::btree_root_selector_bit;
  std::size_t const BTreeDatabase::btree_root_info_start;
  std::size_t const BTreeDatabase::btree_root_info_size;

  std::size_t BTreeDatabase::IndexNode::pointer_count() const {
    // If no begin pointer is set then the index is simply uninitialized.
    if (!begin_pointer)
    return 0;
    else
    return pointers.size() + 1;
  }

  auto BTreeDatabase::IndexNode::pointer(std::size_t i) const -> BlockIndex {
    if (i == 0)
    return *begin_pointer;
    else
    return pointers[i - 1].pointer;
  }

  void BTreeDatabase::IndexNode::update_pointer(std::size_t i, BlockIndex p) {
    if (i == 0)
    *begin_pointer = p;
    else
    pointers[i - 1].pointer = p;
  }

  ByteArray const& BTreeDatabase::IndexNode::key_before(std::size_t i) const {
    return pointers[i - 1].key;
  }

  void BTreeDatabase::IndexNode::update_key_before(std::size_t i, ByteArray k) { pointers[i - 1].key = std::move(k); }

  void BTreeDatabase::IndexNode::erase_before(std::size_t i) {
    if (i == 0) {
      begin_pointer = pointers[0].pointer;
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

    pointers.push_back(Element{.key = mid, .pointer = *right.begin_pointer});

    ElementList::iterator s = right.pointers.begin();
    std::advance(s, count - 1);
    pointers.insert(pointers.end(), std::make_move_iterator(right.pointers.begin()), std::make_move_iterator(s));

    right.pointers.erase(right.pointers.begin(), s);
    if (right.pointers.size() != 0) {
      right.begin_pointer = right.pointers[0].pointer;
      right.pointers.erase_at(0);
    } else {
      right.begin_pointer.reset();
    }
  }

  void BTreeDatabase::IndexNode::shift_right(ByteArray const& mid, IndexNode& left, std::size_t count) {
    count = std::min(left.pointer_count(), count);

    if (count == 0)
    return;
    --count;

    pointers.insert(pointers.begin(), Element{.key = mid, .pointer = *begin_pointer});

    ElementList::iterator s = left.pointers.begin();
    std::advance(s, left.pointers.size() - count);
    pointers.insert(pointers.begin(), std::make_move_iterator(s), std::make_move_iterator(left.pointers.end()));

    left.pointers.erase(s, left.pointers.end());
    if (left.pointers.size() != 0) {
      begin_pointer = left.pointers[left.pointers.size() - 1].pointer;
      left.pointers.erase_at(left.pointers.size() - 1);
    } else {
      begin_pointer = std::exchange(left.begin_pointer, std::nullopt);
    }
  }

  ByteArray BTreeDatabase::IndexNode::split(IndexNode& right, std::size_t i) {
    ElementList::iterator s = pointers.begin();
    std::advance(s, i - 1);

    right.begin_pointer = s->pointer;
    ByteArray mid_key = std::move(s->key);
    right.level = level;
    ++s;

    right.pointers.insert(right.pointers.begin(), std::make_move_iterator(s), std::make_move_iterator(pointers.end()));
    --s;

    pointers.erase(s, pointers.end());

    return mid_key;
  }

  std::size_t BTreeDatabase::LeafNode::size() const {
    return elements.size();
  }

  ByteArray const& BTreeDatabase::LeafNode::key(std::size_t i) const {
    return elements[i].key;
  }

  ByteArray const& BTreeDatabase::LeafNode::data(std::size_t i) const {
    return elements[i].data;
  }

  std::uint32_t BTreeDatabase::LeafNode::element_size(Element const& element) {
    return element.key.size() + vlq_u_size(element.data.size()) + element.data.size();
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
    count = std::min(right.size(), count);

    if (count == 0)
    return;

    ElementList::iterator s = right.elements.begin();
    std::advance(s, count);

    std::uint32_t transferred_size = right.encoded_size - 6;
    if (count != right.size()) {
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
    count = std::min(left.size(), count);

    if (count == 0)
    return;

    ElementList::iterator s = left.elements.begin();
    std::advance(s, left.elements.size() - count);

    std::uint32_t transferred_size = left.encoded_size - 6;
    if (count != left.size()) {
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
    return parent.m_root_is_leaf;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::set_new_root(Pointer pointer, bool is_leaf) {
    parent.m_root = pointer;
    parent.m_root_is_leaf = is_leaf;
    return {};
  }

  auto BTreeDatabase::BTreeImpl::create_index(Pointer begin_pointer) -> Index {
    auto index = std::make_shared<IndexNode>();
    index->self = invalid_block_index;
    index->level = 0;
    index->begin_pointer = begin_pointer;
    return index;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::load_index(Pointer pointer) -> std::expected<Index, StarException> {
    std::unique_lock lock(parent.m_index_cache_spin_lock);
    if (auto index = parent.m_index_cache.find_ref(pointer)) {
      return index->get();
    }
    lock.unlock();

    auto index = std::make_shared<IndexNode>();

    auto block_bytes = parent.read_block(pointer);
    if (!block_bytes) return std::unexpected(std::move(block_bytes.error()));
    DataStreamBuffer buffer(std::move(*block_bytes));

    auto magic = buffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if (!std::ranges::equal(magic->span(), index_magic)) {
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
    index->begin_pointer = (*begin_pointer);
    if (s >= parent.max_index_pointers()) return std::unexpected(DBException("Invalid index node pointer count"));
    index->pointers.resize(s);
    for (std::uint32_t i = 0; i < s; ++i) {

      auto& e = index->pointers[i];
      auto key_bytes = buffer.read_bytes(parent.m_key_size);
      if (!key_bytes) return std::unexpected(std::move(key_bytes.error()));
      e.key = std::move(*key_bytes);
      auto child_pointer = buffer.read<BlockIndex>();
      if (!child_pointer) return std::unexpected(std::move(child_pointer.error()));
      e.pointer = (*child_pointer);
    }

    lock.lock();
    // Decode without the spinlock; only the cheap shared-owner publication is locked.
    auto cached = parent.m_index_cache.get(pointer, [&index](BlockIndex const&) { return std::move(index); });
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
    right->self = invalid_block_index;
    Key k = index->split(*right, (index->pointer_count() + 1) / 2);
    return std::make_pair(std::move(k), std::move(right));
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::store_index(Index index) -> std::expected<Pointer, StarException> {
    if (index->self != invalid_block_index) {

      if (!parent.m_uncommitted.contains(index->self)) {

        parent.free_block(index->self);
        parent.m_index_cache.erase(index->self);
        index->self = invalid_block_index;
      }
    }

    if (index->self == invalid_block_index) {
      auto reserved = parent.reserve_block();
      if (!reserved) return std::unexpected(std::move(reserved.error()));
      index->self = (*reserved);
    }

    DataStreamBuffer buffer(parent.m_block_size);
    if (auto magic_written = buffer.write_data(index_magic); !magic_written)
      return std::unexpected(std::move(magic_written.error()));

    if (auto level_written = buffer.write<std::uint8_t>(index->level); !level_written)
      return std::unexpected(std::move(level_written.error()));

    if (auto pointer_count_written = buffer.write<std::uint32_t>(index->pointers.size()); !pointer_count_written)
      return std::unexpected(std::move(pointer_count_written.error()));

    if (auto begin_pointer_written = buffer.write<BlockIndex>(*index->begin_pointer); !begin_pointer_written)
      return std::unexpected(std::move(begin_pointer_written.error()));

    for (auto i = index->pointers.begin(); i != index->pointers.end(); ++i) {

      if (auto key_written = buffer.write_bytes(i->key); !key_written)
        return std::unexpected(std::move(key_written.error()));
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((i->key.size() == parent.m_key_size));
      }
      if (auto pointer_written = buffer.write<BlockIndex>(i->pointer); !pointer_written)
        return std::unexpected(std::move(pointer_written.error()));

    }

    if (auto staged = parent.update_block(index->self, buffer.take_data()); !staged)
      return std::unexpected(std::move(staged.error()));

    auto cached = parent.m_index_cache.get(index->self, [&index](BlockIndex const&) { return index; });
    if (!cached) return std::unexpected(std::move(cached.error()));
    cached->get() = std::move(index);
    return cached->get()->self;
  }

  void BTreeDatabase::BTreeImpl::delete_index(Index index) {
    parent.m_index_cache.erase(index->self);
    parent.free_block(index->self);
  }

  auto BTreeDatabase::BTreeImpl::create_leaf() -> Leaf {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = invalid_block_index;
    return leaf;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::load_leaf(Pointer pointer) -> std::expected<Leaf, StarException> {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = pointer;

    BlockIndex current_leaf_block = leaf->self;
    ByteArray leaf_bytes;
    leaf_bytes.resize(parent.m_block_size);
    DataStreamBuffer leaf_buffer(std::move(leaf_bytes));
    if (auto read = parent.read_block(current_leaf_block, 0, leaf_buffer.span()); !read)
      return std::unexpected(std::move(read.error()));

    auto magic = leaf_buffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if (!std::ranges::equal(magic->span(), leaf_magic)) {
      return std::unexpected(DBException("Error, incorrect leaf block signature."));
    }

    DataStreamFunctions leaf_input([&](std::span<char> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (left > 0) {

        auto position = leaf_buffer.pos();
        if (!position) return std::unexpected(std::move(position.error()));
        if ((*position) + left < parent.m_block_size - sizeof(BlockIndex)) {

          if (auto read = leaf_buffer.read_data(data.subspan(pos, left)); !read)
            return std::unexpected(std::move(read.error()));

          pos += left;
          left = 0;
        } else {

          auto tail_position = leaf_buffer.pos();
          if (!tail_position) return std::unexpected(std::move(tail_position.error()));
          std::size_t to_read = parent.m_block_size - sizeof(BlockIndex) - (*tail_position);
          if (auto read = leaf_buffer.read_data(data.subspan(pos, to_read)); !read)
            return std::unexpected(std::move(read.error()));

          pos += to_read;
          left -= to_read;
        }

        auto next_position = leaf_buffer.pos();
        if (!next_position) return std::unexpected(std::move(next_position.error()));
        if ((*next_position) == (parent.m_block_size - sizeof(BlockIndex)) && left > 0) {

          auto next_block = leaf_buffer.read<BlockIndex>();
          if (!next_block) return std::unexpected(std::move(next_block.error()));
          current_leaf_block = (*next_block);
          if (current_leaf_block != invalid_block_index) {

            if (auto positioned = leaf_buffer.seek(0); !positioned)
              return std::unexpected(std::move(positioned.error()));
            if (auto read = parent.read_block(current_leaf_block, 0, leaf_buffer.span()); !read)
              return std::unexpected(std::move(read.error()));

            auto magic = leaf_buffer.read_bytes(2);
            if (!magic) return std::unexpected(std::move(magic.error()));
            if (!std::ranges::equal(magic->span(), leaf_magic)) {
              return std::unexpected(DBException("Error, incorrect leaf block signature."));
            }
          } else {

            return std::unexpected(DBException("Leaf read off end of Leaf list."));
          }
        }
      }

      return data.size();
    }, {});

    auto element_count = leaf_input.read<std::uint32_t>();
    if (!element_count) return std::unexpected(std::move(element_count.error()));
    std::uint32_t count = (*element_count);
    if (count > static_cast<std::uint64_t>(parent.m_device_size - header_size) / (parent.m_key_size + 1))
    return std::unexpected(DBException("Invalid leaf element count"));
    leaf->elements.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {

      auto& element = leaf->elements[i];
      auto key_bytes = leaf_input.read_bytes(parent.m_key_size);
      if (!key_bytes) return std::unexpected(std::move(key_bytes.error()));
      element.key = std::move(*key_bytes);
      auto data_bytes = leaf_input.read<ByteArray>();
      if (!data_bytes) return std::unexpected(std::move(data_bytes.error()));
      element.data = std::move(*data_bytes);
      leaf->encoded_size += LeafNode::element_size(element);
    }

    return leaf;
  }

  bool BTreeDatabase::BTreeImpl::leaf_needs_shift(Leaf const& l) {
    return parent.leaf_size(l) < parent.m_block_size / 2;
  }

  bool BTreeDatabase::BTreeImpl::leaf_shift(Leaf& left, Leaf& right) {
    if (left->size() == 0) {
      left->shift_left(*right, right->size());
      return true;
    }

    if (right->size() == 0)
    return true;

    std::uint32_t left_size = parent.leaf_size(left);
    std::uint32_t right_size = parent.leaf_size(right);
    if (left_size + right_size < parent.m_block_size) {
      left->shift_left(*right, right->size());
      return true;
    }

    // TODO: Shifting algorithm is bad, could potentially want to shift more
    // than one element here.
    std::uint32_t right_begin_size = parent.m_key_size + parent.data_size(right->elements[0].data);
    std::uint32_t left_end_size = parent.m_key_size + parent.data_size(left->elements[left->elements.size() - 1].data);
    if (left_size < right_size - right_begin_size && left_size + right_begin_size < parent.m_block_size) {
      left->shift_left(*right, 1);
      return true;
    } else if (right_size < left_size - left_end_size && right_size + left_end_size < parent.m_block_size) {
      right->shift_right(*left, 1);
      return true;
    }

    return false;
  }

  auto BTreeDatabase::BTreeImpl::leaf_split(Leaf& leaf) -> std::optional<Leaf> {
    if (leaf->elements.size() < 2)
    return {};

    std::uint32_t size = leaf->encoded_size;
    if (size < parent.m_block_size * 2 - 2 * sizeof(BlockIndex) - 4)
      return {};
    std::uint32_t prefix_size = 6;
    std::uint32_t boundary = 0;
    for (std::uint32_t i = 0; i < leaf->elements.size(); ++i) {
      prefix_size += parent.m_key_size;
      prefix_size += parent.data_size(leaf->elements[i].data);
      if (prefix_size > parent.m_block_size - sizeof(BlockIndex)) {
        boundary = i;
        break;
      }
    }
    if (boundary == 0)
    boundary = 1;

    auto right = std::make_shared<LeafNode>();
    right->self = invalid_block_index;
    leaf->split(*right, boundary);
    return right;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::store_leaf(Leaf leaf) -> std::expected<Pointer, StarException> {
    if (leaf->self != invalid_block_index) {

      auto tail_blocks = parent.leaf_tail_blocks(leaf->self);
      if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
      for (auto block : *tail_blocks)
        parent.free_block(block);

      if (!parent.m_uncommitted.contains(leaf->self)) {

        parent.free_block(leaf->self);
        leaf->self = invalid_block_index;
      }
    }

    if (leaf->self == invalid_block_index) {
      auto reserved = parent.reserve_block();
      if (!reserved) return std::unexpected(std::move(reserved.error()));
      leaf->self = (*reserved);
    }

    BlockIndex current_leaf_block = leaf->self;
    DataStreamBuffer leaf_buffer;
    leaf_buffer.reset(parent.m_block_size);
    if (auto magic_written = leaf_buffer.write_data(leaf_magic); !magic_written)
      return std::unexpected(std::move(magic_written.error()));

    DataStreamFunctions leaf_output({}, [&](std::span<char const> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (true) {
        auto buffer_position = leaf_buffer.pos();
        if (!buffer_position) return std::unexpected(std::move(buffer_position.error()));
        std::size_t to_write = std::min(left, parent.m_block_size - *buffer_position - sizeof(BlockIndex));

        if (to_write != 0) {
          auto written = leaf_buffer.write_data(data.subspan(pos, to_write));
          if (!written) return std::unexpected(std::move(written.error()));
          left -= to_write;
          pos += to_write;
        }

        if (left == 0)
          break;

        if (*buffer_position + to_write == parent.m_block_size - sizeof(BlockIndex)) {
          auto next_block = parent.reserve_block();
          if (!next_block) return std::unexpected(std::move(next_block.error()));
          auto linked = leaf_buffer.write<BlockIndex>(*next_block);
          if (!linked) return std::unexpected(std::move(linked.error()));
          auto updated = parent.update_block(current_leaf_block, leaf_buffer.data());
          if (!updated) return std::unexpected(std::move(updated.error()));
          current_leaf_block = *next_block;
          leaf_buffer.reset(parent.m_block_size);
          auto marked = leaf_buffer.write_data(leaf_magic);
          if (!marked) return std::unexpected(std::move(marked.error()));
        }
      }

      return data.size();
    });

    if (auto count_written = leaf_output.write<std::uint32_t>(leaf->elements.size()); !count_written)
      return std::unexpected(std::move(count_written.error()));

    for (LeafNode::ElementList::iterator i = leaf->elements.begin(); i != leaf->elements.end(); ++i) {

      if (auto key_written = leaf_output.write_bytes(i->key); !key_written)
        return std::unexpected(std::move(key_written.error()));
      if constexpr (Star::debug_enabled) {
        Star::star_assert_failed((i->key.size() == parent.m_key_size));
      }
      if (auto data_written = leaf_output.write(i->data); !data_written)
        return std::unexpected(std::move(data_written.error()));

    }

    if (auto tail_positioned = leaf_buffer.seek(parent.m_block_size - sizeof(BlockIndex)); !tail_positioned)
      return std::unexpected(std::move(tail_positioned.error()));

    if (auto tail_terminated = leaf_buffer.write<BlockIndex>(invalid_block_index); !tail_terminated)
      return std::unexpected(std::move(tail_terminated.error()));

    if (auto staged = parent.update_block(current_leaf_block, leaf_buffer.take_data()); !staged)
      return std::unexpected(std::move(staged.error()));

    return leaf->self;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::delete_leaf(Leaf leaf) {
    auto tail_blocks = parent.leaf_tail_blocks(leaf->self);
    if (!tail_blocks) return std::unexpected(std::move(tail_blocks.error()));
    for (auto block : *tail_blocks)
      parent.free_block(block);

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
    return leaf->size();
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

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::read_block(BlockIndex block_index, std::size_t block_offset, std::span<char> block) const {
    if (auto block_checked = check_block_index(block_index); !block_checked)
      return std::unexpected(std::move(block_checked.error()));

    return raw_read_block(block_index, block_offset, block);
  }

  [[nodiscard]] std::expected<ByteArray, StarException> BTreeDatabase::read_block(BlockIndex block_index) const {
    ByteArray block;
    block.resize(m_block_size);
    if (auto read = read_block(block_index, 0, block.span()); !read)
      return std::unexpected(std::move(read.error()));

    return block;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::update_block(BlockIndex block_index, ByteArray const& block) {
    if (auto block_checked = check_block_index(block_index); !block_checked)
      return std::unexpected(std::move(block_checked.error()));

    return raw_write_block(block_index, 0, block.span());
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::update_block(BlockIndex block_index, ByteArray&& block) {
    if (auto block_checked = check_block_index(block_index); !block_checked)
      return std::unexpected(std::move(block_checked.error()));
    if (block.size() != m_block_size)
      return raw_write_block(block_index, 0, block.span());
    m_uncommitted_writes.insert_or_assign(block_index, std::move(block));
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::raw_read_block(BlockIndex block_index, std::size_t block_offset, std::span<char> block) const {
    auto size = block.size();
    if (block_offset > m_block_size || size > m_block_size - block_offset) {
      return std::unexpected(DBException::format("Read past end of block, offset: {} size {}", block_offset, size));
    }

    if (size <= 0) {
      return {};
    }

    if (auto buffer = m_uncommitted_writes.find_ref(block_index)) {
      buffer->get().copy_to(block, block_offset);
    } else {
      return m_device->read_full_absolute(
        header_size + block_index * static_cast<StreamOffset>(m_block_size) + block_offset, block);
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::raw_write_block(BlockIndex block_index, std::size_t block_offset, std::span<char const> block) {
    auto size = block.size();
    if (block_offset > m_block_size || size > m_block_size - block_offset) {
      return std::unexpected(DBException::format("Write past end of block, offset: {} size {}", block_offset, size));
    }

    if (size <= 0) {
      return {};
    }

    if (block_offset == 0 && size == m_block_size) {
      if (auto staged = m_uncommitted_writes.find_ref(block_index)) {
        auto& destination = staged->get();
        if (std::as_const(destination).span().begin() != block.begin())
          destination.write_from(block, 0);
      } else {
        m_uncommitted_writes.emplace(block_index, ByteArray(block));
      }
      return {};
    }

    StreamOffset block_start = header_size + block_index * static_cast<StreamOffset>(m_block_size);
    auto buffer = m_uncommitted_writes.find(block_index);
    if (buffer == m_uncommitted_writes.end()) {
      auto original_block = m_device->read_bytes_absolute(block_start, m_block_size);
      if (!original_block) return std::unexpected(std::move(original_block.error()));
      buffer = m_uncommitted_writes.emplace(block_index, std::move(*original_block)).first;
    }

    buffer->second.write_from(block, block_offset);
    return {};
  }

  [[nodiscard]] auto BTreeDatabase::read_free_index_block(BlockIndex block_index) -> std::expected<FreeIndexBlock, StarException> {
    auto block = read_block(block_index);
    if (!block) return std::unexpected(std::move(block.error()));
    DataStreamReader buffer(block->span());
    auto magic = buffer.read_bytes(2);
    if (!magic) return std::unexpected(std::move(magic.error()));
    if (!std::ranges::equal(magic->span(), free_index_magic))
      return std::unexpected(DBException::format("Internal exception! block {} missing free index block marker!", block_index));

    FreeIndexBlock free_index_block;
    auto next = buffer.read<BlockIndex>();
    if (!next) return std::unexpected(std::move(next.error()));
    free_index_block.next_free_block = *next;
    auto count = buffer.read<std::uint32_t>();
    if (!count) return std::unexpected(std::move(count.error()));
    for (std::uint32_t i = 0; i < *count; ++i) {
      // Keep the checked block-bound diagnostic of the former field reads.
      std::size_t offset = 6 + sizeof(BlockIndex) + sizeof(BlockIndex) * static_cast<std::size_t>(i);
      if (offset > m_block_size || sizeof(BlockIndex) > m_block_size - offset)
        return std::unexpected(DBException::format("Read past end of block, offset: {} size {}", offset, sizeof(BlockIndex)));
      auto free_block = buffer.read<BlockIndex>();
      if (!free_block) return std::unexpected(std::move(free_block.error()));
      free_index_block.free_blocks.push_back(*free_block);
    }
    return free_index_block;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::write_free_index_block(BlockIndex block_index, FreeIndexBlock const& index_block) {
    // Preserve old padding and acquire the complete old payload before staging
    // any fields, exactly as the first partial write previously did.
    auto block = read_block(block_index);
    if (!block) return std::unexpected(std::move(block.error()));
    DataStreamBuffer buffer(std::move(*block));
    if (auto magic = buffer.write_data(free_index_magic); !magic)
      return std::unexpected(std::move(magic.error()));
    if (auto next = buffer.write<BlockIndex>(index_block.next_free_block); !next)
      return std::unexpected(std::move(next.error()));
    if (auto count = buffer.write<std::uint32_t>(index_block.free_blocks.size()); !count)
      return std::unexpected(std::move(count.error()));
    for (auto free_block : index_block.free_blocks) {
      if (auto written = buffer.write<BlockIndex>(free_block); !written)
        return std::unexpected(std::move(written.error()));
    }
    return update_block(block_index, buffer.take_data());
  }

  std::uint32_t BTreeDatabase::leaf_size(std::shared_ptr<LeafNode> const& leaf) const {
    return leaf->encoded_size;
  }

  std::uint32_t BTreeDatabase::max_index_pointers() const {
    // 2 for magic, 1 byte for level, sizeof(BlockIndex) for begin_pointer, 4
    // for size.
    return (m_block_size - 2 - 1 - sizeof(BlockIndex) - 4) / (m_key_size + sizeof(BlockIndex)) + 1;
  }

  std::uint32_t BTreeDatabase::data_size(ByteArray const& d) const {
    return vlq_u_size(d.size()) + d.size();
  }

  [[nodiscard]] auto BTreeDatabase::leaf_tail_blocks(BlockIndex leaf_pointer) -> std::expected<List<BlockIndex>, StarException> {
    List<BlockIndex> tail_blocks;
    DataStreamBuffer pointer_buffer(sizeof(BlockIndex));
    while (leaf_pointer != invalid_block_index) {

      if (auto tail_read = read_block(leaf_pointer, m_block_size - sizeof(BlockIndex), pointer_buffer.span().first(sizeof(BlockIndex))); !tail_read)
        return std::unexpected(std::move(tail_read.error()));

      if (auto positioned = pointer_buffer.seek(0); !positioned)
        return std::unexpected(std::move(positioned.error()));

      auto next_block = pointer_buffer.read<BlockIndex>();
      if (!next_block) return std::unexpected(std::move(next_block.error()));
      leaf_pointer = (*next_block);
      if (leaf_pointer != invalid_block_index) {
        tail_blocks.push_back(leaf_pointer);
      }
    }
    return tail_blocks;
  }

  void BTreeDatabase::free_block(BlockIndex b) {
    m_uncommitted.remove(b);
    if (m_uncommitted_writes.contains(b))
    m_uncommitted_writes.remove(b);

    m_available_blocks.add(b);
  }

  [[nodiscard]] auto BTreeDatabase::reserve_block() -> std::expected<BlockIndex, StarException> {
    if (m_available_blocks.empty()) {

      if (m_head_free_index_block != invalid_block_index) {

        // If available, make available all the blocks in the first free index
        // block.
        auto free_index = read_free_index_block(m_head_free_index_block);
        if (!free_index) return std::unexpected(std::move(free_index.error()));
        FreeIndexBlock index_block = std::move(*free_index);
        for (auto const& b : index_block.free_blocks) {
          m_available_blocks.add(b);
        }
        m_available_blocks.add(m_head_free_index_block);
        m_head_free_index_block = index_block.next_free_block;
      }

      if (m_available_blocks.empty()) {

        // If we still don't have any available blocks, just add a block to the
        // end of the file.
        auto appended = make_end_block();
        if (!appended) return std::unexpected(std::move(appended.error()));
        m_available_blocks.add((*appended));
      }
    }

    auto available = m_available_blocks.take_first();
    if (!available) return std::unexpected(std::move(available.error()));
    BlockIndex block = (*available);
    m_uncommitted.add(block);
    return block;
  }

  [[nodiscard]] auto BTreeDatabase::make_end_block() -> std::expected<BlockIndex, StarException> {
    BlockIndex block_count = (m_device_size - header_size) / m_block_size;
    if (auto resized = m_device->resize(m_device_size + m_block_size); !resized)
      return std::unexpected(std::move(resized.error()));

    m_device_size += m_block_size;
    return block_count;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::write_root() {
    DataStreamIODevice ds(*m_device);
    // commit_writes() has already synchronized staged blocks. Serialize only the
    // inactive root's 4 + 8 + 4 + 1 bytes; the other root and reserved header
    // padding must not be rewritten. Keep the existing DataStream encodings.
    if (auto root_positioned = ds.seek(btree_root_info_start + (m_using_alt_root ? 0 : btree_root_info_size)); !root_positioned)
      return std::unexpected(std::move(root_positioned.error()));

    static_assert(2 * sizeof(BlockIndex) + sizeof(StreamOffset) + 1 == btree_root_info_size);
    std::array<char, btree_root_info_size> root_info;
    std::span<char> remaining(root_info);
    DataStreamFunctions fields({},
                               [&remaining](std::span<char const> data) -> std::expected<std::size_t, StarException> {
                                 if (data.size() > remaining.size())
                                   return std::unexpected(IOException("BTree root info exceeds reserved size"));
                                 std::ranges::copy(data, remaining.begin());
                                 remaining = remaining.subspan(data.size());
                                 return data.size();
                               });

    if (auto free_index_written = fields.write<BlockIndex>(m_head_free_index_block); !free_index_written)
      return std::unexpected(std::move(free_index_written.error()));

    if (auto size_written = fields.write<StreamOffset>(m_device_size); !size_written)
      return std::unexpected(std::move(size_written.error()));

    if (auto root_written = fields.write<BlockIndex>(m_root); !root_written)
      return std::unexpected(std::move(root_written.error()));

    if (auto leaf_flag_written = fields.write<bool>(m_root_is_leaf); !leaf_flag_written)
      return std::unexpected(std::move(leaf_flag_written.error()));

    if (auto root_written = ds.write_data(root_info); !root_written)
      return std::unexpected(std::move(root_written.error()));

    // Barrier 2: the complete inactive root must be durable before selecting it.
    if (auto changes_synced = m_device->sync(); !changes_synced)
      return std::unexpected(std::move(changes_synced.error()));

    // Then switch headers by writing the single bit that switches them
    bool using_alt_root = !m_using_alt_root;
    if (auto selector_positioned = ds.seek(btree_root_selector_bit); !selector_positioned)
      return std::unexpected(std::move(selector_positioned.error()));

    if (auto selector_written = ds.write(using_alt_root); !selector_written)
      return std::unexpected(std::move(selector_written.error()));

    m_using_alt_root = using_alt_root;

    // Barrier 3: acknowledge the selector only after its own synchronization.
    // Preserve the selector update even on sync failure: durability is uncertain.
    return m_device->sync();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::read_root() {
    DataStreamIODevice ds(*m_device);
    if (auto selector_positioned = ds.seek(btree_root_selector_bit); !selector_positioned)
      return std::unexpected(std::move(selector_positioned.error()));

    auto selector = ds.read<bool>();
    if (!selector) return std::unexpected(std::move(selector.error()));
    auto using_alt_root = std::move(*selector);
    if (auto root_positioned = ds.seek(btree_root_info_start + (using_alt_root ? btree_root_info_size : 0)); !root_positioned)
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
    auto physical_size = std::move(*device_size);
    if (size < header_size || size > physical_size || (size - header_size) % m_block_size != 0) {
      return std::unexpected(DBException("Invalid database root size"));
    }
    auto block_count = (size - header_size) / m_block_size;
    if (root >= block_count || (head != invalid_block_index && head >= block_count)) {
      return std::unexpected(DBException("Invalid database root block"));
    }
    m_using_alt_root = using_alt_root;
    m_head_free_index_block = head;
    m_device_size = size;
    m_root = root;
    m_root_is_leaf = root_is_leaf;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::do_commit() {
    if (m_available_blocks.empty() && m_uncommitted.empty()) {
      return {};
    }

    if (!m_available_blocks.empty()) {
      auto head = m_head_free_index_block;
      FreeIndexBlock index_block{.next_free_block = invalid_block_index, .free_blocks = {}};
      if (head != invalid_block_index) {
        auto existing = read_free_index_block(head);
        if (!existing) return std::unexpected(std::move(existing.error()));
        index_block = std::move(*existing);
      }

      auto capacity = max_free_index_length();
      while (!m_available_blocks.empty()) {
        auto first = m_available_blocks.begin();
        auto next = first;
        if (head == invalid_block_index || index_block.free_blocks.size() >= capacity) {
          index_block.next_free_block = head;
          head = *next++;
          index_block.free_blocks.clear();
        }
        while (next != m_available_blocks.end() && index_block.free_blocks.size() < capacity)
          index_block.free_blocks.push_back(*next++);

        // Until the padding read and serialization succeed, these blocks
        // remain available and the previous (possibly staged) head is valid.
        if (auto staged = write_free_index_block(head, index_block); !staged)
          return std::unexpected(std::move(staged.error()));
        m_head_free_index_block = head;
        m_available_blocks.erase(first, next);
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
    for (auto& write : m_uncommitted_writes) {
      if (auto written = m_device->write_full_absolute(header_size + write.first * static_cast<StreamOffset>(m_block_size),
                                                     write.second.span().first(m_block_size)); !written)
        return std::unexpected(std::move(written.error()));

    }

    // Barrier 1: keep staged bytes until every block write and this sync succeed.
    // Root publication must not cross this barrier, even on a retry.
    if (auto synced = m_device->sync(); !synced)
      return std::unexpected(std::move(synced.error()));

    m_uncommitted_writes.clear();
    return {};
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::try_flatten() {
    if (m_head_free_index_block == invalid_block_index || m_root_is_leaf || !m_device->is_writable()) {
      return false;
    }

    BlockIndex free_block_count = 0;
    BlockIndex index_block_index = m_head_free_index_block;
    while (index_block_index != invalid_block_index) {

      auto free_index = read_free_index_block(index_block_index);
      if (!free_index) return std::unexpected(std::move(free_index.error()));
      FreeIndexBlock index_block = std::move(*free_index);
      free_block_count += 1 + index_block.free_blocks.size();
      index_block_index = index_block.next_free_block;
    }

    BlockIndex expected_block_count = (m_device_size - header_size) / m_block_size;
    float free = float(free_block_count) / float(expected_block_count);
    if (free < 0.05f) {
      return false;
    }

    auto initial_device_name = m_device->device_name();
    if (!initial_device_name) return std::unexpected(std::move(initial_device_name.error()));
    auto flatten_logged = m_logger.info("[BTreeDatabase] File '{}' is {:.2f}% free space, flattening", *initial_device_name, free * 100.f);
    if (!flatten_logged) return std::unexpected(std::move(flatten_logged.error()));

    index_block_index = m_head_free_index_block;
    {

      List<BlockIndex> available_blocks_list;
      do {
        auto free_index = read_free_index_block(index_block_index);
        if (!free_index) return std::unexpected(std::move(free_index.error()));
        FreeIndexBlock index_block = std::move(*free_index);
        available_blocks_list.append_range(index_block.free_blocks);
        available_blocks_list.push_back(index_block_index);
        index_block_index = index_block.next_free_block;
      } while (index_block_index != invalid_block_index);
      m_head_free_index_block = invalid_block_index;

      std::sort(available_blocks_list.begin(), available_blocks_list.end(), std::less<BlockIndex>());
      for (auto& available_block : available_blocks_list) {
        m_available_blocks.insert(m_available_blocks.end(), available_block);
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
      index->self = invalid_block_index;
      auto stored_root = storage.store_index(index);
      if (!stored_root) return std::unexpected(std::move(stored_root.error()));
      m_root = (*stored_root);
    }

    m_available_blocks.clear();
    auto flattened_size = header_size + static_cast<StreamOffset>(m_block_size) * count;
    m_device_size = flattened_size;

    m_index_cache.clear();
    if (auto blocks_committed = commit_writes(); !blocks_committed)
      return std::unexpected(std::move(blocks_committed.error()));

    if (auto root_published = write_root(); !root_published)
      return std::unexpected(std::move(root_published.error()));

    m_uncommitted.clear();

    if (auto resized = m_device->resize(flattened_size); !resized)
      return std::unexpected(std::move(resized.error()));

    auto finished_device_name = m_device->device_name();
    if (!finished_device_name) return std::unexpected(std::move(finished_device_name.error()));
    auto finished_logged = m_logger.info("[BTreeDatabase] Finished flattening '{}' in {:.2f} milliseconds", *finished_device_name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    if (!finished_logged) return std::unexpected(std::move(finished_logged.error()));
    return true;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::flatten_visitor(BTreeImpl::Index& index, BlockIndex& count) {
    BTreeImpl storage(*this);
    auto pointer_count = index->pointer_count();
    count += pointer_count;
    bool can_store = !m_available_blocks.empty();

    bool needs_store = false;
    if (storage.index_level(index) == 0) {

      for (std::size_t i = 0; i != pointer_count; ++i) {

        auto index_pointer = index->pointer(i);
        auto loaded_tails = leaf_tail_blocks(index_pointer);
        if (!loaded_tails) return std::unexpected(std::move(loaded_tails.error()));
        auto tail_blocks = std::move(*loaded_tails);
        if (can_store) {

          bool leaf_needs_store = *m_available_blocks.begin() < index_pointer;

          if (!leaf_needs_store) {
            for (std::size_t i = 0; !leaf_needs_store && i != tail_blocks.size(); ++i) {
              if (*m_available_blocks.begin() < tail_blocks[i]) {
                leaf_needs_store = true;
              }
            }
          }

          if (leaf_needs_store) {

            auto loaded_leaf = storage.load_leaf(index_pointer);
            if (!loaded_leaf) return std::unexpected(std::move(loaded_leaf.error()));
            auto leaf = std::move(*loaded_leaf);
            if (auto deleted_leaf = storage.delete_leaf(leaf); !deleted_leaf)
              return std::unexpected(std::move(deleted_leaf.error()));

            leaf->self = invalid_block_index;
            auto stored_leaf = storage.store_leaf(leaf);
            if (!stored_leaf) return std::unexpected(std::move(stored_leaf.error()));
            index->update_pointer(i, (*stored_leaf));
            auto new_tail_blocks = leaf_tail_blocks(leaf->self);
            if (!new_tail_blocks) return std::unexpected(std::move(new_tail_blocks.error()));
            tail_blocks = std::move(*new_tail_blocks);
            needs_store = true;
          }
          can_store = !m_available_blocks.empty();
        }
        count += tail_blocks.size();
      }
    } else {

      for (std::size_t i = 0; i != pointer_count; ++i) {

        auto loaded_child = storage.load_index(index->pointer(i));
        if (!loaded_child) return std::unexpected(std::move(loaded_child.error()));
        auto child_index = std::move(*loaded_child);
        if (can_store) {

          auto relocated = flatten_visitor(child_index, count);
          if (!relocated) return std::unexpected(std::move(relocated.error()));
          if ((*relocated)) {

            storage.delete_index(child_index);
            child_index->self = invalid_block_index;
            auto stored_child = storage.store_index(child_index);
            if (!stored_child) return std::unexpected(std::move(stored_child.error()));
            index->update_pointer(i, (*stored_child));
            can_store = !m_available_blocks.empty();
            needs_store = true;
          }
        }
      }
    }
    return needs_store || (can_store && *m_available_blocks.begin() < index->self);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_if_open(std::string_view method_name, bool should_be_open) const {
    if (should_be_open && !m_open) {
      return std::unexpected(DBException::format("BTreeDatabase method '{}' called when not open, must be open.", method_name));
    } else {
      if (!should_be_open && m_open) {
        return std::unexpected(DBException::format("BTreeDatabase method '{}' called when open, cannot call when open.", method_name));
      }
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_block_index(std::size_t block_index) const {
    BlockIndex block_count = (m_device_size - header_size) / m_block_size;
    if (block_index >= block_count) {
      return std::unexpected(DBException::format("blockIndex: {} out of block range", block_index));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::check_key_size(ByteArray const& k) const {
    if (auto state_checked = check_if_open("key access", true); !state_checked)
      return std::unexpected(std::move(state_checked.error()));

    if (k.size() != m_key_size) {
      return std::unexpected(DBException::format("Wrong key size {}", k.size()));
    }
    return {};
  }

  std::uint32_t BTreeDatabase::max_free_index_length() const {
    return (m_block_size / sizeof(BlockIndex)) - 2 - sizeof(BlockIndex) - 4;
  }

  BTreeSha256Database::BTreeSha256Database(Logger& logger) : BTreeDatabase(logger, {}, 32) {}

  [[nodiscard]] std::expected<std::unique_ptr<BTreeSha256Database>, StarException> BTreeSha256Database::create(Logger& logger, String const& content_identifier) {
    auto database = std::make_unique<BTreeSha256Database>(logger);
    if (content_identifier.empty())
      return std::unexpected(DBException("Opening new database and no content identifier set!"));
    if (content_identifier.utf8_size() > database->content_identifier_string_size)
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
