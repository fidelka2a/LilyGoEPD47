#include "Utf8.h"

#include <array>
#include <string>

namespace apps {

namespace {

constexpr std::array<uint16_t, 128> kCp1250ToUnicode = {
    0x20AC, 0xFFFD, 0x201A, 0xFFFD, 0x201E, 0x2026, 0x2020, 0x2021,
    0xFFFD, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool decodeNextUtf8(const std::string& input, std::size_t& index, uint32_t& cp) {
    if (index >= input.size()) {
        return false;
    }

    const uint8_t c0 = static_cast<uint8_t>(input[index]);
    if (c0 < 0x80) {
        cp = c0;
        index += 1;
        return true;
    }

    if ((c0 & 0xE0) == 0xC0) {
        if (index + 1 >= input.size()) {
            return false;
        }
        const uint8_t c1 = static_cast<uint8_t>(input[index + 1]);
        if ((c1 & 0xC0) != 0x80) {
            return false;
        }
        cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
        if (cp < 0x80) {
            return false;
        }
        index += 2;
        return true;
    }

    if ((c0 & 0xF0) == 0xE0) {
        if (index + 2 >= input.size()) {
            return false;
        }
        const uint8_t c1 = static_cast<uint8_t>(input[index + 1]);
        const uint8_t c2 = static_cast<uint8_t>(input[index + 2]);
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) {
            return false;
        }
        cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return false;
        }
        index += 3;
        return true;
    }

    if ((c0 & 0xF8) == 0xF0) {
        if (index + 3 >= input.size()) {
            return false;
        }
        const uint8_t c1 = static_cast<uint8_t>(input[index + 1]);
        const uint8_t c2 = static_cast<uint8_t>(input[index + 2]);
        const uint8_t c3 = static_cast<uint8_t>(input[index + 3]);
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
            return false;
        }
        cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) {
            return false;
        }
        index += 4;
        return true;
    }

    return false;
}

bool isValidUtf8(const std::string& input) {
    std::size_t index = 0;
    uint32_t cp = 0;
    while (index < input.size()) {
        if (!decodeNextUtf8(input, index, cp)) {
            return false;
        }
    }
    return true;
}

bool appendCzechAsciiFallback(std::string& out, uint32_t cp) {
    switch (cp) {
        case 0x010C:  // Č
        case 0x010D:  // č
            out.push_back('c');
            return true;
        case 0x010E:  // Ď
        case 0x010F:  // ď
            out.push_back('d');
            return true;
        case 0x011A:  // Ě
        case 0x011B:  // ě
            out.push_back('e');
            return true;
        case 0x0147:  // Ň
        case 0x0148:  // ň
            out.push_back('n');
            return true;
        case 0x0158:  // Ř
        case 0x0159:  // ř
            out.push_back('r');
            return true;
        case 0x0160:  // Š
        case 0x0161:  // š
            out.push_back('s');
            return true;
        case 0x0164:  // Ť
        case 0x0165:  // ť
            out.push_back('t');
            return true;
        case 0x016E:  // Ů
        case 0x016F:  // ů
            out.push_back('u');
            return true;
        case 0x017D:  // Ž
        case 0x017E:  // ž
            out.push_back('z');
            return true;
        default:
            return false;
    }
}

std::string normalizeUtf8ForCurrentFont(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    std::size_t index = 0;
    uint32_t cp = 0;
    while (index < input.size() && decodeNextUtf8(input, index, cp)) {
        if (cp <= 0x7F || (cp >= 0x00A0 && cp <= 0x00FF)) {
            appendUtf8(out, cp);
            continue;
        }
        if (appendCzechAsciiFallback(out, cp)) {
            continue;
        }
        out.push_back('?');
    }

    return out;
}

std::string cp1250ToUtf8(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);

    for (unsigned char ch : input) {
        if (ch < 0x80) {
            out.push_back(static_cast<char>(ch));
            continue;
        }
        uint32_t cp = kCp1250ToUnicode[ch - 0x80];
        if (cp == 0xFFFD) {
            out.push_back('?');
            continue;
        }
        if (appendCzechAsciiFallback(out, cp)) {
            continue;
        }
        appendUtf8(out, cp);
    }

    return out;
}

}  // namespace

std::string ensureSafeUtf8ForFont(const std::string& input) {
    if (input.empty()) {
        return input;
    }

    if (isValidUtf8(input)) {
        return normalizeUtf8ForCurrentFont(input);
    }

    return cp1250ToUtf8(input);
}

}  // namespace apps

