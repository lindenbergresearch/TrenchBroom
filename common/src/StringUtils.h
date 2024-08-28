#pragma once
#include "string"

namespace TrenchBroom {

/**
 * @brief Replacement function of 'stringf' from v0.6.x
 * @param zcFormat Format string
 * @param ... Formats args
 * @return Rendered string
 *
 * This is used as replacement for the dropped 'stringf'
 * function from string.cpp used in Rack 0.6.x.
 * It just covers the functionality but uses a quite differen
 * code found at:
 *
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
 *
 */
inline std::string stringf(const char *const zcFormat, ...) {
    // initialize use of the variable argument array
    va_list vaArgs;
    va_start(vaArgs, zcFormat);

    // reliably acquire the size
    // from a copy of the variable argument array
    // and a functionally reliable call to mock the formatting
    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int iLen = std::vsnprintf(nullptr, 0, zcFormat, vaArgsCopy);
    va_end(vaArgsCopy);

    // return a formatted string without risking memory mismanagement
    // and without assuming any compiler or platform specific behavior
    std::vector<char> zc(iLen + 1);
    std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
    va_end(vaArgs);

    return std::string(zc.data(), iLen);
}
} // namespace TrenchBroom
