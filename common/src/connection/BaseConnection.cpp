#include "BaseConnection.h"

#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

#include "Packets.h"
#include "SocketDefs.h"

#ifndef WINDOWS
#define htonll(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define ntohll(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
#endif

bool BaseConnection::IsServer() {
  return false;
}

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

Packet BaseConnection::ReadPacket(SOCKET socket) {
  uint8_t correctHeaderBytes{};
  auto headerBE = htonll(PACKET_HEADER);
  spdlog::debug("Reading packet header...");
  while(correctHeaderBytes < sizeof(PACKET_HEADER)) {
    uint8_t headerByte{};
    int result = (int)read(socket, &headerByte, 1);
    if(result <= 0) {
      auto error = GetPacketError(result, SOCKET_LAST_ERROR);
      if(error != PacketError::NONE) {
        spdlog::error("Reading packet header failed.");
        return {error};
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
  auto result = ReadData(socket, sizeof(packetId));
  if(result.first != PacketError::NONE || result.second.size() != sizeof(packetId)) {
    spdlog::error("Reading packet ID failed.");
    return {result.first};
  }
  std::memcpy(&packetId, result.second.data(), sizeof(packetId));
  packetId = ntohs(packetId);

  spdlog::debug("Reading packet length... (ID={0:X})", packetId);
  uint16_t packetLength{};
  result = ReadData(socket, sizeof(packetLength));
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
  result = ReadData(socket, packetLength);
  if(result.first != PacketError::NONE || result.second.size() != packetLength) {
    spdlog::error("Reading packet data failed. (Len={})", packetLength);
    return {result.first};
  }
  spdlog::debug("Done reading packet.");
  return {PacketError::NONE, packetId, result.second};
}

PacketError BaseConnection::WritePacket(SOCKET socket, uint16_t packetId, const std::vector<uint8_t> &data) {
  PacketError error{};
  spdlog::debug("Writing packet header...");
  uint64_t packetHeader = htonll(PACKET_HEADER);
  error = WriteData(socket, reinterpret_cast<const char *>(&packetHeader), sizeof(packetHeader));
  if(error != PacketError::NONE) {
    spdlog::error("Writing packet header failed.");
    return error;
  }

  spdlog::debug("Writing packet ID... (ID={0:X})", packetId);
  uint16_t packetIdNet = htons(packetId);
  error = WriteData(socket, reinterpret_cast<const char *>(&packetIdNet), sizeof(packetIdNet));
  if(error != PacketError::NONE) {
    spdlog::error("Writing packet ID failed.");
    return error;
  }

  spdlog::debug("Writing packet length... (Len={})", data.size());
  uint16_t packetSize = htons(static_cast<uint16_t>(data.size()));
  error = WriteData(socket, reinterpret_cast<const char *>(&packetSize), sizeof(packetSize));
  if(error != PacketError::NONE) {
    spdlog::error("Writing packet length failed.");
    return error;
  }

  spdlog::debug("Writing packet data...");
  error = WriteData(socket, reinterpret_cast<const char *>(data.data()), data.size());
  if(error != PacketError::NONE) {
    spdlog::error("Writing packet data failed. (Len={})", packetSize);
    return error;
  }
  spdlog::debug("Done writing packet.");
  return PacketError::NONE;
}

std::pair<PacketError, std::vector<uint8_t>> BaseConnection::ReadData(SOCKET socket, uint32_t size) {
  uint32_t bytesRead = 0;
  std::vector<uint8_t> buffer{};
  buffer.resize(size);
  while(bytesRead < size) {
    int result = (int)read(socket, buffer.data() + bytesRead, size - bytesRead);
    if(result <= 0) {
      auto error = GetPacketError(result, SOCKET_LAST_ERROR);
      if(error != PacketError::NONE)
        return {error, {}};
    } else {
      bytesRead += result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return {PacketError::NONE, buffer};
}

PacketError BaseConnection::WriteData(SOCKET socket, const char *data, uint32_t size) {
  uint32_t bytesWritten = 0;
  while(bytesWritten < size) {
    int result = (int)write(socket, data + bytesWritten, size - bytesWritten);
    if(result <= 0) {
      auto error = GetPacketError(result, SOCKET_LAST_ERROR);
      if(error != PacketError::NONE)
        return error;
    } else {
      bytesWritten += result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return PacketError::NONE;
}

PacketError BaseConnection::GetPacketError(int result, int error) {
  if(result == 0)
    return PacketError::CLOSED_CONNECTION;
  if(error == SOCKET_ERROR_WOULD_BLOCK || error == SOCKET_ERROR_IN_PROGRESS || error == SOCKET_ERROR_TRY_AGAIN)
    return PacketError::NONE;

  spdlog::error("Socket operation failed. (Code={}, Str={})", error, strerror(error));
  if(error == SOCKET_ERROR_CONNECT_REFUSED || error == SOCKET_ERROR_HOST_UNREACHABLE || error == SOCKET_ERROR_CONNECT_ABORTED ||
    error == SOCKET_ERROR_CONNECT_RESET || error == SOCKET_ERROR_NET_UNREACHABLE)
    return PacketError::CLOSED_CONNECTION;
  if(error == SOCKET_ERROR_TIMEOUT)
    return PacketError::TIMEOUT;
#ifdef WINDOWS
  if(error == WSAESHUTDOWN || error == WSAENOTSOCK)
    return PacketError::CLOSED_CONNECTION;
#else
  if(error == EPIPE)
    return PacketError::CLOSED_CONNECTION;
#endif
  return PacketError::UNKNOWN;
}
