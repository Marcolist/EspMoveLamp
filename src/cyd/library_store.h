#pragma once
#include <Preferences.h>
#include "library.h"

class LibraryStore {
    struct Record {
        uint32_t magic;
        uint32_t checksum;
        Lamp::SceneLibrary library;
    };
    static uint32_t checksum(const Lamp::SceneLibrary& library) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&library);
        uint32_t hash = 2166136261UL;
        for (size_t i = 0; i < sizeof(library); ++i) hash = (hash ^ bytes[i]) * 16777619UL;
        return hash;
    }
public:
    static bool save(const Lamp::SceneLibrary& library) {
        if (!Lamp::validLibrary(library)) return false;
        Record record{};
        record.magic = 0x4d4c0001;
        record.library = library;
        record.checksum = checksum(record.library);
        Preferences prefs;
        if (!prefs.begin("move-scenes", false)) return false;
        // NVS commits this single blob atomically; never erase the old value first.
        bool saved = prefs.putBytes("library", &record, sizeof(record)) == sizeof(record);
        prefs.end();
        return saved;
    }
    static bool load(Lamp::SceneLibrary& library) {
        Preferences prefs;
        if (!prefs.begin("move-scenes", true)) return false;
        Record record{};
        bool loaded = prefs.getBytesLength("library") == sizeof(record) &&
                      prefs.getBytes("library", &record, sizeof(record)) == sizeof(record);
        prefs.end();
        if (!loaded || record.magic != 0x4d4c0001 || record.checksum != checksum(record.library) ||
            !Lamp::validLibrary(record.library)) return false;
        library = record.library;
        return true;
    }
};
