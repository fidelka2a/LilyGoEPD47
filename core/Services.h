#ifndef CORE_SERVICES_H
#define CORE_SERVICES_H

namespace core {

struct ClockService {
    virtual ~ClockService() = default;
    virtual unsigned long nowMs() const = 0;
};

struct FontService {
    virtual ~FontService() = default;
    virtual const void* getDefaultFont() const = 0;
};

struct StorageService {
    virtual ~StorageService() = default;
    virtual bool readConfig(const char* key, char* buffer, unsigned int size) = 0;
};

struct Services {
    ClockService* clock = nullptr;
    FontService* fonts = nullptr;
    StorageService* storage = nullptr;

    constexpr Services() = default;
    constexpr Services(ClockService* clockSvc, FontService* fontSvc, StorageService* storageSvc)
        : clock(clockSvc), fonts(fontSvc), storage(storageSvc) {}
};

}  // namespace core

#endif  // CORE_SERVICES_H
