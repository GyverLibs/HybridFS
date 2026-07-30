#include <Arduino.h>

#if defined(ESP8266)
#include <LittleFS.h>
#include <SD.h>
#include <SDFS.h>
#include <SPI.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include <LittleFS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#elif defined(ESP32)
#include <LittleFS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#else
#error "HybridFS test supports only ESP8266, ESP32 and ESP32-S3"
#endif

#include <HybridFS.h>

HybridFS hfs;
fs::FS* selectedSD = nullptr;

uint16_t passedTests = 0;
uint16_t failedTests = 0;

// вывести результат проверки
void check(bool result, const char* name) {
    Serial.print(result ? "[ok]   " : "[fail] ");
    Serial.println(name);

    if (result) {
        passedTests++;
    } else {
        failedTests++;
    }
}

// подключить littlefs с форматированием при ошибке
bool beginFlash() {
#if defined(ESP8266)
    LittleFS.setConfig(LittleFSConfig(false));
    if (LittleFS.begin()) return true;
    if (!LittleFS.format()) return false;
    LittleFS.setConfig(LittleFSConfig(false));
    return LittleFS.begin();
#elif defined(ESP32)
    return LittleFS.begin(true);
#endif
}

// записать строку в файл
bool writeText(const char* path, const char* text, const char* mode = "w") {
    fs::File file = hfs.open(path, mode);
    if (!file) return false;

    size_t length = strlen(text);
    bool result = file.write(reinterpret_cast<const uint8_t*>(text), length) == length;
    file.close();
    return result;
}

// прочитать строку из файла
String readText(const char* path) {
    fs::File file = hfs.openRead(path);
    if (!file) return String();

    String result;
    while (file.available()) {
        int value = file.read();
        if (value >= 0) result += static_cast<char>(value);
    }

    file.close();
    return result;
}

// проверить существование файла
bool fileExists(const char* path) {
    fs::File file = hfs.openRead(path);
    bool result = static_cast<bool>(file);
    if (file) file.close();
    return result;
}

// удалить тестовые файлы и каталоги
void cleanupTestFiles() {
    hfs.remove("/hfs_test/sub/nested.txt");
    hfs.remove("/hfs_test/file.txt");
    hfs.rmdir("/hfs_test/sub");
    hfs.rmdir("/hfs_test");
    hfs.rmdir("/hfs_empty");

    hfs.remove("/sd/hfs_test/sub/nested.txt");
    hfs.remove("/sd/hfs_test/file.txt");
    hfs.rmdir("/sd/hfs_test/sub");
    hfs.rmdir("/sd/hfs_test");
    hfs.rmdir("/sd/hfs_empty");
}

// вывести объём файловой системы
void printSpace(const char* name, const FSWrapper& storage) {
    char buffer[128];
    snprintf(buffer,
             sizeof(buffer),
             "%s total=%llu used=%llu free=%llu exact=%s",
             name,
             static_cast<unsigned long long>(storage.totalSpace()),
             static_cast<unsigned long long>(storage.usedSpace()),
             static_cast<unsigned long long>(storage.freeSpace()),
             storage.spaceValid() ? "yes" : "no");
    Serial.println(buffer);
}

// проверить выбор файловой системы
void testRouting() {
    Serial.println("\nмаршрутизация");

    check(hfs.fs("/file.txt") == static_cast<fs::FS*>(&LittleFS),
          "обычный путь выбирает littlefs");
    check(hfs.fs("/dir/file.txt") == static_cast<fs::FS*>(&LittleFS),
          "вложенный путь выбирает littlefs");
    check(hfs.fs("/sd") == selectedSD,
          "путь /sd выбирает sd");
    check(hfs.fs("/sd/file.txt") == selectedSD,
          "путь /sd/... выбирает sd");
    check(hfs.fs("/sdcard/file.txt") == static_cast<fs::FS*>(&LittleFS),
          "путь /sdcard не выбирает sd");
    check(hfs.fs("/sdx/file.txt") == static_cast<fs::FS*>(&LittleFS),
          "путь /sdx не выбирает sd");
    check(hfs.fs("file.txt") == nullptr,
          "относительный путь не выбирает файловую систему");
    check(hfs.fs(nullptr) == nullptr,
          "нулевой путь не выбирает файловую систему");
}

