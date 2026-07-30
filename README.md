# HybridFS
Объединяет внутреннюю файловую систему и SD-карту под единым интерфейсом для ESP8266 и ESP32

- Работа с любой файловой системой через одну точку входа
- Реализация учитывает особенности и отличия esp8266 и esp32, упрощая создание кросс-платформенного кода
- Открытие, удаление, создание каталогов, вывод списка файлов и размера накопителя
- Обычные пути работают с LittleFS/SPIFFS: `/config.json`
- Пути с префиксом `/sd` работают с картой: `/sd/log.txt`
- Все пути должны начинаться с `/`

## Описание классов
### FSWrapper
```cpp
// создать обёртку для файловой системы
template <typename FsT>
explicit FSWrapper(FsT& filesystem);

// получить указатель на файловую систему
fs::FS* fs();

// проверить подключение файловой системы
bool valid();

// проверить подключение файловой системы
explicit operator bool();

// подключить файловую систему
template <typename FsT>
void setFS(FsT& filesystem);

#if defined(ESP32)
// подключить базовую fs без статистики объёма
void setFS(fs::FS& filesystem);
#endif

// отключить файловую систему
void reset();

// удалить файл
bool remove(const char* path);

// открыть файл
File open(const char* path, const char* mode);

// открыть файл для чтения
File openRead(const char* path);

// открыть файл для записи
File openWrite(const char* path);

// получить список файлов
String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// добавить список файлов в строку
void listDir(String& result,
                const char* path = "/",
                char pathDiv = ';',
                char sizeDiv = 0,
                const char* prefix = "");

// создать каталог
bool mkdir(const char* path);

// удалить пустой каталог
bool rmdir(const char* path);

// получить общий объём в байтах
uint64_t totalSpace();

// получить занятый объём в байтах
uint64_t usedSpace();

// проверить доступность точной статистики объёма
bool spaceValid();

// получить свободный объём в байтах
uint64_t freeSpace();
```

### HybridFS
```cpp
FSWrapper flash;
FSWrapper sd;

// создать гибридную файловую систему без карты памяти
template <typename FlashT>
explicit HybridFS(FlashT& flashFs);

// создать гибридную файловую систему с картой памяти
template <typename FlashT, typename SdT>
HybridFS(FlashT& flashFs, SdT& sdFs);

// подключить файловую систему без карты памяти
template <typename FlashT>
void setFS(FlashT& flashFs);

// подключить файловую систему с картой памяти
template <typename FlashT, typename SdT>
void setFS(FlashT& flashFs, SdT& sdFs);

// получить список файлов
String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// добавить список файлов в строку
void listDir(String& result, const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// удалить файл
bool remove(const char* path);

// открыть файл
File open(const char* path, const char* mode);

// открыть файл для чтения
File openRead(const char* path);

// открыть файл для записи
File openWrite(const char* path);

// создать каталог
bool mkdir(const char* path);

// удалить пустой каталог
bool rmdir(const char* path);

// получить указатель на выбранную файловую систему
fs::FS* fs(const char* path);
```

## ESP8266 — LittleFS + SDFS

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SDFS.h>
#include <SD.h>
#include "HybridFS.h"

HybridFS hfs;

void setup() {
    SPI.begin();

    bool flashOk = LittleFS.begin();

    SDFS.setConfig(SDFSConfig(SS, SD_SCK_MHZ(10)));
    bool sdOk = SDFS.begin();

    if (flashOk && sdOk) {
        hfs.setFS(LittleFS, SDFS);
    } else if (flashOk) {
        hfs.setFS(LittleFS);
    }
}
```

Замените `SS` на GPIO линии CS вашей SD-карты при необходимости

## ESP32 — LittleFS + SD через SPI

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SD.h>
#include "HybridFS.h"

HybridFS hfs;

void setup() {
    const int cs = 5;
    const int sck = 18;
    const int miso = 19;
    const int mosi = 23;

    bool flashOk = LittleFS.begin(false);

    SPI.begin(sck, miso, mosi, cs);
    bool sdOk = SD.begin(cs, SPI, 4000000);

    if (flashOk && sdOk) {
        hfs.setFS(LittleFS, SD);
    } else if (flashOk) {
        hfs.setFS(LittleFS);
    }
}
```

Укажите GPIO согласно разводке вашей платы или внешнего SD-модуля

## ESP32 — LittleFS + SD_MMC

Пример для ESP32-S3 CAM с линиями `CLK=39`, `CMD=38`, `D0=40`:

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include "HybridFS.h"

HybridFS hfs;

void setup() {
    bool flashOk = LittleFS.begin(false);

    bool pinsOk = SD_MMC.setPins(39, 38, 40);
    bool sdOk = pinsOk && SD_MMC.begin("/sdcard", true);

    if (flashOk && sdOk) {
        hfs.setFS(LittleFS, SD_MMC);
    } else if (flashOk) {
        hfs.setFS(LittleFS);
    }
}
```

`true` включает однобитный режим SDMMC

Для классической ESP32-CAM не вызывайте `setPins()`:

```cpp
bool sdOk = SD_MMC.begin("/sdcard", true);
```

## основные операции

```cpp
// запись во внутреннюю память
File flashFile = hfs.openWrite("/config.txt");
if (flashFile) {
    flashFile.print("value=1");
    flashFile.close();
}

// запись на sd
File sdFile = hfs.openWrite("/sd/log.txt");
if (sdFile) {
    sdFile.println("hello");
    sdFile.close();
}

// чтение
File file = hfs.openRead("/sd/log.txt");
if (file) {
    String text = file.readString();
    file.close();
}

// каталоги создаются по одному уровню
hfs.mkdir("/data");
hfs.mkdir("/data/cache");
hfs.mkdir("/sd/logs");

// удаление
hfs.remove("/config.txt");
hfs.rmdir("/data/cache");

// список всех файлов LittleFS и SD
String files = hfs.listDir("/", ';', ':');
Serial.println(files);
```

Пример результата `listDir("/", ';', ':')`:

```text
/config.txt:12;/data/file.bin:128;/sd/log.txt:32;
```

## объём файловых систем

```cpp
Serial.println(hfs.flash.totalSpace());
Serial.println(hfs.flash.usedSpace());
Serial.println(hfs.flash.freeSpace());

Serial.println(hfs.sd.totalSpace());
Serial.println(hfs.sd.usedSpace());
Serial.println(hfs.sd.freeSpace());
```

На ESP8266 точность статистики SD зависит от реализации SDFS. Проверить доступность точных значений можно через:

```cpp
if (hfs.sd.spaceValid()) {
    Serial.println(hfs.sd.freeSpace());
}
```

## правила путей

```text
/config.json        LittleFS
/data/file.bin      LittleFS
/sd                 корень SD
/sd/log.txt         SD
/sd/logs/file.txt   SD
file.txt            некорректный путь
sd/file.txt         некорректный путь
```
