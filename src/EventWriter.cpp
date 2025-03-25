#include "EventWriter.h"
#include <iostream>
// #include "epd_driver.h"
#include "Calendar.h"
#include "firasans.h"
#include <cstring>

EventWriter::EventWriter(int32_t x, int32_t y, int32_t x_lim)
{
    // Initialize members or process the parameters as needed
    // Here we assume they are just placeholders since no member variables are defined in the header
    std::cout << "EventWriter created with x: " << x << ", y: " << y << ", X_lim: " << x_lim << std::endl;

    this->x = x;
    this->y = y;
    this->x_lim = x_lim;

    // this->framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    // if (!this->framebuffer) {
    //     Serial.println("alloc memory failed !!!");
    //     while (1);
    // }
    // memset(this->framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

}

void EventWriter::writeText(const std::string& text) 
{
    const int maxCharsPerLine = 17;
    std::vector<const char*> chunks = splitText(text, maxCharsPerLine);
    std::cout << "x: " << this->x << " y: "<< this->y << " vektor size: " << chunks.size() <<std::endl;


    for (const char* chunk : chunks) {
        writeln((GFXfont *)&FiraSans, chunk, &this->x, &this->y, NULL); 
        delay(200);       
        this->x = 10;
        this->y += 50;
        delete[] chunk;  // Uvolníme paměť alokovanou pro chunk
    }     

    //  epd_draw_vline(480,10,200,0,this->framebuffer);
    //  epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, this->framebuffer);
    
     delay(200);
}

std::vector<const char*> EventWriter::splitText(const std::string& text, int chunkSize) const {
    std::vector<const char*> chunks;
    int textLength = text.length();
    
    for (int i = 0; i < textLength; i += chunkSize) {
        // Určete délku podřetězce, aby se nepřesáhl konec textu
        int currentChunkSize = std::min(chunkSize, textLength - i);
        
        // Vytvoříme dočasný buffer pro uchování podřetězce
        char* chunk = new char[currentChunkSize + 1];  // +1 pro ukončovací znak '\0'
        strncpy(chunk, text.c_str() + i, currentChunkSize);
        chunk[currentChunkSize] = '\0';  // Přidání ukončovacího znaku
        std::cout << "chunk "<< strlen(chunk) << ":" << chunk << std::endl;
        chunks.push_back(chunk);
    }

    return chunks;
}