// проверить некорректные пути
void testInvalidPaths() {
    Serial.println("\nнекорректные пути");

    fs::File relativeFile = hfs.openRead("relative.txt");
    check(!relativeFile, "открытие относительного пути отклоняется");
    check(!hfs.remove("relative.txt"), "удаление относительного пути отклоняется");
    check(!hfs.mkdir("relative"), "создание относительного каталога отклоняется");
    check(!hfs.rmdir("relative"), "удаление относительного каталога отклоняется");
    check(!hfs.open(nullptr, "r"), "нулевой путь отклоняется");
    check(!hfs.open("/file.txt", nullptr), "нулевой режим отклоняется");
    check(!hfs.open("/file.txt", ""), "пустой режим отклоняется");
    check(!hfs.mkdir("/"), "корневой каталог littlefs не создаётся");
    check(!hfs.rmdir("/"), "корневой каталог littlefs не удаляется");
    check(!hfs.mkdir("/sd"), "виртуальный корень sd не создаётся");
    check(!hfs.rmdir("/sd"), "виртуальный корень sd не удаляется");

    String result = "seed";
    hfs.listDir(result, "relative");
    check(result == "seed", "listDir не меняет результат для относительного пути");
}

// проверить каталоги
void testDirectories() {
    Serial.println("\nкаталоги");

    check(hfs.mkdir("/hfs_test"), "создание каталога littlefs");
    check(hfs.mkdir("/hfs_test/sub"), "создание вложенного каталога littlefs");
    check(hfs.mkdir("/sd/hfs_test"), "создание каталога sd");
    check(hfs.mkdir("/sd/hfs_test/sub"), "создание вложенного каталога sd");

    check(hfs.mkdir("/hfs_empty"), "создание пустого каталога littlefs");
    check(hfs.rmdir("/hfs_empty"), "удаление пустого каталога littlefs");
    check(hfs.mkdir("/sd/hfs_empty"), "создание пустого каталога sd");
    check(hfs.rmdir("/sd/hfs_empty"), "удаление пустого каталога sd");
}

// проверить файлы
void testFiles() {
    Serial.println("\nфайлы");

    check(writeText("/hfs_test/file.txt", "flash"),
          "запись файла littlefs");
    check(writeText("/hfs_test/file.txt", "-append", "a"),
          "добавление в файл littlefs");
    check(readText("/hfs_test/file.txt") == "flash-append",
          "чтение файла littlefs");
    check(writeText("/hfs_test/sub/nested.txt", "flash nested"),
          "запись вложенного файла littlefs");

    check(writeText("/sd/hfs_test/file.txt", "sd"),
          "запись файла sd");
    check(writeText("/sd/hfs_test/file.txt", "-append", "a"),
          "добавление в файл sd");
    check(readText("/sd/hfs_test/file.txt") == "sd-append",
          "чтение файла sd");
    check(writeText("/sd/hfs_test/sub/nested.txt", "sd nested"),
          "запись вложенного файла sd");
}

// проверить список файлов
void testListDir() {
    Serial.println("\nсписок файлов");

    String all = hfs.listDir("/", ';', ':');
    Serial.print("root: ");
    Serial.println(all);

    check(all.indexOf("/hfs_test/file.txt:") >= 0,
          "корневой список содержит файл littlefs");
    check(all.indexOf("/hfs_test/sub/nested.txt:") >= 0,
          "корневой список содержит вложенный файл littlefs");
    check(all.indexOf("/sd/hfs_test/file.txt:") >= 0,
          "корневой список содержит файл sd");
    check(all.indexOf("/sd/hfs_test/sub/nested.txt:") >= 0,
          "корневой список содержит вложенный файл sd");
    check(all.indexOf("/sd//") < 0,
          "корневой список не содержит двойной разделитель sd");

    String flashList = hfs.listDir("/hfs_test");
    check(flashList.indexOf("/hfs_test/file.txt") >= 0,
          "список littlefs содержит обычный файл");
    check(flashList.indexOf("/hfs_test/sub/nested.txt") >= 0,
          "список littlefs содержит вложенный файл");
    check(flashList.indexOf("/sd/") < 0,
          "список littlefs не содержит sd");

    String sdList = hfs.listDir("/sd/hfs_test");
    check(sdList.indexOf("/sd/hfs_test/file.txt") >= 0,
          "список sd содержит обычный файл");
    check(sdList.indexOf("/sd/hfs_test/sub/nested.txt") >= 0,
          "список sd содержит вложенный файл");
    check(sdList.indexOf("/sd//") < 0,
          "список sd не содержит двойной разделитель");
}

