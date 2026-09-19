#pragma once
#include <string>
#include <variant>
namespace rk {
enum class ErrorCode { InvalidInput, Unsupported, Unavailable, Conflict, Io, DeviceRemoved, FatalRollback };
struct Error { ErrorCode code; std::string message; };
template<class T> using Result = std::variant<T, Error>;
}
