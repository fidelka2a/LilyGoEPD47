#ifndef APPS_UTF8_H
#define APPS_UTF8_H

#include <cstdint>
#include <string>

namespace apps {

// Ensures a string is valid UTF-8 to prevent crashes inside font rendering.
// If input is not valid UTF-8, it is treated as CP1250 and converted to UTF-8.
// Characters not supported by the current font (mostly Czech letters > U+00FF)
// are transliterated to ASCII equivalents where possible.
std::string ensureSafeUtf8ForFont(const std::string& input);

}  // namespace apps

#endif  // APPS_UTF8_H

