import std;
import star.thread;
import star.format;

namespace Star {

#ifdef STAR_SYSTEM_WINDOWS
void lockWithDiagnostics(std::unique_lock<std::recursive_mutex>& lock) {
  lock.lock();
}
#endif

ReadersWriterMutex::ReadersWriterMutex()
  : m_readers(), m_writers(), m_readWaiters(), m_writeWaiters() {}

void ReadersWriterMutex::readLock() {
  std::unique_lock locker(m_mutex);
  if (m_writers || m_writeWaiters) {
    m_readWaiters++;
    while (m_writers || m_writeWaiters)
      m_readCond.wait(locker);
    m_readWaiters--;
  }
  m_readers++;
}

bool ReadersWriterMutex::tryReadLock() {
  std::lock_guard locker(m_mutex);
  if (m_writers || m_writeWaiters)
    return false;
  m_readers++;
  return true;
}

void ReadersWriterMutex::readUnlock() {
  std::lock_guard locker(m_mutex);
  m_readers--;
  if (m_writeWaiters)
    m_writeCond.notify_one();
}

void ReadersWriterMutex::writeLock() {
  std::unique_lock locker(m_mutex);
  if (m_readers || m_writers) {
    m_writeWaiters++;
    while (m_readers || m_writers)
      m_writeCond.wait(locker);
    m_writeWaiters--;
  }
  m_writers = 1;
}

bool ReadersWriterMutex::tryWriteLock() {
  std::lock_guard locker(m_mutex);
  if (m_readers || m_writers)
    return false;
  m_writers = 1;
  return true;
}

void ReadersWriterMutex::writeUnlock() {
  std::lock_guard locker(m_mutex);
  m_writers = 0;
  if (m_writeWaiters)
    m_writeCond.notify_one();
  else if (m_readWaiters)
    m_readCond.notify_all();
}

ReadLocker::ReadLocker(ReadersWriterMutex& rwlock, bool startLocked) : m_lock(rwlock), m_locked(false) {
  if (startLocked)
    lock();
}

ReadLocker::~ReadLocker() {
  unlock();
}

void ReadLocker::unlock() {
  if (m_locked)
    m_lock.readUnlock();
  m_locked = false;
}

void ReadLocker::lock() {
  if (!m_locked)
    m_lock.readLock();
  m_locked = true;
}

bool ReadLocker::tryLock() {
  if (!m_locked) {
    m_locked = m_lock.tryReadLock();
    return m_locked;
  }
  return true;
}

WriteLocker::WriteLocker(ReadersWriterMutex& rwlock, bool startLocked) : m_lock(rwlock), m_locked(false) {
  if (startLocked)
    lock();
}

WriteLocker::~WriteLocker() {
  unlock();
}

void WriteLocker::unlock() {
  if (m_locked)
    m_lock.writeUnlock();
  m_locked = false;
}

void WriteLocker::lock() {
  if (!m_locked)
    m_lock.writeLock();
  m_locked = true;
}

bool WriteLocker::tryLock() {
  if (!m_locked) {
    m_locked = m_lock.tryWriteLock();
    return m_locked;
  }
  return true;
}

}
