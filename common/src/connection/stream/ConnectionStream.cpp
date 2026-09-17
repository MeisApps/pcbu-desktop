#include "ConnectionStream.h"

#include <chrono>
#include <thread>

void ConnectionStream::Write(const uint8_t *buffer, size_t length) {
  m_WriteBuffer.insert(m_WriteBuffer.end(), buffer, buffer + length);
}

PacketError ConnectionStream::Flush() {
  size_t bytesWritten = 0;
  while(bytesWritten < m_WriteBuffer.size()) {
    auto result = WriteRaw(m_WriteBuffer.data() + bytesWritten, m_WriteBuffer.size() - bytesWritten);
    if(result.bytes > 0) {
      bytesWritten += result.bytes;
      continue;
    }
    if(result.error != PacketError::NONE) {
      m_WriteBuffer.clear();
      return result.error;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  m_WriteBuffer.clear();
  return PacketError::NONE;
}
