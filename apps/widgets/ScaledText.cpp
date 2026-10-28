#include "ScaledText.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <string>

#include <epd_driver.h>

#include "zlib/zlib.h"

namespace apps {
namespace widgets {

namespace {

uint32_t nextCodepoint(const char*& cursor) {
    const auto first = static_cast<uint8_t>(*cursor);
    if (first == 0) {
        return 0;
    }

    if (first < 0x80) {
        cursor += 1;
        return first;
    }

    auto getByte = [&](int offset) -> uint8_t {
        return static_cast<uint8_t>(cursor[offset]);
    };

    if ((first & 0xE0) == 0xC0) {
        uint8_t b1 = getByte(1);
        if ((b1 & 0xC0) != 0x80) {
            cursor += 1;
            return 0xFFFD;
        }
        uint32_t cp = ((first & 0x1F) << 6) | (b1 & 0x3F);
        cursor += 2;
        return cp;
    }

    if ((first & 0xF0) == 0xE0) {
        uint8_t b1 = getByte(1);
        uint8_t b2 = getByte(2);
        if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80)) {
            cursor += 1;
            return 0xFFFD;
        }
        uint32_t cp = ((first & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        cursor += 3;
        return cp;
    }

    if ((first & 0xF8) == 0xF0) {
        uint8_t b1 = getByte(1);
        uint8_t b2 = getByte(2);
        uint8_t b3 = getByte(3);
        if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)) {
            cursor += 1;
            return 0xFFFD;
        }
        uint32_t cp = ((first & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        cursor += 4;
        return cp;
    }

    cursor += 1;
    return 0xFFFD;
}

int32_t floorDiv2(int32_t value) {
    if (value >= 0) {
        return value / 2;
    }
    return -(((-value) + 1) / 2);
}

uint8_t readPixelNibble(const uint8_t* bitmap, int32_t byteWidth, int32_t x, int32_t y) {
    const uint8_t packed = bitmap[y * byteWidth + x / 2];
    if ((x & 1) == 0) {
        return packed >> 4;
    }
    return packed & 0x0F;
}

bool getGlyph(const GFXfont* font, uint32_t codepoint, GFXglyph*& glyphOut) {
    if (!font) {
        glyphOut = nullptr;
        return false;
    }

    GFXglyph* glyph = nullptr;
    get_glyph(font, codepoint, &glyph);
    if (!glyph) {
        get_glyph(font, static_cast<uint32_t>('?'), &glyph);
    }
    glyphOut = glyph;
    return glyphOut != nullptr;
}

}  // namespace

int32_t measureTextWidth(const GFXfont* font, const std::string& text) {
    if (!font || text.empty()) {
        return 0;
    }

    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t w = 0;
    int32_t h = 0;
    get_text_bounds(font, text.c_str(), &x, &y, &x1, &y1, &w, &h, nullptr);
    return w;
}

int32_t measureTextWidthHalf(const GFXfont* font, const std::string& text) {
    const int32_t base = measureTextWidth(font, text);
    return std::max<int32_t>(0, base / 2);
}

void drawText2x(const GFXfont* font, const std::string& text, int32_t baselineX, int32_t baselineY, uint8_t* framebuffer) {
    if (!font || !framebuffer || text.empty()) {
        return;
    }

    int32_t cursorX = 0;
    const char* cursor = text.c_str();
    while (true) {
        uint32_t cp = nextCodepoint(cursor);
        if (cp == 0) {
            break;
        }

        GFXglyph* glyph = nullptr;
        if (!getGlyph(font, cp, glyph)) {
            continue;
        }

        const int32_t byteWidth = (glyph->width / 2 + glyph->width % 2);
        const unsigned long bitmapSize = static_cast<unsigned long>(byteWidth) * glyph->height;
        if (bitmapSize == 0) {
            cursorX += glyph->advance_x;
            continue;
        }

        uint8_t* bitmap = nullptr;
        if (font->compressed) {
            bitmap = static_cast<uint8_t*>(malloc(bitmapSize));
            if (!bitmap) {
                cursorX += glyph->advance_x;
                continue;
            }
            unsigned long destLen = bitmapSize;
            if (uncompress(bitmap, &destLen, &font->bitmap[glyph->data_offset], glyph->compressed_size) != Z_OK) {
                free(bitmap);
                cursorX += glyph->advance_x;
                continue;
            }
        } else {
            bitmap = &font->bitmap[glyph->data_offset];
        }

        const int32_t glyphOriginX = baselineX + (cursorX + glyph->left) * 2;
        const int32_t glyphOriginY = baselineY - glyph->top * 2;

        for (int32_t sy = 0; sy < glyph->height; ++sy) {
            for (int32_t sx = 0; sx < glyph->width; ++sx) {
                const uint8_t alpha = readPixelNibble(bitmap, byteWidth, sx, sy);
                if (alpha == 0) {
                    continue;
                }
                const uint8_t value = static_cast<uint8_t>(15 - alpha);
                const uint8_t color = static_cast<uint8_t>(value << 4);
                const int32_t dx = glyphOriginX + sx * 2;
                const int32_t dy = glyphOriginY + sy * 2;
                epd_draw_pixel(dx, dy, color, framebuffer);
                epd_draw_pixel(dx + 1, dy, color, framebuffer);
                epd_draw_pixel(dx, dy + 1, color, framebuffer);
                epd_draw_pixel(dx + 1, dy + 1, color, framebuffer);
            }
        }

        if (font->compressed) {
            free(bitmap);
        }

        cursorX += glyph->advance_x;
    }
}

void drawTextHalf(const GFXfont* font, const std::string& text, int32_t baselineX, int32_t baselineY, uint8_t* framebuffer) {
    if (!font || !framebuffer || text.empty()) {
        return;
    }

    int32_t cursorX = 0;
    const char* cursor = text.c_str();
    while (true) {
        uint32_t cp = nextCodepoint(cursor);
        if (cp == 0) {
            break;
        }

        GFXglyph* glyph = nullptr;
        if (!getGlyph(font, cp, glyph)) {
            continue;
        }

        const int32_t byteWidth = (glyph->width / 2 + glyph->width % 2);
        const unsigned long bitmapSize = static_cast<unsigned long>(byteWidth) * glyph->height;
        if (bitmapSize == 0) {
            cursorX += glyph->advance_x;
            continue;
        }

        uint8_t* bitmap = nullptr;
        if (font->compressed) {
            bitmap = static_cast<uint8_t*>(malloc(bitmapSize));
            if (!bitmap) {
                cursorX += glyph->advance_x;
                continue;
            }
            unsigned long destLen = bitmapSize;
            if (uncompress(bitmap, &destLen, &font->bitmap[glyph->data_offset], glyph->compressed_size) != Z_OK) {
                free(bitmap);
                cursorX += glyph->advance_x;
                continue;
            }
        } else {
            bitmap = &font->bitmap[glyph->data_offset];
        }

        const int32_t destOriginX = baselineX + floorDiv2(cursorX + glyph->left);
        const int32_t destOriginY = baselineY + floorDiv2(-glyph->top);

        const int32_t blocksX = (glyph->width + 1) / 2;
        const int32_t blocksY = (glyph->height + 1) / 2;

        for (int32_t by = 0; by < blocksY; ++by) {
            for (int32_t bx = 0; bx < blocksX; ++bx) {
                uint8_t maxAlpha = 0;
                for (int32_t oy = 0; oy < 2; ++oy) {
                    int32_t sy = by * 2 + oy;
                    if (sy >= glyph->height) {
                        continue;
                    }
                    for (int32_t ox = 0; ox < 2; ++ox) {
                        int32_t sx = bx * 2 + ox;
                        if (sx >= glyph->width) {
                            continue;
                        }
                        maxAlpha = std::max<uint8_t>(maxAlpha, readPixelNibble(bitmap, byteWidth, sx, sy));
                    }
                }

                if (maxAlpha == 0) {
                    continue;
                }

                const uint8_t value = static_cast<uint8_t>(15 - maxAlpha);
                const uint8_t color = static_cast<uint8_t>(value << 4);
                const int32_t dx = destOriginX + bx;
                const int32_t dy = destOriginY + by;
                epd_draw_pixel(dx, dy, color, framebuffer);
            }
        }

        if (font->compressed) {
            free(bitmap);
        }

        cursorX += glyph->advance_x;
    }
}

}  // namespace widgets
}  // namespace apps