// проверить объём файловых систем
void testSpace() {
    Serial.println("\nобъём");

    printSpace("littlefs", hfs.flash);
    printSpace("sd", hfs.sd);

    uint64_t flashTotal = hfs.flash.totalSpace();
    uint64_t flashUsed = hfs.flash.usedSpace();
    uint64_t sdTotal = hfs.sd.totalSpace();
    uint64_t sdUsed = hfs.sd.usedSpace();

    check(flashTotal > 0, "littlefs сообщает общий объём");
    check(hfs.flash.spaceValid(), "статистика littlefs доступна");
    check(flashUsed <= flashTotal, "занятый объём littlefs корректен");
    check(hfs.flash.freeSpace() == flashTotal - flashUsed,
          "свободный объём littlefs корректен");

#if defined(ESP8266)
    fs::FSInfo64 nativeInfo{};
    bool nativeInfoOk = SDFS.info64(nativeInfo);
    uint64_t nativeTotal = SD.size64();

    char buffer[192];
    snprintf(buffer,
             sizeof(buffer),
             "sdfs info64=%s total=%llu used=%llu block=%u calculated=%llu",
             nativeInfoOk ? "true" : "false",
             static_cast<unsigned long long>(nativeInfo.totalBytes),
             static_cast<unsigned long long>(nativeInfo.usedBytes),
             static_cast<unsigned>(nativeInfo.blockSize),
             static_cast<unsigned long long>(nativeTotal));
    Serial.println(buffer);

    check(sdTotal > 0, "sd сообщает общий объём");
    check(sdTotal == nativeTotal, "общий объём sd рассчитан в 64 битах");

    if (hfs.sd.spaceValid()) {
        check(sdUsed <= sdTotal, "занятый объём sd корректен");
        check(hfs.sd.freeSpace() == sdTotal - sdUsed,
              "свободный объём sd корректен");
    } else {
        Serial.println("[info] точная статистика used/free недоступна в sdfs core");
        check(sdUsed == 0, "недостоверный занятый объём sd отклоняется");
        check(hfs.sd.freeSpace() == 0, "недостоверный свободный объём sd отклоняется");
    }
#else
    check(sdTotal > 0, "sd сообщает общий объём");
    check(hfs.sd.spaceValid(), "статистика sd доступна");
    check(sdUsed <= sdTotal, "занятый объём sd корректен");
    check(hfs.sd.freeSpace() == sdTotal - sdUsed,
          "свободный объём sd корректен");
#endif
}

// проверить удаление файлов
void testRemove() {
    Serial.println("\nудаление");

    check(hfs.remove("/hfs_test/sub/nested.txt"),
          "удаление вложенного файла littlefs");
    check(hfs.remove("/hfs_test/file.txt"),
          "удаление файла littlefs");
    check(!fileExists("/hfs_test/file.txt"),
          "удалённый файл littlefs не открывается");

    check(hfs.remove("/sd/hfs_test/sub/nested.txt"),
          "удаление вложенного файла sd");
    check(hfs.remove("/sd/hfs_test/file.txt"),
          "удаление файла sd");
    check(!fileExists("/sd/hfs_test/file.txt"),
          "удалённый файл sd не открывается");
}

// запустить тесты
void runTests() {
    cleanupTestFiles();

    testRouting();
    testInvalidPaths();
    testDirectories();
    testFiles();
    testListDir();
    testSpace();
    testRemove();

    cleanupTestFiles();

    Serial.println("\nрезультат");
    Serial.print("успешно: ");
    Serial.println(passedTests);
    Serial.print("ошибок: ");
    Serial.println(failedTests);
    Serial.println(failedTests ? "HYBRIDFS TEST FAILED" : "HYBRIDFS TEST PASSED");
}

