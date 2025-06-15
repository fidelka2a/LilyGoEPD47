#ifndef DRAWABLE_H
#define DRAWABLE_H

#include <stdint.h>

class Drawable {
public:
    virtual ~Drawable() = default;

    // Čistě virtuální metoda pro vykreslení na framebuffer
    virtual void draw(uint8_t* framebuffer) = 0;
};

#endif // DRAWABLE_H