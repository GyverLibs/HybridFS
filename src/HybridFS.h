#pragma once

#include <Arduino.h>
#include <FS.h>

#if defined(ESP8266)
#include <SDFS.h>

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SDFS) && !defined(NO_GLOBAL_SD)
#include <SD.h>
#define HFS_HAS_ESP8266_SDFS_SIZE64
#endif
#endif

#if !defined(ESP32) && !defined(ESP8266)
#error "HybridFS supports only ESP32 and ESP8266"
#endif

class FSWrapper {
   public:
    // создать пустую обёртку
    FSWrapper() = default;

    // создать обёртку для файловой системы
    template <typename FsT>
    explicit FSWrapper(FsT& filesystem) {
        setFS(filesystem);
    }

    // получить указатель на файловую систему
    fs::FS* fs() {
        return _fs;
    }

    // проверить подключение файловой системы
    bool valid() const {
        return _fs != nullptr;
    }

    // проверить подключение файловой системы
    explicit operator bool() const {
        return valid();
    }

    // подключить файловую систему
    template <typename FsT>
    void setFS(FsT& filesystem) {
        _fs = &filesystem;
        _spaceContext = &filesystem;
        _readSpace = &readSpace<FsT>;
    }

#if defined(ESP32)
    // подключить базовую fs без статистики объёма
    void setFS(fs::FS& filesystem) {
        _fs = &filesystem;
        _spaceContext = nullptr;
        _readSpace = nullptr;
    }
#endif

    // отключить файловую систему
    void reset() {
        _fs = nullptr;
        _spaceContext = nullptr;
        _readSpace = nullptr;
    }

    // удалить файл
    bool remove(const char* path) {
        return _fs && validPath(path) && _fs->remove(path);
    }

    // открыть файл
    fs::File open(const char* path, const char* mode) {
        if (!_fs || !validPath(path) || !mode || !mode[0]) return fs::File();

#if defined(ESP8266)
        return _fs->open(path, mode);
#elif defined(ESP32)
        bool createPath = mode[0] == 'w' || mode[0] == 'a';
        return _fs->open(path, mode, createPath);
#endif
    }

    // открыть файл для чтения
    fs::File openRead(const char* path) {
        return open(path, "r");
    }

    // открыть файл для записи
    fs::File openWrite(const char* path) {
        return open(path, "w");
    }

    // получить список файлов
    String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0) {
        String result;
        listDir(result, path, pathDiv, sizeDiv);
        return result;
    }

    // добавить список файлов в строку
    void listDir(String& result,
                 const char* path = "/",
                 char pathDiv = ';',
                 char sizeDiv = 0,
                 const char* prefix = "") {
        if (!_fs || !validPath(path)) return;

#if defined(ESP8266)
        fs::Dir dir = _fs->openDir(path);
        while (dir.next()) {
            String fullPath = dir.fileName();
            if (!fullPath.length()) continue;

            if (fullPath[0] != '/') {
                String base(path);
                if (!base.endsWith("/")) base += '/';
                fullPath = base + fullPath;
            }

            if (dir.isDirectory()) {
                listDir(result, fullPath.c_str(), pathDiv, sizeDiv, prefix);
            } else if (dir.isFile()) {
                append(result, prefix, fullPath.c_str(), dir.fileSize(), pathDiv, sizeDiv);
            }
        }
#elif defined(ESP32)
        fs::File root = _fs->open(path, "r");
        if (!root || !root.isDirectory()) return;

        fs::File file;
        while ((file = root.openNextFile())) {
            if (file.isDirectory()) {
                listDir(result, file.path(), pathDiv, sizeDiv, prefix);
            } else {
                append(result, prefix, file.path(), file.size(), pathDiv, sizeDiv);
            }
        }
#endif
    }

    // создать каталог
    bool mkdir(const char* path) {
        return _fs && validPath(path) && strcmp(path, "/") && _fs->mkdir(path);
    }

    // удалить пустой каталог
    bool rmdir(const char* path) {
        return _fs && validPath(path) && strcmp(path, "/") && _fs->rmdir(path);
    }

    // получить общий объём в байтах
    uint64_t totalSpace() const {
        return space().total;
    }

    // получить занятый объём в байтах
    uint64_t usedSpace() const {
        return space().used;
    }

    // проверить доступность точной статистики объёма
    bool spaceValid() const {
        return space().valid;
    }

    // получить свободный объём в байтах
    uint64_t freeSpace() const {
        SpaceInfo info = space();
        return info.valid ? info.total - info.used : 0;
    }

   private:
    struct SpaceInfo {
        uint64_t total;
        uint64_t used;
        bool valid;

        SpaceInfo(uint64_t total = 0, uint64_t used = 0, bool valid = false)
            : total(total), used(used), valid(valid) {}
    };

    using SpaceReader = SpaceInfo (*)(void*);

    // получить статистику файловой системы
    SpaceInfo space() const {
        return _readSpace ? _readSpace(_spaceContext) : SpaceInfo();
    }

    // получить статистику файловой системы выбранного типа
    template <typename FsT>
    static SpaceInfo readSpace(void* context) {
        FsT* filesystem = static_cast<FsT*>(context);

#if defined(ESP8266)
        fs::FSInfo64 info{};
        if (!filesystem->info64(info)) return SpaceInfo();

#ifdef HFS_HAS_ESP8266_SDFS_SIZE64
        if (static_cast<fs::FS*>(filesystem) == &SDFS) {
            uint64_t total = SD.size64();
            bool valid = total && info.totalBytes == total && info.usedBytes <= total;
            return SpaceInfo(total, valid ? info.usedBytes : 0, valid);
        }
#endif

        bool valid = info.totalBytes && info.usedBytes <= info.totalBytes;
        return SpaceInfo(info.totalBytes, valid ? info.usedBytes : 0, valid);
#elif defined(ESP32)
        uint64_t total = filesystem->totalBytes();
        uint64_t used = filesystem->usedBytes();
        bool valid = total && used <= total;
        return SpaceInfo(total, valid ? used : 0, valid);
#endif
    }

    // проверить корректность пути
    static bool validPath(const char* path) {
        return path && path[0] == '/';
    }

    // добавить файл в результат
    static void append(String& result,
                       const char* prefix,
                       const char* path,
                       uint64_t size,
                       char pathDiv,
                       char sizeDiv) {
        if (!path) return;
        if (prefix && prefix[0]) {
            result += prefix;
            if (prefix[strlen(prefix) - 1] == '/' && path[0] == '/') path++;
        }
        result += path;
        if (sizeDiv) {
            result += sizeDiv;
            result += size;
        }
        if (pathDiv) result += pathDiv;
    }

    fs::FS* _fs = nullptr;
    void* _spaceContext = nullptr;
    SpaceReader _readSpace = nullptr;
};

