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

void ReadersWriterMutex::lock_shared() {
  std::unique_lock locker(m_mutex);
  if (m_writers || m_writeWaiters) {
    m_readWaiters++;
    while (m_writers || m_writeWaiters)
      m_readCond.wait(locker);
    m_readWaiters--;
  }
  m_readers++;
}

bool ReadersWriterMutex::try_lock_shared() {
  std::lock_guard locker(m_mutex);
  if (m_writers || m_writeWaiters)
    return false;
  m_readers++;
  return true;
}

void ReadersWriterMutex::unlock_shared() {
  std::lock_guard locker(m_mutex);
  m_readers--;
  if (m_writeWaiters)
    m_writeCond.notify_one();
}

void ReadersWriterMutex::lock() {
  std::unique_lock locker(m_mutex);
  if (m_readers || m_writers) {
    m_writeWaiters++;
    while (m_readers || m_writers)
      m_writeCond.wait(locker);
    m_writeWaiters--;
  }
  m_writers = 1;
}

bool ReadersWriterMutex::try_lock() {
  std::lock_guard locker(m_mutex);
  if (m_readers || m_writers)
    return false;
  m_writers = 1;
  return true;
}

void ReadersWriterMutex::unlock() {
  std::lock_guard locker(m_mutex);
  m_writers = 0;
  if (m_writeWaiters)
    m_writeCond.notify_one();
  else if (m_readWaiters)
    m_readCond.notify_all();
}

}