void setup() {
    Serial.begin(115200);
    delay(500);

#if defined(ESP8266)
    Serial.println("\nplatform: ESP8266");
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial.println("\nplatform: ESP32-S3");
#elif defined(ESP32)
    Serial.println("\nplatform: ESP32");
#endif

    bool flashReady = beginFlash();
    check(flashReady, "монтирование littlefs");
    if (!flashReady) {
        Serial.println("тест остановлен: littlefs не подключена");
        return;
    }

    bool sdReady = false;

#if defined(ESP8266)

    // esp8266 sdfs через spi
    {
        const uint8_t cs = SS;
        const uint8_t frequencyMHz = 10;

        Serial.printf("sd: SDFS SPI cs=%u frequency=%u MHz\n",
                      static_cast<unsigned>(cs),
                      static_cast<unsigned>(frequencyMHz));

        SPI.begin();
        SDFS.setConfig(SDFSConfig(cs, SD_SCK_MHZ(frequencyMHz)));
        sdReady = SDFS.begin();

        if (sdReady) {
            selectedSD = static_cast<fs::FS*>(&SDFS);
            hfs.setFS(LittleFS, SDFS);
        }
    }

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

    // esp32-s3 cam sd_mmc 1-bit встроенный слот
    {
        const int clk = 39;
        const int cmd = 38;
        const int d0 = 40;

        Serial.printf("sd: SD_MMC 1-bit clk=%d cmd=%d d0=%d\n", clk, cmd, d0);

        sdReady = SD_MMC.setPins(clk, cmd, d0) &&
                  SD_MMC.begin("/sdcard", true, true);

        if (sdReady) {
            selectedSD = static_cast<fs::FS*>(&SD_MMC);
            hfs.setFS(LittleFS, SD_MMC);
        }
    }

    // esp32-s3 cam sd_mmc 4-bit встроенный слот (другие платы)
    // заменить d1 d2 d3 на gpio из схемы платы
    // {
    //     const int clk = 39;
    //     const int cmd = 38;
    //     const int d0 = 40;
    //     const int d1 = -1;
    //     const int d2 = -1;
    //     const int d3 = -1;

    //     Serial.printf("sd: SD_MMC 4-bit clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d\n",
    //                   clk, cmd, d0, d1, d2, d3);

    //     sdReady = SD_MMC.setPins(clk, cmd, d0, d1, d2, d3) &&
    //               SD_MMC.begin("/sdcard", false, true);

    //     if (sdReady) {
    //         selectedSD = static_cast<fs::FS*>(&SD_MMC);
    //         hfs.setFS(LittleFS, SD_MMC);
    //     }
    // }

    /*
    // esp32-s3 внешний модуль sd через spi
    // заменить gpio на фактическое подключение
    {
        const int cs = 10;
        const int sck = 12;
        const int miso = 13;
        const int mosi = 11;
        const uint32_t frequency = 4000000;

        Serial.printf("sd: external SD SPI cs=%d sck=%d miso=%d mosi=%d\n",
                      cs, sck, miso, mosi);

        SPI.begin(sck, miso, mosi, cs);
        sdReady = SD.begin(cs, SPI, frequency);

        if (sdReady) {
            selectedSD = static_cast<fs::FS*>(&SD);
            hfs.setFS(LittleFS, SD);
        }
    }
    */

#elif defined(ESP32)

    // esp32-cam sd_mmc 4-bit встроенный слот
    // {
    //     Serial.println("sd: SD_MMC 4-bit clk=14 cmd=15 d0=2 d1=4 d2=12 d3=13");

    //     sdReady = SD_MMC.begin("/sdcard", false, true);

    //     if (sdReady) {
    //         selectedSD = static_cast<fs::FS*>(&SD_MMC);
    //         hfs.setFS(LittleFS, SD_MMC);
    //     }
    // }

    // esp32-cam встроенный слот через sd.h и spi
    // {
    //     const int cs = 13;
    //     const int sck = 14;
    //     const int miso = 2;
    //     const int mosi = 15;
    //     const uint32_t frequency = 4000000;

    //     Serial.printf("sd: SD SPI cs=%d sck=%d miso=%d mosi=%d\n",
    //                   cs, sck, miso, mosi);

    //     SPI.begin(sck, miso, mosi, cs);
    //     sdReady = SD.begin(cs, SPI, frequency);

    //     if (sdReady) {
    //         selectedSD = static_cast<fs::FS*>(&SD);
    //         hfs.setFS(LittleFS, SD);
    //     }
    // }

#endif

    check(sdReady, "монтирование sd");
    if (!sdReady) {
        Serial.println("тест остановлен: sd не подключена");
        return;
    }

    runTests();
}

void loop() {}
