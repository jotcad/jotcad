#pragma once

#include <stdexcept>
#include <string>

namespace fs {

/**
 * VFSException: Signals resolution or validation failures within the mesh.
 */
class VFSException : public std::runtime_error {
public:
    int code;
    VFSException(const std::string& msg, int code = 500) : std::runtime_error(msg), code(code) {}
};

} // namespace fs
