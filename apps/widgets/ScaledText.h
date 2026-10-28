#ifndef APPS_WIDGETS_SCALED_TEXT_H
#define APPS_WIDGETS_SCALED_TEXT_H

#include <cstdint>
#include <string>

#include <Arduino.h>
#include <epd_driver.h>

namespace apps {
namespace widgets {

void drawText2x(const GFXfont* font, const std::string& text, int32_t baselineX, int32_t baselineY, uint8_t* framebuffer);
void drawTextHalf(const GFXfont* font, const std::string& text, int32_t baselineX, int32_t baselineY, uint8_t* framebuffer);
int32_t measureTextWidth(const GFXfont* font, const std::string& text);
int32_t measureTextWidthHalf(const GFXfont* font, const std::string& text);

}  // namespace widgets
}  // namespace apps

#endif  // APPS_WIDGETS_SCALED_TEXT_H
