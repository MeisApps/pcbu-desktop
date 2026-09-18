#ifndef ELEVATORCOMMANDS_H
#define ELEVATORCOMMANDS_H

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct ShellCmdResult {
  int exitCode{};
  std::string output{};
};

enum class ElevatorCommandType { NONE, RUN_CMD, READ_BYTES, WRITE_BYTES, CREATE_FILE, REMOVE_FILE, PROTECT_FILE, QUIT };
enum class ElevatorCommandResponseType { NONE, MESSAGE, DATA, CMD_RESULT };

struct ElevatorCommand {
  ElevatorCommandType type{};
  std::vector<std::string> args{};
  std::vector<uint8_t> dataBytes{};

  [[nodiscard]] nlohmann::json ToJson() const {
    return {{"type", type}, {"args", args}, {"dataLength", dataBytes.size()}};
  }

  static std::optional<ElevatorCommand> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto cmd = ElevatorCommand();
      cmd.type = json["type"];
      cmd.args = json["args"];
      return cmd;
    } catch(...) {
      return {};
    }
  }
};

struct ElevatorCommandResponse {
  ElevatorCommandResponseType type{};
  bool isError{};
  std::string message{};
  ShellCmdResult cmdResult{};
  std::vector<uint8_t> dataBytes{};

  ElevatorCommandResponse() = default;
  explicit ElevatorCommandResponse(bool isError, const std::string &message) {
    this->type = ElevatorCommandResponseType::MESSAGE;
    this->isError = isError;
    this->message = message;
  }

  explicit ElevatorCommandResponse(std::vector<uint8_t> dataBytes) {
    this->type = ElevatorCommandResponseType::DATA;
    this->dataBytes = std::move(dataBytes);
  }

  explicit ElevatorCommandResponse(const ShellCmdResult &cmdResult) {
    this->type = ElevatorCommandResponseType::CMD_RESULT;
    this->cmdResult = cmdResult;
  }

  [[nodiscard]] nlohmann::json ToJson() const {
    nlohmann::json cmdResultJson = {
        {"exitCode", cmdResult.exitCode},
        {"output", cmdResult.output},
    };
    return {
        {"type", type}, {"isError", isError}, {"message", message}, {"dataLength", dataBytes.size()}, {"cmdResult", cmdResultJson},
    };
  }

  static std::optional<ElevatorCommandResponse> FromJson(const std::string &jsonStr) {
    try {
      auto json = nlohmann::json::parse(jsonStr);
      auto resp = ElevatorCommandResponse();
      resp.type = json["type"];
      resp.isError = json["isError"];
      resp.message = json["message"];
      resp.cmdResult.exitCode = json["cmdResult"]["exitCode"];
      resp.cmdResult.output = json["cmdResult"]["output"];
      return resp;
    } catch(...) {
      return {};
    }
  }
};

#endif
