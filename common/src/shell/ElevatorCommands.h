#ifndef ELEVATORCOMMANDS_H
#define ELEVATORCOMMANDS_H

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "IPCHelper.h"

struct ShellCmdResult {
  int exitCode{};
  std::string output{};
};

enum class ElevatorCommandType { NONE, RUN_CMD, READ_BYTES, WRITE_BYTES, CREATE_FILE, REMOVE, PROTECT_FILE };

struct ElevatorCommand {
  ElevatorCommandType type{};
  std::vector<std::string> args{};
  std::vector<uint8_t> dataBytes{};

  [[nodiscard]] IPCMessage ToMessage() const {
    nlohmann::json json = {{"type", type}, {"args", args}};
    return {json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), dataBytes};
  }

  [[nodiscard]] std::string ToString() const {
    auto argsStr = nlohmann::json(args).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    if(dataBytes.empty())
      return fmt::format("Type={}, Args={}", GetTypeName(type), argsStr);
    return fmt::format("Type={}, Args={}, DataSize={}", GetTypeName(type), argsStr, dataBytes.size());
  }

  static std::string GetTypeName(ElevatorCommandType type) {
    switch(type) {
      case ElevatorCommandType::NONE:
        return "NONE";
      case ElevatorCommandType::RUN_CMD:
        return "RUN_CMD";
      case ElevatorCommandType::READ_BYTES:
        return "READ_BYTES";
      case ElevatorCommandType::WRITE_BYTES:
        return "WRITE_BYTES";
      case ElevatorCommandType::CREATE_FILE:
        return "CREATE_FILE";
      case ElevatorCommandType::REMOVE:
        return "REMOVE";
      case ElevatorCommandType::PROTECT_FILE:
        return "PROTECT_FILE";
    }
    return fmt::format("UNKNOWN({})", static_cast<int>(type));
  }

  static std::optional<ElevatorCommand> FromMessage(IPCMessage msg) {
    try {
      auto json = nlohmann::json::parse(msg.json);
      auto cmd = ElevatorCommand();
      cmd.type = json["type"];
      cmd.args = json["args"];
      cmd.dataBytes = std::move(msg.blob);
      return cmd;
    } catch(const std::exception &ex) {
      spdlog::error("Failed parsing elevator command. (Exception={}, Str={})", ex.what(), msg.json);
      return {};
    }
  }
};

struct ElevatorCommandResponse {
  bool isError{};
  std::string message{};
  ShellCmdResult cmdResult{};
  std::vector<uint8_t> dataBytes{};

  ElevatorCommandResponse() = default;
  explicit ElevatorCommandResponse(bool isError, const std::string &message) {
    this->isError = isError;
    this->message = message;
  }

  explicit ElevatorCommandResponse(std::vector<uint8_t> dataBytes) {
    this->dataBytes = std::move(dataBytes);
  }

  explicit ElevatorCommandResponse(const ShellCmdResult &cmdResult) {
    this->cmdResult = cmdResult;
  }

  [[nodiscard]] IPCMessage ToMessage() const {
    nlohmann::json json = {
        {"isError", isError},
        {"message", message},
        {"cmdResult", {{"exitCode", cmdResult.exitCode}, {"output", cmdResult.output}}},
    };
    return {json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), dataBytes};
  }

  [[nodiscard]] std::string ToString() const {
    return fmt::format("IsError={}, Message='{}', ExitCode={}, DataSize={}", isError, message, cmdResult.exitCode, dataBytes.size());
  }

  static std::optional<ElevatorCommandResponse> FromMessage(IPCMessage msg) {
    try {
      auto json = nlohmann::json::parse(msg.json);
      auto resp = ElevatorCommandResponse();
      resp.isError = json["isError"];
      resp.message = json["message"];
      resp.cmdResult.exitCode = json["cmdResult"]["exitCode"];
      resp.cmdResult.output = json["cmdResult"]["output"];
      resp.dataBytes = std::move(msg.blob);
      return resp;
    } catch(const std::exception &ex) {
      spdlog::error("Failed parsing elevator response. (Exception={}, Str={})", ex.what(), msg.json);
      return {};
    }
  }
};

#endif