// префикс виртуального пути sd должен начинаться и заканчиваться символом /
#ifndef HFS_SD_PREFIX
#define HFS_SD_PREFIX "/sd/"
#endif
#define HFS_SD_PREFIX_LEN (sizeof(HFS_SD_PREFIX) - 1)
#define HFS_SD_ROOT_LEN (HFS_SD_PREFIX_LEN - 1)

static_assert(HFS_SD_PREFIX_LEN >= 3, "HFS_SD_PREFIX must contain a name between slashes");
static_assert(HFS_SD_PREFIX[0] == '/', "HFS_SD_PREFIX must start with /");
static_assert(HFS_SD_PREFIX[HFS_SD_PREFIX_LEN - 1] == '/', "HFS_SD_PREFIX must end with /");

class HybridFS {
   public:
    FSWrapper flash;
    FSWrapper sd;

    // создать пустую гибридную файловую систему
    HybridFS() = default;

    // создать гибридную файловую систему без карты памяти
    template <typename FlashT>
    explicit HybridFS(FlashT& flashFs) {
        setFS(flashFs);
    }

    // создать гибридную файловую систему с картой памяти
    template <typename FlashT, typename SdT>
    HybridFS(FlashT& flashFs, SdT& sdFs) {
        setFS(flashFs, sdFs);
    }

    // подключить файловую систему без карты памяти
    template <typename FlashT>
    void setFS(FlashT& flashFs) {
        flash.setFS(flashFs);
        sd.reset();
    }

    // подключить файловую систему с картой памяти
    template <typename FlashT, typename SdT>
    void setFS(FlashT& flashFs, SdT& sdFs) {
        flash.setFS(flashFs);
        sd.setFS(sdFs);
    }

    // получить список файлов
    String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0) {
        String result;
        listDir(result, path, pathDiv, sizeDiv);
        return result;
    }

    // добавить список файлов в строку
    void listDir(String& result, const char* path = "/", char pathDiv = ';', char sizeDiv = 0) {
        if (!validPath(path)) return;

        if (isSD(path)) {
            if (sd) sd.listDir(result, realPath(path), pathDiv, sizeDiv, HFS_SD_PREFIX);
            return;
        }

        flash.listDir(result, path, pathDiv, sizeDiv);
        if (sd && !strcmp(path, "/")) {
            sd.listDir(result, "/", pathDiv, sizeDiv, HFS_SD_PREFIX);
        }
    }

    // удалить файл
    bool remove(const char* path) {
        return validPath(path) && select(path).remove(realPath(path));
    }

    // открыть файл
    fs::File open(const char* path, const char* mode) {
        return validPath(path) ? select(path).open(realPath(path), mode) : fs::File();
    }

    // открыть файл для чтения
    fs::File openRead(const char* path) {
        return open(path, "r");
    }

    // открыть файл для записи
    fs::File openWrite(const char* path) {
        return open(path, "w");
    }

    // создать каталог
    bool mkdir(const char* path) {
        return validPath(path) && select(path).mkdir(realPath(path));
    }

    // удалить пустой каталог
    bool rmdir(const char* path) {
        return validPath(path) && select(path).rmdir(realPath(path));
    }

    // получить указатель на выбранную файловую систему
    fs::FS* fs(const char* path) {
        return validPath(path) ? select(path).fs() : nullptr;
    }

   private:
    // проверить корректность пути
    static bool validPath(const char* path) {
        return path && path[0] == '/';
    }

    // проверить принадлежность пути карте памяти
    static bool isSD(const char* path) {
        return path &&
               !strncmp(path, HFS_SD_PREFIX, HFS_SD_ROOT_LEN) &&
               (path[HFS_SD_ROOT_LEN] == 0 || path[HFS_SD_ROOT_LEN] == '/');
    }

    // получить путь внутри выбранной файловой системы
    static const char* realPath(const char* path) {
        if (!isSD(path)) return path;
        return path[HFS_SD_ROOT_LEN] ? path + HFS_SD_ROOT_LEN : "/";
    }

    // выбрать файловую систему по пути
    FSWrapper& select(const char* path) {
        return isSD(path) ? sd : flash;
    }
};

#ifdef HFS_HAS_ESP8266_SDFS_SIZE64
#undef HFS_HAS_ESP8266_SDFS_SIZE64
#endif
