#ifndef EVENTWRITER_H
#define EVENTWRITER_H

#include <iostream>
#include <string>
#include <vector>

class EventWriter
{
    public:
    //position of cursor
    int32_t x, y, x_lim;

    EventWriter(int32_t x,int32_t y,int32_t x_lim);
    void writeText(const std::string& text); 

    private:

        static int counter;
        uint8_t *framebuffer = NULL;
        std::vector<const char*> splitText(const std::string& text, int chunkSize) const;
};

#endif
