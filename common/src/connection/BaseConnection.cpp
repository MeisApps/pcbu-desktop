#include "BaseConnection.h"

#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

#include "Packets.h"
#include "SocketDefs.h"

#ifdef LINUX
#define htonll(x) ((1 == htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define ntohll(x) ((1 == ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
#endif

bool BaseConnection::SetSocketBlocking(SOCKET socket, bool isBlocking) {
#ifdef WINDOWS
  u_long mode = isBlocking ? 0 : 1;
  return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
  int oldFlags = fcntl(socket, F_GETFL, 0);
  if(oldFlags == -1)
    return false;
  auto flags = isBlocking ? (oldFlags & ~O_NONBLOCK) : (oldFlags | O_NONBLOCK);
  return fcntl(socket, F_SETFL, flags) != -1;
#endif
}

bool BaseConnection::SetSocketRWTimeout(SOCKET socket, uint32_t secs) {
#ifdef WINDOWS
  auto timeoutVal = (DWORD)secs * 1000;
  if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutVal), sizeof(timeoutVal)) < 0 ||
     setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeoutVal), sizeof(timeoutVal)) < 0) {
#else
  struct timeval timeoutVal{};
  timeoutVal.tv_sec = secs;
  if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal)) < 0 ||
     setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeoutVal, sizeof(timeoutVal)) < 0) {
#endif
    return false;
  }
  return true;
}

Packet BaseConnection::ReadPacket(ConnectionStream &stream) {
  uint8_t correctHeaderBytes{};
  auto headerBE = htonll(PACKET_HEADER);
  spdlog::debug("Reading packet header...");
  while(correctHeaderBytes < sizeof(PACKET_HEADER)) {
    uint8_t headerByte{};
    auto readRes = stream.Read(&headerByte, 1);
    if(readRes.bytes <= 0) {
      if(readRes.error != PacketError::NONE) {
        spdlog::error("Reading packet header failed.");
        return {readRes.error};
      }
    } else {
      if(headerByte == reinterpret_cast<uint8_t *>(&headerBE)[correctHeaderBytes])
        correctHeaderBytes++;
      else
        correctHeaderBytes = 0;
    }
  }

  spdlog::debug("Reading packet ID...");
  uint16_t packetId{};
  auto result = ReadData(stream, sizeof(packetId));
  if(result.first != PacketError::NONE || result.second.size() != sizeof(packetId)) {
    spdlog::error("Reading packet ID failed.");
    return {result.first};
  }
  std::memcpy(&packetId, result.second.data(), sizeof(packetId));
  packetId = ntohs(packetId);

  spdlog::debug("Reading packet length... (ID={0:X})", packetId);
  uint16_t packetLength{};
  result = ReadData(stream, sizeof(packetLength));
  if(result.first != PacketError::NONE || result.second.size() != sizeof(packetLength)) {
    spdlog::error("Reading packet length failed.");
    return {result.first};
  }
  std::memcpy(&packetLength, result.second.data(), sizeof(packetLength));
  packetLength = ntohs(packetLength);

  if(packetLength == 0) {
    spdlog::error("Empty packet received.");
    return {PacketError::UNKNOWN};
  }

  spdlog::debug("Reading packet data... (Len={})", packetLength);
  result = ReadData(stream, packetLength);
  if(result.first != PacketError::NONE || result.second.size() != packetLength) {
    spdlog::error("Reading packet data failed. (Len={})", packetLength);
    return {result.first};
  }
  spdlog::debug("Done reading packet.");
  return {PacketError::NONE, packetId, result.second};
}

PacketError BaseConnection::WritePacket(ConnectionStream &stream, uint16_t packetId, const std::vector<uint8_t> &data) {
  spdlog::debug("Writing packet... (ID={0:X}, Len={1})", packetId, data.size());
  uint64_t packetHeader = htonll(PACKET_HEADER);
  uint16_t packetIdNet = htons(packetId);
  uint16_t packetSize = htons(static_cast<uint16_t>(data.size()));
  stream.Write(reinterpret_cast<const uint8_t *>(&packetHeader), sizeof(packetHeader));
  stream.Write(reinterpret_cast<const uint8_t *>(&packetIdNet), sizeof(packetIdNet));
  stream.Write(reinterpret_cast<const uint8_t *>(&packetSize), sizeof(packetSize));
  stream.Write(data.data(), data.size());

  auto error = stream.Flush();
  if(error != PacketError::NONE) {
    spdlog::error("Writing packet failed. (ID={0:X}, Len={1})", packetId, data.size());
    return error;
  }
  spdlog::debug("Done writing packet.");
  return PacketError::NONE;
}

std::pair<PacketError, std::vector<uint8_t>> BaseConnection::ReadData(ConnectionStream &stream, uint32_t size) {
  uint32_t bytesRead = 0;
  std::vector<uint8_t> buffer{};
  buffer.resize(size);
  while(bytesRead < size) {
    auto result = stream.Read(buffer.data() + bytesRead, size - bytesRead);
    if(result.bytes <= 0) {
      if(result.error != PacketError::NONE)
        return {result.error, {}};
    } else {
      bytesRead += result.bytes;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return {PacketError::NONE, buffer};
}
