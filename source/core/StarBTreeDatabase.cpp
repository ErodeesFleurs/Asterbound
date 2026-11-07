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
    m_indexCache.setMaxSize(64);
    m_root = InvalidBlockIndex;
    m_rootIsLeaf = false;
    m_usingAltRoot = false;
  }

  BTreeDatabase::BTreeDatabase(Logger& logger, String const& contentIdentifier, std::size_t keySize)
  : BTreeDatabase(logger) {
    m_contentIdentifier = contentIdentifier;
    m_keySize = keySize;
  }

  BTreeDatabase::~BTreeDatabase() noexcept {
    // Destruction cannot report a failed transaction to its caller.  In
    // particular, never retry a failed close/commit while unwinding.
    if (m_open && (!m_availableBlocks.empty() || !m_uncommitted.empty() || !m_uncommittedWrites.empty()))
    std::fprintf(stderr, "BTreeDatabase '%s' destroyed with uncommitted changes; call commit() or close() explicitly\n",
    m_contentIdentifier.utf8Ptr());
  }

  std::uint32_t BTreeDatabase::blockSize() const {
    std::shared_lock readLocker(m_lock);
    return m_blockSize;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::setBlockSize(std::uint32_t blockSize) {
    std::unique_lock writeLocker(m_lock);
    auto result296 = checkIfOpen("setBlockSize", false);
    if (!result296) return std::unexpected(std::move(result296.error()));

    m_blockSize = blockSize;
    return {};
  }

  std::uint32_t BTreeDatabase::keySize() const {
    std::shared_lock readLocker(m_lock);
    return m_keySize;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::setKeySize(std::uint32_t keySize) {
    std::unique_lock writeLocker(m_lock);
    auto result295 = checkIfOpen("setKeySize", false);
    if (!result295) return std::unexpected(std::move(result295.error()));

    m_keySize = keySize;
    return {};
  }

  String BTreeDatabase::contentIdentifier() const {
    std::shared_lock readLocker(m_lock);
    return m_contentIdentifier;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::setContentIdentifier(String contentIdentifier) {
    std::unique_lock writeLocker(m_lock);
    auto result294 = checkIfOpen("setContentIdentifier", false);
    if (!result294) return std::unexpected(std::move(result294.error()));

    m_contentIdentifier = std::move(contentIdentifier);
    return {};
  }

  std::uint32_t BTreeDatabase::indexCacheSize() const {
    std::lock_guard lock(m_indexCacheSpinLock);
    return m_indexCache.maxSize();
  }

  void BTreeDatabase::setIndexCacheSize(std::uint32_t indexCacheSize) {
    std::lock_guard lock(m_indexCacheSpinLock);
    m_indexCache.setMaxSize(indexCacheSize);
  }

  bool BTreeDatabase::autoCommit() const {
    std::shared_lock readLocker(m_lock);
    return m_autoCommit;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::setAutoCommit(bool autoCommit) {
    std::unique_lock writeLocker(m_lock);
    m_autoCommit = autoCommit;
    if (m_autoCommit)
      return doCommit();
    return {};
  }

  std::shared_ptr<IODevice> BTreeDatabase::ioDevice() const {
    std::shared_lock readLocker(m_lock);
    return m_device;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::setIODevice(std::shared_ptr<IODevice> device) {
    std::unique_lock writeLocker(m_lock);
    auto result292 = checkIfOpen("setIODevice", false);
    if (!result292) return std::unexpected(std::move(result292.error()));

    m_device = std::move(device);
    return {};
  }

  bool BTreeDatabase::isOpen() const {
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
    bool openedDevice = m_device && !m_device->isOpen();
    std::optional<StreamOffset> initialDeviceSize;
    auto initialize = [&]() -> std::expected<bool, StarException> {

      if (!m_device) {
        return std::unexpected(DBException("BlockStorage::open called with no IODevice set"));
      }

      if (!m_device->isOpen()) {
        auto result103 = m_device->open(IOMode::ReadWrite);
        if (!result103) return std::unexpected(std::move(result103.error()));

      }

      auto result104 = m_device->size();
      if (!result104) return std::unexpected(std::move(result104.error()));
      initialDeviceSize = *result104;
      if ((*result104) > 0) {

        DataStreamIODevice ds(*m_device);
        auto result105 = ds.seek(0);
        if (!result105) return std::unexpected(std::move(result105.error()));

        auto result106 = ds.readBytes(VersionMagicSize);
        if (!result106) return std::unexpected(std::move(result106.error()));
        auto magic = std::move(*result106);
        if (magic != ByteArray(std::span(VersionMagic))) {
          return std::unexpected(DBException("Device is not a valid BTreeDatabase file"));
        }

        auto result107 = ds.read<std::uint32_t>();
        if (!result107) return std::unexpected(std::move(result107.error()));
        m_blockSize = (*result107);
        if (m_blockSize < 44) {
          return std::unexpected(DBException("Invalid database block size"));
        }

        auto result108 = ds.readBytes(ContentIdentifierStringSize);
        if (!result108) return std::unexpected(std::move(result108.error()));
        auto contentIdentifier = std::move(*result108);
        auto identifierBytes = contentIdentifier.span();
        m_contentIdentifier = String(std::string_view(identifierBytes.begin(), std::ranges::find(identifierBytes, '\0')));
        auto result109 = ds.read<std::uint32_t>();
        if (!result109) return std::unexpected(std::move(result109.error()));
        m_keySize = (*result109);
        if (m_keySize == 0 || m_keySize > m_blockSize - 11) {
          return std::unexpected(DBException("Invalid database key size"));
        }

        auto result110 = readRoot();
        if (!result110) return std::unexpected(std::move(result110.error()));

        if (m_device->isWritable()) {
          auto result111 = m_device->resize(m_deviceSize);
          if (!result111) return std::unexpected(std::move(result111.error()));

        }

        return false;
      } else {

        if (m_contentIdentifier.empty()) {
          return std::unexpected(DBException("Opening new database and no content identifier set!"));
        }
        if (m_contentIdentifier.utf8Size() > ContentIdentifierStringSize) {
          return std::unexpected(DBException("contentIdentifier in BTreeDatabase implementation is greater than maximum identifier length"));
        }
        if (m_keySize == 0 || m_blockSize < 44 || m_keySize > m_blockSize - 11) {
          return std::unexpected(DBException("Invalid key or block size opening a new BTreeDatabase"));
        }
        m_deviceSize = HeaderSize;
        auto result112 = m_device->resize(m_deviceSize);
        if (!result112) return std::unexpected(std::move(result112.error()));

        m_headFreeIndexBlock = InvalidBlockIndex;

        DataStreamIODevice ds(*m_device);
        auto result113 = ds.seek(0);
        if (!result113) return std::unexpected(std::move(result113.error()));

        auto result114 = ds.writeData(VersionMagic);
        if (!result114) return std::unexpected(std::move(result114.error()));

        auto result115 = ds.write<std::uint32_t>(m_blockSize);
        if (!result115) return std::unexpected(std::move(result115.error()));

        std::array<char, ContentIdentifierStringSize> contentIdentifier{};
        std::ranges::copy(m_contentIdentifier.utf8(), contentIdentifier.begin());
        auto result116 = ds.writeData(std::span<char const>(contentIdentifier));
        if (!result116) return std::unexpected(std::move(result116.error()));

        auto result117 = ds.write(m_keySize);
        if (!result117) return std::unexpected(std::move(result117.error()));

        auto createdRoot = m_impl.createNewRoot();
        if (!createdRoot) return std::unexpected(std::move(createdRoot.error()));
        auto result118 = doCommit();
        if (!result118) return std::unexpected(std::move(result118.error()));

        return true;
      }
    };
    auto initialized = initialize();
    if (!initialized) {

      auto failure = std::move(initialized.error());
      if (initialDeviceSize == 0 && m_device && m_device->isOpen() && m_device->isWritable()) {
        auto truncated = m_device->resize(0);
        if (!truncated)
          failure = DBException(strf("Opening database failed; removing the partial new database also failed: {}", truncated.error().what()), ExceptionDiagnostic(failure));
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
      if (openedDevice && m_device->isOpen()) {
        auto closed = m_device->close();
        if (!closed)
          failure = DBException(strf("Opening database failed; closing its device also failed: {}", closed.error().what()), ExceptionDiagnostic(failure));
      }
      return std::unexpected(std::move(failure));
    }
    m_open = true;
    return initialized;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::contains(ByteArray const& k) {
    std::shared_lock readLocker(m_lock);
    if (auto keyChecked = checkKeySize(k); !keyChecked)
      return std::unexpected(std::move(keyChecked.error()));

    return m_impl.contains(k);
  }

  [[nodiscard]] std::expected<std::optional<ByteArray>, StarException> BTreeDatabase::find(ByteArray const& k) {
    std::shared_lock readLocker(m_lock);
    if (auto keyChecked = checkKeySize(k); !keyChecked)
      return std::unexpected(std::move(keyChecked.error()));

    return m_impl.find(k);
  }

  [[nodiscard]] std::expected<List<std::pair<ByteArray, ByteArray>>, StarException> BTreeDatabase::find(ByteArray const& lower, ByteArray const& upper) {
    std::shared_lock readLocker(m_lock);
    if (auto lowerKeyChecked = checkKeySize(lower); !lowerKeyChecked)
      return std::unexpected(std::move(lowerKeyChecked.error()));

    if (auto upperKeyChecked = checkKeySize(upper); !upperKeyChecked)
      return std::unexpected(std::move(upperKeyChecked.error()));

    return m_impl.find(lower, upper);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::forEach(ByteArray const& lower, ByteArray const& upper, std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto lowerKeyChecked = checkKeySize(lower); !lowerKeyChecked)
      return std::unexpected(std::move(lowerKeyChecked.error()));

    if (auto upperKeyChecked = checkKeySize(upper); !upperKeyChecked)
      return std::unexpected(std::move(upperKeyChecked.error()));

    return m_impl.forEach(lower, upper, std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::forAll(std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v) {
    if (!v) {
      return std::unexpected(DBException("Missing database visitor"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = checkIfOpen("forAll", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.forAll(std::move(v));
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::recoverAll(std::function<std::expected<void, StarException>(ByteArray, ByteArray)> v, std::function<std::expected<void, StarException>(String const&, ExceptionDiagnostic const&)> e) {
    if (!v || !e) {
      return std::unexpected(DBException("Missing database recovery callback"));
    }
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = checkIfOpen("recoverAll", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.recoverAll(std::move(v), std::move(e));
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::insert(ByteArray const& k, ByteArray const& data) {
    std::unique_lock writeLocker(m_lock);
    auto keyChecked = checkKeySize(k);
    if (!keyChecked) return std::unexpected(std::move(keyChecked.error()));
    auto modified = m_impl.insert(k, data);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_autoCommit) {
      auto committed = doCommit();
      if (!committed) return std::unexpected(std::move(committed.error()));
    }
    return modified;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::remove(ByteArray const& k) {
    std::unique_lock writeLocker(m_lock);
    auto keyChecked = checkKeySize(k);
    if (!keyChecked) return std::unexpected(std::move(keyChecked.error()));
    auto modified = m_impl.remove(k);
    if (!modified) return std::unexpected(std::move(modified.error()));
    if (m_autoCommit) {
      auto committed = doCommit();
      if (!committed) return std::unexpected(std::move(committed.error()));
    }
    return modified;
  }

  [[nodiscard]] std::expected<List<ByteArray>, StarException> BTreeDatabase::remove(ByteArray const& lower, ByteArray const& upper) {
    auto matches = find(lower, upper);
    if (!matches) return std::unexpected(std::move(matches.error()));
    List<ByteArray> removed;
    removed.reserve(matches->size());
    for (auto const& entry : *matches) {
      auto erased = remove(entry.first);
      if (!erased) return std::unexpected(std::move(erased.error()));
      if (*erased) removed.append(entry.first);
    }
    return removed;
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> BTreeDatabase::recordCount() {
    std::shared_lock readLocker(m_lock);
    if (auto stateChecked = checkIfOpen("recordCount", true); !stateChecked)
      return std::unexpected(std::move(stateChecked.error()));

    return m_impl.recordCount();
  }

  [[nodiscard]] std::expected<std::uint8_t, StarException> BTreeDatabase::indexLevels() {
    std::shared_lock readLocker(m_lock);
    auto result269 = checkIfOpen("indexLevels", true);
    if (!result269) return std::unexpected(std::move(result269.error()));

    auto result270 = m_impl.indexLevels();
    if (!result270) return std::unexpected(std::move(result270.error()));
    return std::move(*result270);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::totalBlockCount() {
    std::shared_lock readLocker(m_lock);
    auto result267 = checkIfOpen("totalBlockCount", true);
    if (!result267) return std::unexpected(std::move(result267.error()));

    auto result268 = m_device->size();
    if (!result268) return std::unexpected(std::move(result268.error()));
    return ((*result268) - HeaderSize) / m_blockSize;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::freeBlockCount() {
    std::shared_lock readLocker(m_lock);
    auto result264 = checkIfOpen("freeBlockCount", true);
    if (!result264) return std::unexpected(std::move(result264.error()));

    // Go through every FreeIndexBlock in the chain and count all of the tracked
    // free blocks.
    BlockIndex count = 0;
    BlockIndex indexBlockIndex = m_headFreeIndexBlock;
    while (indexBlockIndex != InvalidBlockIndex) {

      auto result265 = readFreeIndexBlock(indexBlockIndex);
      if (!result265) return std::unexpected(std::move(result265.error()));
      FreeIndexBlock indexBlock = std::move(*result265);
      count += 1 + indexBlock.freeBlocks.size();
      indexBlockIndex = indexBlock.nextFreeBlock;
    }

    count += m_availableBlocks.size();

    // Include untracked blocks at the end of the file in the free count.
    auto result266 = m_device->size();
    if (!result266) return std::unexpected(std::move(result266.error()));
    count += ((*result266) - m_deviceSize) / m_blockSize;

    return count;
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::indexBlockCount() {
    std::shared_lock readLocker(m_lock);
    auto result262 = checkIfOpen("indexBlockCount", true);
    if (!result262) return std::unexpected(std::move(result262.error()));

    // Indexes are simply one index per block
    auto result263 = m_impl.indexCount();
    if (!result263) return std::unexpected(std::move(result263.error()));
    return (*result263);
  }

  [[nodiscard]] std::expected<std::uint32_t, StarException> BTreeDatabase::leafBlockCount() {
    std::unique_lock writeLocker(m_lock);
    auto result260 = checkIfOpen("leafBlockCount", true);
    if (!result260) return std::unexpected(std::move(result260.error()));

    struct LeafBlocksVisitor {
      [[nodiscard]] std::expected<bool, StarException> operator()(std::shared_ptr<IndexNode> const&) {
        return true;
      }

      [[nodiscard]] std::expected<bool, StarException> operator()(std::shared_ptr<LeafNode> const& leaf) {
        auto result129 = parent.leafTailBlocks(leaf->self);
        if (!result129) return std::unexpected(std::move(result129.error()));
        leafBlockCount += 1 + (*result129).size();
        return true;
      }

      BTreeDatabase& parent;
      BlockIndex leafBlockCount = 0;
    };

    LeafBlocksVisitor visitor{.parent = *this};
    auto result261 = m_impl.forAllNodes(visitor);
    if (!result261) return std::unexpected(std::move(result261.error()));

    return visitor.leafBlockCount;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::commit() {
    std::unique_lock writeLocker(m_lock);
    return doCommit();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::rollback() {
    std::unique_lock writeLocker(m_lock);
    auto stateChecked = checkIfOpen("rollback", true);
    if (!stateChecked) return std::unexpected(std::move(stateChecked.error()));

    m_availableBlocks.clear();
    m_indexCache.clear();
    m_uncommittedWrites.clear();
    m_uncommitted.clear();

    if (auto rootRead = readRoot(); !rootRead)
      return std::unexpected(std::move(rootRead.error()));

    if (m_device->isWritable())
      return m_device->resize(m_deviceSize);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::close(bool closeDevice) {
    std::unique_lock writeLocker(m_lock);
    if (m_open) {

      auto flattened = tryFlatten();
      if (!flattened) return std::unexpected(std::move(flattened.error()));
      if (!*flattened) {
        if (auto committed = doCommit(); !committed)
          return std::unexpected(std::move(committed.error()));
      }

      m_indexCache.clear();

      m_open = false;
      if (closeDevice && m_device && m_device->isOpen())
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

  std::size_t BTreeDatabase::IndexNode::pointerCount() const {
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

  void BTreeDatabase::IndexNode::updatePointer(std::size_t i, BlockIndex p) {
    if (i == 0)
    *beginPointer = p;
    else
    pointers[i - 1].pointer = p;
  }

  ByteArray const& BTreeDatabase::IndexNode::keyBefore(std::size_t i) const {
    return pointers[i - 1].key;
  }

  void BTreeDatabase::IndexNode::updateKeyBefore(std::size_t i, ByteArray k) { pointers[i - 1].key = std::move(k); }

  void BTreeDatabase::IndexNode::removeBefore(std::size_t i) {
    if (i == 0) {
      beginPointer = pointers[0].pointer;
      pointers.eraseAt(0);
    } else {
      pointers.eraseAt(i - 1);
    }
  }

  void BTreeDatabase::IndexNode::insertAfter(std::size_t i, ByteArray k, BlockIndex p) {
    pointers.insertAt(i, Element{.key = std::move(k), .pointer = p});
  }

  std::uint8_t BTreeDatabase::IndexNode::indexLevel() const {
    return level;
  }

  void BTreeDatabase::IndexNode::setIndexLevel(std::uint8_t indexLevel) {
    level = indexLevel;
  }

  void BTreeDatabase::IndexNode::shiftLeft(ByteArray const& mid, IndexNode& right, std::size_t count) {
    count = std::min(right.pointerCount(), count);

    if (count == 0)
    return;

    pointers.append(Element{.key = mid, .pointer = *right.beginPointer});

    ElementList::iterator s = right.pointers.begin();
    std::advance(s, count - 1);
    pointers.insert(pointers.end(), right.pointers.begin(), s);

    right.pointers.erase(right.pointers.begin(), s);
    if (right.pointers.size() != 0) {
      right.beginPointer = right.pointers[0].pointer;
      right.pointers.eraseAt(0);
    } else {
      right.beginPointer.reset();
    }
  }

  void BTreeDatabase::IndexNode::shiftRight(ByteArray const& mid, IndexNode& left, std::size_t count) {
    count = std::min(left.pointerCount(), count);

    if (count == 0)
    return;
    --count;

    pointers.insert(pointers.begin(), Element{.key = mid, .pointer = *beginPointer});

    ElementList::iterator s = left.pointers.begin();
    std::advance(s, left.pointers.size() - count);
    pointers.insert(pointers.begin(), s, left.pointers.end());

    left.pointers.erase(s, left.pointers.end());
    if (left.pointers.size() != 0) {
      beginPointer = left.pointers[left.pointers.size() - 1].pointer;
      left.pointers.eraseAt(left.pointers.size() - 1);
    } else {
      beginPointer = std::exchange(left.beginPointer, std::nullopt);
    }
  }

  ByteArray BTreeDatabase::IndexNode::split(IndexNode& right, std::size_t i) {
    ElementList::iterator s = pointers.begin();
    std::advance(s, i - 1);

    right.beginPointer = s->pointer;
    ByteArray midKey = s->key;
    right.level = level;
    ++s;

    right.pointers.insert(right.pointers.begin(), s, pointers.end());
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

  void BTreeDatabase::LeafNode::insert(std::size_t i, ByteArray k, ByteArray d) {
    elements.insertAt(i, Element{.key = std::move(k), .data = std::move(d)});
  }

  void BTreeDatabase::LeafNode::remove(std::size_t i) {
    elements.eraseAt(i);
  }

  void BTreeDatabase::LeafNode::shiftLeft(LeafNode& right, std::size_t count) {
    count = std::min(right.count(), count);

    if (count == 0)
    return;

    ElementList::iterator s = right.elements.begin();
    std::advance(s, count);

    elements.insert(elements.end(), right.elements.begin(), s);
    right.elements.erase(right.elements.begin(), s);
  }

  void BTreeDatabase::LeafNode::shiftRight(LeafNode& left, std::size_t count) {
    count = std::min(left.count(), count);

    if (count == 0)
    return;

    ElementList::iterator s = left.elements.begin();
    std::advance(s, left.elements.size() - count);

    elements.insert(elements.begin(), s, left.elements.end());
    left.elements.erase(s, left.elements.end());
  }

  void BTreeDatabase::LeafNode::split(LeafNode& right, std::size_t i) {
    ElementList::iterator s = elements.begin();
    std::advance(s, i);

    right.elements.insert(right.elements.begin(), s, elements.end());
    elements.erase(s, elements.end());
  }

  auto BTreeDatabase::BTreeImpl::rootPointer() -> Pointer {
    return parent.m_root;
  }

  bool BTreeDatabase::BTreeImpl::rootIsLeaf() {
    return parent.m_rootIsLeaf;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::setNewRoot(Pointer pointer, bool isLeaf) {
    parent.m_root = pointer;
    parent.m_rootIsLeaf = isLeaf;
    return {};
    return {};
  }

  auto BTreeDatabase::BTreeImpl::createIndex(Pointer beginPointer) -> Index {
    auto index = std::make_shared<IndexNode>();
    index->self = InvalidBlockIndex;
    index->level = 0;
    index->beginPointer = beginPointer;
    return index;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::loadIndex(Pointer pointer) -> std::expected<Index, StarException> {
    std::unique_lock lock(parent.m_indexCacheSpinLock);
    if (auto index = parent.m_indexCache.find_ref(pointer)) {
      return index->get();
    }
    lock.unlock();

    auto index = std::make_shared<IndexNode>();

    auto result247 = parent.readBlock(pointer);
    if (!result247) return std::unexpected(std::move(result247.error()));
    DataStreamBuffer buffer(std::move(*result247));

    auto result248 = buffer.readBytes(2);
    if (!result248) return std::unexpected(std::move(result248.error()));
    if ((*result248) != ByteArray(std::span(IndexMagic))) {
      return std::unexpected(DBException("Error, incorrect index block signature."));
    }

    index->self = pointer;

    auto result249 = buffer.read<std::uint8_t>();
    if (!result249) return std::unexpected(std::move(result249.error()));
    index->level = (*result249);
    auto result250 = buffer.read<std::uint32_t>();
    if (!result250) return std::unexpected(std::move(result250.error()));
    std::uint32_t s = (*result250);
    auto result251 = buffer.read<BlockIndex>();
    if (!result251) return std::unexpected(std::move(result251.error()));
    index->beginPointer = (*result251);
    if (s >= parent.maxIndexPointers()) return std::unexpected(DBException("Invalid index node pointer count"));
    index->pointers.resize(s);
    for (std::uint32_t i = 0; i < s; ++i) {

      auto& e = index->pointers[i];
      auto result252 = buffer.readBytes(parent.m_keySize);
      if (!result252) return std::unexpected(std::move(result252.error()));
      e.key = std::move(*result252);
      auto result253 = buffer.read<BlockIndex>();
      if (!result253) return std::unexpected(std::move(result253.error()));
      e.pointer = (*result253);
    }

    lock.lock();
    parent.m_indexCache.set(pointer, index);
    return index;
  }

  bool BTreeDatabase::BTreeImpl::indexNeedsShift(Index const& index) {
    return index->pointerCount() < (parent.maxIndexPointers() + 1) / 2;
  }

  bool BTreeDatabase::BTreeImpl::indexShift(Index const& left, Key const& mid, Index const& right) {
    if (left->pointerCount() + right->pointerCount() <= parent.maxIndexPointers()) {
      left->shiftLeft(mid, *right, right->pointerCount());
      return true;
    } else {
      if (indexNeedsShift(right)) {
        right->shiftRight(mid, *left, 1);
        return true;
      } else if (indexNeedsShift(left)) {
        left->shiftLeft(mid, *right, 1);
        return true;
      } else {
        return false;
      }
    }
  }

  auto BTreeDatabase::BTreeImpl::indexSplit(Index const& index) -> std::optional<std::pair<Key, Index>> {
    if (index->pointerCount() <= parent.maxIndexPointers())
    return {};

    auto right = std::make_shared<IndexNode>();
    right->self = InvalidBlockIndex;
    Key k = index->split(*right, (index->pointerCount() + 1) / 2);
    return std::make_pair(k, right);
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::storeIndex(Index index) -> std::expected<Pointer, StarException> {
    if (index->self != InvalidBlockIndex) {

      if (!parent.m_uncommitted.contains(index->self)) {

        parent.freeBlock(index->self);
        parent.m_indexCache.remove(index->self);
        index->self = InvalidBlockIndex;
      }
    }

    if (index->self == InvalidBlockIndex) {
      auto result239 = parent.reserveBlock();
      if (!result239) return std::unexpected(std::move(result239.error()));
      index->self = (*result239);
    }

    DataStreamBuffer buffer(parent.m_blockSize);
    auto result240 = buffer.writeData(IndexMagic);
    if (!result240) return std::unexpected(std::move(result240.error()));

    auto result241 = buffer.write<std::uint8_t>(index->level);
    if (!result241) return std::unexpected(std::move(result241.error()));

    auto result242 = buffer.write<std::uint32_t>(index->pointers.size());
    if (!result242) return std::unexpected(std::move(result242.error()));

    auto result243 = buffer.write<BlockIndex>(*index->beginPointer);
    if (!result243) return std::unexpected(std::move(result243.error()));

    for (auto i = index->pointers.begin(); i != index->pointers.end(); ++i) {

      auto result244 = buffer.writeBytes(i->key);
      if (!result244) return std::unexpected(std::move(result244.error()));
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((i->key.size() == parent.m_keySize));
      }
      (*result244);
      auto result245 = buffer.write<BlockIndex>(i->pointer);
      if (!result245) return std::unexpected(std::move(result245.error()));

    }

    auto result246 = parent.updateBlock(index->self, buffer.data());
    if (!result246) return std::unexpected(std::move(result246.error()));

    parent.m_indexCache.set(index->self, index);
    return index->self;
  }

  void BTreeDatabase::BTreeImpl::deleteIndex(Index index) {
    parent.m_indexCache.remove(index->self);
    parent.freeBlock(index->self);
  }

  auto BTreeDatabase::BTreeImpl::createLeaf() -> Leaf {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = InvalidBlockIndex;
    return leaf;
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::loadLeaf(Pointer pointer) -> std::expected<Leaf, StarException> {
    auto leaf = std::make_shared<LeafNode>();
    leaf->self = pointer;

    BlockIndex currentLeafBlock = leaf->self;
    DataStreamBuffer leafBuffer;
    leafBuffer.reset(parent.m_blockSize);
    auto result232 = parent.readBlock(currentLeafBlock, 0, leafBuffer.span());
    if (!result232) return std::unexpected(std::move(result232.error()));

    auto result233 = leafBuffer.readBytes(2);
    if (!result233) return std::unexpected(std::move(result233.error()));
    if ((*result233) != ByteArray(std::span(LeafMagic))) {
      return std::unexpected(DBException("Error, incorrect leaf block signature."));
    }

    DataStreamFunctions leafInput([&](std::span<char> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (left > 0) {

        auto result119 = leafBuffer.pos();
        if (!result119) return std::unexpected(std::move(result119.error()));
        if ((*result119) + left < parent.m_blockSize - sizeof(BlockIndex)) {

          auto result120 = leafBuffer.readData(data.subspan(pos, left));
          if (!result120) return std::unexpected(std::move(result120.error()));

          pos += left;
          left = 0;
        } else {

          auto result121 = leafBuffer.pos();
          if (!result121) return std::unexpected(std::move(result121.error()));
          std::size_t toRead = parent.m_blockSize - sizeof(BlockIndex) - (*result121);
          auto result122 = leafBuffer.readData(data.subspan(pos, toRead));
          if (!result122) return std::unexpected(std::move(result122.error()));

          pos += toRead;
          left -= toRead;
        }

        auto result123 = leafBuffer.pos();
        if (!result123) return std::unexpected(std::move(result123.error()));
        if ((*result123) == (parent.m_blockSize - sizeof(BlockIndex)) && left > 0) {

          auto result124 = leafBuffer.read<BlockIndex>();
          if (!result124) return std::unexpected(std::move(result124.error()));
          currentLeafBlock = (*result124);
          if (currentLeafBlock != InvalidBlockIndex) {

            leafBuffer.reset(parent.m_blockSize);
            auto result125 = parent.readBlock(currentLeafBlock, 0, leafBuffer.span());
            if (!result125) return std::unexpected(std::move(result125.error()));

            auto result126 = leafBuffer.readBytes(2);
            if (!result126) return std::unexpected(std::move(result126.error()));
            if ((*result126) != ByteArray(std::span(LeafMagic))) {
              return std::unexpected(DBException("Error, incorrect leaf block signature."));
            }
          } else {

            return std::unexpected(DBException("Leaf read off end of Leaf list."));
          }
        }
      }

      return data.size();
    }, {});

    auto result234 = leafInput.read<std::uint32_t>();
    if (!result234) return std::unexpected(std::move(result234.error()));
    std::uint32_t count = (*result234);
    if (count > static_cast<std::uint64_t>(parent.m_deviceSize - HeaderSize) / (parent.m_keySize + 1))
    return std::unexpected(DBException("Invalid leaf element count"));
    leaf->elements.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {

      auto& element = leaf->elements[i];
      auto result235 = leafInput.readBytes(parent.m_keySize);
      if (!result235) return std::unexpected(std::move(result235.error()));
      element.key = std::move(*result235);
      auto result236 = leafInput.read<ByteArray>();
      if (!result236) return std::unexpected(std::move(result236.error()));
      element.data = std::move(*result236);
    }

    return leaf;
  }

  bool BTreeDatabase::BTreeImpl::leafNeedsShift(Leaf const& l) {
    return parent.leafSize(l) < parent.m_blockSize / 2;
  }

  bool BTreeDatabase::BTreeImpl::leafShift(Leaf& left, Leaf& right) {
    if (left->count() == 0) {
      left->shiftLeft(*right, right->count());
      return true;
    }

    if (right->count() == 0)
    return true;

    std::uint32_t leftSize = parent.leafSize(left);
    std::uint32_t rightSize = parent.leafSize(right);
    if (leftSize + rightSize < parent.m_blockSize) {
      left->shiftLeft(*right, right->count());
      return true;
    }

    // TODO: Shifting algorithm is bad, could potentially want to shift more
    // than one element here.
    std::uint32_t rightBeginSize = parent.m_keySize + parent.dataSize(right->elements[0].data);
    std::uint32_t leftEndSize = parent.m_keySize + parent.dataSize(left->elements[left->elements.size() - 1].data);
    if (leftSize < rightSize - rightBeginSize && leftSize + rightBeginSize < parent.m_blockSize) {
      left->shiftLeft(*right, 1);
      return true;
    } else if (rightSize < leftSize - leftEndSize && rightSize + leftEndSize < parent.m_blockSize) {
      right->shiftRight(*left, 1);
      return true;
    }

    return false;
  }

  auto BTreeDatabase::BTreeImpl::leafSplit(Leaf& leaf) -> std::optional<Leaf> {
    if (leaf->elements.size() < 2)
    return {};

    std::uint32_t size = 6;
    bool boundaryFound = false;
    std::uint32_t boundary = 0;
    for (std::uint32_t i = 0; i < leaf->elements.size(); ++i) {
      size += parent.m_keySize;
      size += parent.dataSize(leaf->elements[i].data);
      if (size > parent.m_blockSize - sizeof(BlockIndex) && !boundaryFound) {
        boundary = i;
        boundaryFound = true;
      }
    }
    if (boundary == 0)
    boundary = 1;

    if (size < parent.m_blockSize * 2 - 2 * sizeof(BlockIndex) - 4) {
      return {};
    } else {
      auto right = std::make_shared<LeafNode>();
      right->self = InvalidBlockIndex;
      leaf->split(*right, boundary);
      return right;
    }
  }

  [[nodiscard]] auto BTreeDatabase::BTreeImpl::storeLeaf(Leaf leaf) -> std::expected<Pointer, StarException> {
    if (leaf->self != InvalidBlockIndex) {

      auto result222 = parent.leafTailBlocks(leaf->self);
      if (!result222) return std::unexpected(std::move(result222.error()));
      List<BlockIndex> tailBlocks = std::move(*result222);
      for (std::uint32_t i = 0; i < tailBlocks.size(); ++i) {
        parent.freeBlock(tailBlocks[i]);
      }

      if (!parent.m_uncommitted.contains(leaf->self)) {

        parent.freeBlock(leaf->self);
        leaf->self = InvalidBlockIndex;
      }
    }

    if (leaf->self == InvalidBlockIndex) {
      auto result224 = parent.reserveBlock();
      if (!result224) return std::unexpected(std::move(result224.error()));
      leaf->self = (*result224);
    }

    BlockIndex currentLeafBlock = leaf->self;
    DataStreamBuffer leafBuffer;
    leafBuffer.reset(parent.m_blockSize);
    auto result225 = leafBuffer.writeData(LeafMagic);
    if (!result225) return std::unexpected(std::move(result225.error()));

    DataStreamFunctions leafOutput({}, [&](std::span<char const> data) -> std::expected<std::size_t, StarException> {
      std::size_t pos = 0;
      std::size_t left = data.size();

      while (true) {
        auto bufferPosition = leafBuffer.pos();
        if (!bufferPosition) return std::unexpected(std::move(bufferPosition.error()));
        std::size_t toWrite = std::min(left, parent.m_blockSize - *bufferPosition - sizeof(BlockIndex));

        if (toWrite != 0) {
          auto written = leafBuffer.writeData(data.subspan(pos, toWrite));
          if (!written) return std::unexpected(std::move(written.error()));
          left -= toWrite;
          pos += toWrite;
        }

        if (left == 0)
          break;

        if (*bufferPosition + toWrite == parent.m_blockSize - sizeof(BlockIndex)) {
          auto nextBlock = parent.reserveBlock();
          if (!nextBlock) return std::unexpected(std::move(nextBlock.error()));
          auto linked = leafBuffer.write<BlockIndex>(*nextBlock);
          if (!linked) return std::unexpected(std::move(linked.error()));
          auto updated = parent.updateBlock(currentLeafBlock, leafBuffer.data());
          if (!updated) return std::unexpected(std::move(updated.error()));
          currentLeafBlock = *nextBlock;
          leafBuffer.reset(parent.m_blockSize);
          auto marked = leafBuffer.writeData(LeafMagic);
          if (!marked) return std::unexpected(std::move(marked.error()));
        }
      }

      return data.size();
    });

    auto result226 = leafOutput.write<std::uint32_t>(leaf->elements.size());
    if (!result226) return std::unexpected(std::move(result226.error()));

    for (LeafNode::ElementList::iterator i = leaf->elements.begin(); i != leaf->elements.end(); ++i) {

      auto result227 = leafOutput.writeBytes(i->key);
      if (!result227) return std::unexpected(std::move(result227.error()));
      if constexpr (Star::DebugEnabled) {
        Star::starAssertFailed((i->key.size() == parent.m_keySize));
      }
      auto result228 = leafOutput.write(i->data);
      if (!result228) return std::unexpected(std::move(result228.error()));

    }

    auto result229 = leafBuffer.seek(parent.m_blockSize - sizeof(BlockIndex));
    if (!result229) return std::unexpected(std::move(result229.error()));

    auto result230 = leafBuffer.write<BlockIndex>(InvalidBlockIndex);
    if (!result230) return std::unexpected(std::move(result230.error()));

    auto result231 = parent.updateBlock(currentLeafBlock, leafBuffer.data());
    if (!result231) return std::unexpected(std::move(result231.error()));

    return leaf->self;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::BTreeImpl::deleteLeaf(Leaf leaf) {
    auto result221 = parent.leafTailBlocks(leaf->self);
    if (!result221) return std::unexpected(std::move(result221.error()));
    List<BlockIndex> tailBlocks = std::move(*result221);
    for (std::uint32_t i = 0; i < tailBlocks.size(); ++i) {
      parent.freeBlock(tailBlocks[i]);
    }

    parent.freeBlock(leaf->self);
    return {};
  }

  std::size_t BTreeDatabase::BTreeImpl::indexPointerCount(Index const& index) {
    return index->pointerCount();
  }

  auto BTreeDatabase::BTreeImpl::indexPointer(Index const& index, std::size_t i) -> Pointer {
    return index->pointer(i);
  }

  void BTreeDatabase::BTreeImpl::indexUpdatePointer(Index& index, std::size_t i, Pointer p) {
    index->updatePointer(i, p);
  }

  auto BTreeDatabase::BTreeImpl::indexKeyBefore(Index const& index, std::size_t i) -> Key {
    return index->keyBefore(i);
  }

  void BTreeDatabase::BTreeImpl::indexUpdateKeyBefore(Index& index, std::size_t i, Key k) {
    index->updateKeyBefore(i, std::move(k));
  }

  void BTreeDatabase::BTreeImpl::indexRemoveBefore(Index& index, std::size_t i) {
    index->removeBefore(i);
  }

  void BTreeDatabase::BTreeImpl::indexInsertAfter(Index& index, std::size_t i, Key k, Pointer p) {
    index->insertAfter(i, std::move(k), p);
  }

  std::size_t BTreeDatabase::BTreeImpl::indexLevel(Index const& index) {
    return index->indexLevel();
  }

  void BTreeDatabase::BTreeImpl::setIndexLevel(Index& index, std::size_t indexLevel) {
    index->setIndexLevel(indexLevel);
  }

  std::size_t BTreeDatabase::BTreeImpl::leafElementCount(Leaf const& leaf) {
    return leaf->count();
  }

  auto BTreeDatabase::BTreeImpl::leafKey(Leaf const& leaf, std::size_t i) -> Key {
    return leaf->key(i);
  }

  auto BTreeDatabase::BTreeImpl::leafData(Leaf const& leaf, std::size_t i) -> Data {
    return leaf->data(i);
  }

  void BTreeDatabase::BTreeImpl::leafInsert(Leaf& leaf, std::size_t i, Key k, Data d) {
    leaf->insert(i, std::move(k), std::move(d));
  }

  void BTreeDatabase::BTreeImpl::leafRemove(Leaf& leaf, std::size_t i) {
    leaf->remove(i);
  }

  auto BTreeDatabase::BTreeImpl::nextLeaf(Leaf const&) -> std::optional<Pointer> {
    return {};
  }

  void BTreeDatabase::BTreeImpl::setNextLeaf(Leaf&, std::optional<Pointer>) {}

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::readBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const {
    if (auto blockChecked = checkBlockIndex(blockIndex); !blockChecked)
      return std::unexpected(std::move(blockChecked.error()));

    return rawReadBlock(blockIndex, blockOffset, block);
  }

  [[nodiscard]] std::expected<ByteArray, StarException> BTreeDatabase::readBlock(BlockIndex blockIndex) const {
    ByteArray block(m_blockSize, 0);
    auto result218 = readBlock(blockIndex, 0, block.span());
    if (!result218) return std::unexpected(std::move(result218.error()));

    return block;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::updateBlock(BlockIndex blockIndex, ByteArray const& block) {
    if (auto blockChecked = checkBlockIndex(blockIndex); !blockChecked)
      return std::unexpected(std::move(blockChecked.error()));

    return rawWriteBlock(blockIndex, 0, block.span());
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::rawReadBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char> block) const {
    auto size = block.size();
    if (blockOffset > m_blockSize || size > m_blockSize - blockOffset) {
      return std::unexpected(DBException::format("Read past end of block, offset: {} size {}", blockOffset, size));
    }

    if (size <= 0) {
      return {};
    }

    if (auto buffer = m_uncommittedWrites.find_ref(blockIndex)) {
      buffer->get().copyTo(block, blockOffset);
    } else {
      return m_device->readFullAbsolute(HeaderSize + blockIndex * static_cast<StreamOffset>(m_blockSize) + blockOffset,
                                        block);
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::rawWriteBlock(BlockIndex blockIndex, std::size_t blockOffset, std::span<char const> block) {
    auto size = block.size();
    if (blockOffset > m_blockSize || size > m_blockSize - blockOffset) {
      return std::unexpected(DBException::format("Write past end of block, offset: {} size {}", blockOffset, size));
    }

    if (size <= 0) {
      return {};
    }

    StreamOffset blockStart = HeaderSize + blockIndex * static_cast<StreamOffset>(m_blockSize);
    auto buffer = m_uncommittedWrites.find(blockIndex);
    if (buffer == m_uncommittedWrites.end()) {
      auto result214 = m_device->readBytesAbsolute(blockStart, m_blockSize);
      if (!result214) return std::unexpected(std::move(result214.error()));
      buffer = m_uncommittedWrites.emplace(blockIndex, std::move(*result214)).first;
    }

    buffer->second.writeFrom(block, blockOffset);
    return {};
  }

  [[nodiscard]] auto BTreeDatabase::readFreeIndexBlock(BlockIndex blockIndex) -> std::expected<FreeIndexBlock, StarException> {
    auto result202 = checkBlockIndex(blockIndex);
    if (!result202) return std::unexpected(std::move(result202.error()));

    ByteArray magic(2, 0);
    auto result203 = rawReadBlock(blockIndex, 0, magic.span());
    if (!result203) return std::unexpected(std::move(result203.error()));

    if (magic != ByteArray(std::span(FreeIndexMagic))) {
      return std::unexpected(DBException::format("Internal exception! block {} missing free index block marker!", blockIndex));
    }

    FreeIndexBlock freeIndexBlock;
    DataStreamBuffer buffer(std::max(sizeof(BlockIndex), static_cast<std::size_t>(4)));

    auto result204 = rawReadBlock(blockIndex, 2, buffer.span().first(sizeof(BlockIndex)));
    if (!result204) return std::unexpected(std::move(result204.error()));

    auto result205 = buffer.seek(0);
    if (!result205) return std::unexpected(std::move(result205.error()));

    auto result206 = buffer.read<BlockIndex>();
    if (!result206) return std::unexpected(std::move(result206.error()));
    freeIndexBlock.nextFreeBlock = (*result206);

    auto result207 = rawReadBlock(blockIndex, 2 + sizeof(BlockIndex), buffer.span().first(4));
    if (!result207) return std::unexpected(std::move(result207.error()));

    auto result208 = buffer.seek(0);
    if (!result208) return std::unexpected(std::move(result208.error()));

    auto result209 = buffer.read<std::uint32_t>();
    if (!result209) return std::unexpected(std::move(result209.error()));
    std::size_t numFree = (*result209);

    for (std::size_t i = 0; i < numFree; ++i) {

      auto result210 = rawReadBlock(blockIndex, 6 + sizeof(BlockIndex) + sizeof(BlockIndex) * i, buffer.span().first(sizeof(BlockIndex)));
      if (!result210) return std::unexpected(std::move(result210.error()));

      auto result211 = buffer.seek(0);
      if (!result211) return std::unexpected(std::move(result211.error()));

      auto result212 = buffer.read<BlockIndex>();
      if (!result212) return std::unexpected(std::move(result212.error()));
      freeIndexBlock.freeBlocks.append((*result212));
    }

    return freeIndexBlock;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::writeFreeIndexBlock(BlockIndex blockIndex, FreeIndexBlock indexBlock) {
    auto result191 = checkBlockIndex(blockIndex);
    if (!result191) return std::unexpected(std::move(result191.error()));

    auto result192 = rawWriteBlock(blockIndex, 0, FreeIndexMagic);
    if (!result192) return std::unexpected(std::move(result192.error()));

    DataStreamBuffer buffer(std::max(sizeof(BlockIndex), static_cast<std::size_t>(4)));

    auto result193 = buffer.seek(0);
    if (!result193) return std::unexpected(std::move(result193.error()));

    auto result194 = buffer.write<BlockIndex>(indexBlock.nextFreeBlock);
    if (!result194) return std::unexpected(std::move(result194.error()));

    auto result195 = rawWriteBlock(blockIndex, 2, buffer.span().first(sizeof(BlockIndex)));
    if (!result195) return std::unexpected(std::move(result195.error()));

    auto result196 = buffer.seek(0);
    if (!result196) return std::unexpected(std::move(result196.error()));

    auto result197 = buffer.write<std::uint32_t>(indexBlock.freeBlocks.size());
    if (!result197) return std::unexpected(std::move(result197.error()));

    auto result198 = rawWriteBlock(blockIndex, 2 + sizeof(BlockIndex), buffer.span().first(4));
    if (!result198) return std::unexpected(std::move(result198.error()));

    for (std::size_t i = 0; i < indexBlock.freeBlocks.size(); ++i) {

      auto result199 = buffer.seek(0);
      if (!result199) return std::unexpected(std::move(result199.error()));

      auto result200 = buffer.write<BlockIndex>(indexBlock.freeBlocks[i]);
      if (!result200) return std::unexpected(std::move(result200.error()));

      auto result201 = rawWriteBlock(blockIndex, 6 + sizeof(BlockIndex) + sizeof(BlockIndex) * i, buffer.span().first(sizeof(BlockIndex)));
      if (!result201) return std::unexpected(std::move(result201.error()));

    }
    return {};
  }

  std::uint32_t BTreeDatabase::leafSize(std::shared_ptr<LeafNode> const& leaf) const {
    std::size_t s = 6;
    for (LeafNode::ElementList::iterator i = leaf->elements.begin(); i != leaf->elements.end(); ++i) {
      s += m_keySize;
      s += dataSize(i->data);
    }
    return s;
  }

  std::uint32_t BTreeDatabase::maxIndexPointers() const {
    // 2 for magic, 1 byte for level, sizeof(BlockIndex) for beginPointer, 4
    // for size.
    return (m_blockSize - 2 - 1 - sizeof(BlockIndex) - 4) / (m_keySize + sizeof(BlockIndex)) + 1;
  }

  std::uint32_t BTreeDatabase::dataSize(ByteArray const& d) const {
    return vlqUSize(d.size()) + d.size();
  }

  [[nodiscard]] auto BTreeDatabase::leafTailBlocks(BlockIndex leafPointer) -> std::expected<List<BlockIndex>, StarException> {
    List<BlockIndex> tailBlocks;
    DataStreamBuffer pointerBuffer(sizeof(BlockIndex));
    while (leafPointer != InvalidBlockIndex) {

      auto result188 = readBlock(leafPointer, m_blockSize - sizeof(BlockIndex), pointerBuffer.span().first(sizeof(BlockIndex)));
      if (!result188) return std::unexpected(std::move(result188.error()));

      auto result189 = pointerBuffer.seek(0);
      if (!result189) return std::unexpected(std::move(result189.error()));

      auto result190 = pointerBuffer.read<BlockIndex>();
      if (!result190) return std::unexpected(std::move(result190.error()));
      leafPointer = (*result190);
      if (leafPointer != InvalidBlockIndex) {
        tailBlocks.append(leafPointer);
      }
    }
    return tailBlocks;
  }

  void BTreeDatabase::freeBlock(BlockIndex b) {
    if (m_uncommitted.contains(b))
    m_uncommitted.remove(b);
    if (m_uncommittedWrites.contains(b))
    m_uncommittedWrites.remove(b);

    m_availableBlocks.add(b);
  }

  [[nodiscard]] auto BTreeDatabase::reserveBlock() -> std::expected<BlockIndex, StarException> {
    if (m_availableBlocks.empty()) {

      if (m_headFreeIndexBlock != InvalidBlockIndex) {

        // If available, make available all the blocks in the first free index
        // block.
        auto result185 = readFreeIndexBlock(m_headFreeIndexBlock);
        if (!result185) return std::unexpected(std::move(result185.error()));
        FreeIndexBlock indexBlock = std::move(*result185);
        for (auto const& b : indexBlock.freeBlocks) {
          m_availableBlocks.add(b);
        }
        m_availableBlocks.add(m_headFreeIndexBlock);
        m_headFreeIndexBlock = indexBlock.nextFreeBlock;
      }

      if (m_availableBlocks.empty()) {

        // If we still don't have any available blocks, just add a block to the
        // end of the file.
        auto result186 = makeEndBlock();
        if (!result186) return std::unexpected(std::move(result186.error()));
        m_availableBlocks.add((*result186));
      }
    }

    auto result187 = m_availableBlocks.takeFirst();
    if (!result187) return std::unexpected(std::move(result187.error()));
    BlockIndex block = (*result187);
    m_uncommitted.add(block);
    return block;
  }

  [[nodiscard]] auto BTreeDatabase::makeEndBlock() -> std::expected<BlockIndex, StarException> {
    BlockIndex blockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    auto result184 = m_device->resize(m_deviceSize + m_blockSize);
    if (!result184) return std::unexpected(std::move(result184.error()));

    m_deviceSize += m_blockSize;
    return blockCount;
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::writeRoot() {
    DataStreamIODevice ds(*m_device);
    // First write the root info to whichever section we are not currently using
    if (auto rootPositioned = ds.seek(BTreeRootInfoStart + (m_usingAltRoot ? 0 : BTreeRootInfoSize)); !rootPositioned)
      return std::unexpected(std::move(rootPositioned.error()));

    if (auto freeIndexWritten = ds.write<BlockIndex>(m_headFreeIndexBlock); !freeIndexWritten)
      return std::unexpected(std::move(freeIndexWritten.error()));

    if (auto sizeWritten = ds.write<StreamOffset>(m_deviceSize); !sizeWritten)
      return std::unexpected(std::move(sizeWritten.error()));

    if (auto rootWritten = ds.write<BlockIndex>(m_root); !rootWritten)
      return std::unexpected(std::move(rootWritten.error()));

    if (auto leafFlagWritten = ds.write<bool>(m_rootIsLeaf); !leafFlagWritten)
      return std::unexpected(std::move(leafFlagWritten.error()));

    // Then flush all the pending changes.
    if (auto changesSynced = m_device->sync(); !changesSynced)
      return std::unexpected(std::move(changesSynced.error()));

    // Then switch headers by writing the single bit that switches them
    bool usingAltRoot = !m_usingAltRoot;
    if (auto selectorPositioned = ds.seek(BTreeRootSelectorBit); !selectorPositioned)
      return std::unexpected(std::move(selectorPositioned.error()));

    if (auto selectorWritten = ds.write(usingAltRoot); !selectorWritten)
      return std::unexpected(std::move(selectorWritten.error()));

    m_usingAltRoot = usingAltRoot;

    // Then flush this single bit write to make sure it happens before anything
    // else.
    return m_device->sync();
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::readRoot() {
    DataStreamIODevice ds(*m_device);
    auto result167 = ds.seek(BTreeRootSelectorBit);
    if (!result167) return std::unexpected(std::move(result167.error()));

    auto result168 = ds.read<bool>();
    if (!result168) return std::unexpected(std::move(result168.error()));
    auto usingAltRoot = std::move(*result168);
    auto result169 = ds.seek(BTreeRootInfoStart + (usingAltRoot ? BTreeRootInfoSize : 0));
    if (!result169) return std::unexpected(std::move(result169.error()));

    auto result170 = ds.read<BlockIndex>();
    if (!result170) return std::unexpected(std::move(result170.error()));
    auto head = std::move(*result170);
    auto result171 = ds.read<StreamOffset>();
    if (!result171) return std::unexpected(std::move(result171.error()));
    auto size = std::move(*result171);
    auto result172 = ds.read<BlockIndex>();
    if (!result172) return std::unexpected(std::move(result172.error()));
    auto root = std::move(*result172);
    auto result173 = ds.read<bool>();
    if (!result173) return std::unexpected(std::move(result173.error()));
    auto rootIsLeaf = std::move(*result173);
    auto result174 = m_device->size();
    if (!result174) return std::unexpected(std::move(result174.error()));
    auto deviceSize = std::move(*result174);
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
    m_rootIsLeaf = rootIsLeaf;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::doCommit() {
    if (m_availableBlocks.empty() && m_uncommitted.empty()) {
      return {};
    }

    if (!m_availableBlocks.empty()) {

      // First, read the existing head FreeIndexBlock, if it exists
      FreeIndexBlock indexBlock = FreeIndexBlock{.nextFreeBlock = InvalidBlockIndex, .freeBlocks = {}};

      auto newBlock = [&]() -> std::expected<BlockIndex, StarException> {
        if (!m_availableBlocks.empty())
          return m_availableBlocks.takeFirst();
        return makeEndBlock();
      };

      if (m_headFreeIndexBlock != InvalidBlockIndex) {
        auto result159 = readFreeIndexBlock(m_headFreeIndexBlock);
        if (!result159) return std::unexpected(std::move(result159.error()));
        indexBlock = (*result159);
      } else {
        auto result160 = newBlock();
        if (!result160) return std::unexpected(std::move(result160.error()));
        m_headFreeIndexBlock = (*result160);
      }

      // Then, we need to write all the available blocks to the FreeIndexBlock chain.
      while (true) {

        // If we have room on our current FreeIndexBlock, just add a block to it.
        if (!m_availableBlocks.empty() && indexBlock.freeBlocks.size() < maxFreeIndexLength()) {

          auto result161 = m_availableBlocks.takeFirst();
          if (!result161) return std::unexpected(std::move(result161.error()));
          BlockIndex toAdd = (*result161);
          indexBlock.freeBlocks.append(toAdd);
        } else {

          // Update the current head free index block.
          auto result162 = writeFreeIndexBlock(m_headFreeIndexBlock, indexBlock);
          if (!result162) return std::unexpected(std::move(result162.error()));

          // If we're out of blocks to free, then we're done
          if (m_availableBlocks.empty()) {
            break;
          }

          // If our head free index block is full, then
          // need to write a new head free index block.
          if (indexBlock.freeBlocks.size() >= maxFreeIndexLength()) {

            indexBlock.nextFreeBlock = m_headFreeIndexBlock;
            indexBlock.freeBlocks.clear();

            auto result163 = newBlock();
            if (!result163) return std::unexpected(std::move(result163.error()));
            m_headFreeIndexBlock = (*result163);
            auto result164 = writeFreeIndexBlock(m_headFreeIndexBlock, indexBlock);
            if (!result164) return std::unexpected(std::move(result164.error()));

          }
        }
      }
    }

    auto result165 = commitWrites();
    if (!result165) return std::unexpected(std::move(result165.error()));

    auto result166 = writeRoot();
    if (!result166) return std::unexpected(std::move(result166.error()));

    m_uncommitted.clear();
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::commitWrites() {
    for (auto& write : m_uncommittedWrites) {
      auto result157 = m_device->writeFullAbsolute(HeaderSize + write.first * static_cast<StreamOffset>(m_blockSize),
                                                   write.second.span().first(m_blockSize));
      if (!result157) return std::unexpected(std::move(result157.error()));

    }

    auto result158 = m_device->sync();
    if (!result158) return std::unexpected(std::move(result158.error()));

    m_uncommittedWrites.clear();
    return {};
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::tryFlatten() {
    if (m_headFreeIndexBlock == InvalidBlockIndex || m_rootIsLeaf || !m_device->isWritable()) {
      return false;
    }

    BlockIndex freeBlockCount = 0;
    BlockIndex indexBlockIndex = m_headFreeIndexBlock;
    while (indexBlockIndex != InvalidBlockIndex) {

      auto result147 = readFreeIndexBlock(indexBlockIndex);
      if (!result147) return std::unexpected(std::move(result147.error()));
      FreeIndexBlock indexBlock = std::move(*result147);
      freeBlockCount += 1 + indexBlock.freeBlocks.size();
      indexBlockIndex = indexBlock.nextFreeBlock;
    }

    BlockIndex expectedBlockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    float free = float(freeBlockCount) / float(expectedBlockCount);
    if (free < 0.05f) {
      return false;
    }

    auto result148 = m_device->deviceName();
    if (!result148) return std::unexpected(std::move(result148.error()));
    auto flattenLogged = m_logger.info("[BTreeDatabase] File '{}' is {:.2f}% free space, flattening", *result148, free * 100.f);
    if (!flattenLogged) return std::unexpected(std::move(flattenLogged.error()));

    indexBlockIndex = m_headFreeIndexBlock;
    {

      List<BlockIndex> availableBlocksList;
      do {
        auto freeIndex = readFreeIndexBlock(indexBlockIndex);
        if (!freeIndex) return std::unexpected(std::move(freeIndex.error()));
        FreeIndexBlock indexBlock = std::move(*freeIndex);
        availableBlocksList.appendAll(indexBlock.freeBlocks);
        availableBlocksList.append(indexBlockIndex);
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
    auto result150 = storage.loadIndex(storage.rootPointer());
    if (!result150) return std::unexpected(std::move(result150.error()));
    auto index = std::move(*result150);
    auto result151 = flattenVisitor(index, count);
    if (!result151) return std::unexpected(std::move(result151.error()));
    if ((*result151)) {

      storage.deleteIndex(index);
      index->self = InvalidBlockIndex;
      auto result152 = storage.storeIndex(index);
      if (!result152) return std::unexpected(std::move(result152.error()));
      m_root = (*result152);
    }

    m_availableBlocks.clear();
    auto flattenedSize = HeaderSize + static_cast<StreamOffset>(m_blockSize) * count;
    m_deviceSize = flattenedSize;

    m_indexCache.clear();
    auto result153 = commitWrites();
    if (!result153) return std::unexpected(std::move(result153.error()));

    auto result154 = writeRoot();
    if (!result154) return std::unexpected(std::move(result154.error()));

    m_uncommitted.clear();

    auto result155 = m_device->resize(flattenedSize);
    if (!result155) return std::unexpected(std::move(result155.error()));

    auto result156 = m_device->deviceName();
    if (!result156) return std::unexpected(std::move(result156.error()));
    auto finishedLogged = m_logger.info("[BTreeDatabase] Finished flattening '{}' in {:.2f} milliseconds", *result156, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    if (!finishedLogged) return std::unexpected(std::move(finishedLogged.error()));
    return true;
  }

  [[nodiscard]] std::expected<bool, StarException> BTreeDatabase::flattenVisitor(BTreeImpl::Index& index, BlockIndex& count) {
    BTreeImpl storage(*this);
    auto pointerCount = index->pointerCount();
    count += pointerCount;
    bool canStore = !m_availableBlocks.empty();

    bool needsStore = false;
    if (storage.indexLevel(index) == 0) {

      for (std::size_t i = 0; i != pointerCount; ++i) {

        auto indexPointer = index->pointer(i);
        auto result139 = leafTailBlocks(indexPointer);
        if (!result139) return std::unexpected(std::move(result139.error()));
        auto tailBlocks = std::move(*result139);
        if (canStore) {

          bool leafNeedsStore = *m_availableBlocks.begin() < indexPointer;

          if (!leafNeedsStore) {
            for (std::size_t i = 0; !leafNeedsStore && i != tailBlocks.size(); ++i) {
              if (*m_availableBlocks.begin() < tailBlocks[i]) {
                leafNeedsStore = true;
              }
            }
          }

          if (leafNeedsStore) {

            auto result140 = storage.loadLeaf(indexPointer);
            if (!result140) return std::unexpected(std::move(result140.error()));
            auto leaf = std::move(*result140);
            auto result141 = storage.deleteLeaf(leaf);
            if (!result141) return std::unexpected(std::move(result141.error()));

            leaf->self = InvalidBlockIndex;
            auto result142 = storage.storeLeaf(leaf);
            if (!result142) return std::unexpected(std::move(result142.error()));
            index->updatePointer(i, (*result142));
            auto result143 = leafTailBlocks(leaf->self);
            if (!result143) return std::unexpected(std::move(result143.error()));
            tailBlocks = (*result143);
            needsStore = true;
          }
          canStore = !m_availableBlocks.empty();
        }
        count += tailBlocks.size();
      }
    } else {

      for (std::size_t i = 0; i != pointerCount; ++i) {

        auto result144 = storage.loadIndex(index->pointer(i));
        if (!result144) return std::unexpected(std::move(result144.error()));
        auto childIndex = std::move(*result144);
        if (canStore) {

          auto result145 = flattenVisitor(childIndex, count);
          if (!result145) return std::unexpected(std::move(result145.error()));
          if ((*result145)) {

            storage.deleteIndex(childIndex);
            childIndex->self = InvalidBlockIndex;
            auto result146 = storage.storeIndex(childIndex);
            if (!result146) return std::unexpected(std::move(result146.error()));
            index->updatePointer(i, (*result146));
            canStore = !m_availableBlocks.empty();
            needsStore = true;
          }
        }
      }
    }
    return needsStore || (canStore && *m_availableBlocks.begin() < index->self);
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::checkIfOpen(std::string_view methodName, bool shouldBeOpen) const {
    if (shouldBeOpen && !m_open) {
      return std::unexpected(DBException::format("BTreeDatabase method '{}' called when not open, must be open.", methodName));
    } else {
      if (!shouldBeOpen && m_open) {
        return std::unexpected(DBException::format("BTreeDatabase method '{}' called when open, cannot call when open.", methodName));
      }
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::checkBlockIndex(std::size_t blockIndex) const {
    BlockIndex blockCount = (m_deviceSize - HeaderSize) / m_blockSize;
    if (blockIndex >= blockCount) {
      return std::unexpected(DBException::format("blockIndex: {} out of block range", blockIndex));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> BTreeDatabase::checkKeySize(ByteArray const& k) const {
    auto result138 = checkIfOpen("key access", true);
    if (!result138) return std::unexpected(std::move(result138.error()));

    if (k.size() != m_keySize) {
      return std::unexpected(DBException::format("Wrong key size {}", k.size()));
    }
    return {};
  }

  std::uint32_t BTreeDatabase::maxFreeIndexLength() const {
    return (m_blockSize / sizeof(BlockIndex)) - 2 - sizeof(BlockIndex) - 4;
  }

  BTreeSha256Database::BTreeSha256Database(Logger& logger) : BTreeDatabase(logger, {}, 32) {}

  [[nodiscard]] std::expected<std::unique_ptr<BTreeSha256Database>, StarException> BTreeSha256Database::create(Logger& logger, String const& contentIdentifier) {
    auto database = std::make_unique<BTreeSha256Database>(logger);
    if (contentIdentifier.empty())
      return std::unexpected(DBException("Opening new database and no content identifier set!"));
    if (contentIdentifier.utf8Size() > database->ContentIdentifierStringSize)
      return std::unexpected(DBException("contentIdentifier in BTreeDatabase implementation is greater than maximum identifier length"));
    auto configured = database->setContentIdentifier(contentIdentifier);
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

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::remove(ByteArray const& key) {
    return BTreeDatabase::remove(sha256(key));
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

  [[nodiscard]] std::expected<bool, StarException> BTreeSha256Database::remove(String const& key) {
    return BTreeDatabase::remove(sha256(key));
  }

}
