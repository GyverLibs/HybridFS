This is an automatic translation and may be incorrect in some places. See the source README and examples for authoritative information.

# HybridFS
Combines an internal file system and SD card under a single interface for ESP8266 and ESP32

- Work with any file system through a single entry point
- The implementation takes into account the features and differences of esp8266 and esp32, simplifying the creation of cross-platform code.
- Opening, deleting, creating directories, listing files and storage size
- The usual paths work with LittleFS/SPIFFS:`/config.json`
- Prefixed paths`/sd`Working with the map:`/sd/log.txt`
- All paths must begin with`/`

## Description of classes
### FSWrapper
```cpp
// create a wrapper for the file system
template <typename FsT>
explicit FSWrapper(FsT& filesystem);

// filesystem pointer
fs::FS* fs();

// check the connection of the file system
bool valid();

// check the connection of the file system
explicit operator bool();

// filesystem
template <typename FsT>
void setFS(FsT& filesystem);

#if defined(ESP32)
// connect the base fs without volume statistics
void setFS(fs::FS& filesystem);
#endif

// disable
void reset();

// delete
bool remove(const char* path);

// file
File open(const char* path, const char* mode);

// read out
File openRead(const char* path);

// record
File openWrite(const char* path);

// file-list
String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// add a list of files to the line
void listDir(String& result,
                const char* path = "/",
                char pathDiv = ';',
                char sizeDiv = 0,
                const char* prefix = "");

// catalogue
bool mkdir(const char* path);

// delete
bool rmdir(const char* path);

// totalize
uint64_t totalSpace();

// byte
uint64_t usedSpace();

// Check the availability of accurate volume statistics
bool spaceValid();

// free-volume
uint64_t freeSpace();
```

### HybridFS
```cpp
FSWrapper flash;
FSWrapper sd;

// Create a hybrid file system without a memory card
template <typename FlashT>
explicit HybridFS(FlashT& flashFs);

// Create a hybrid file system with a memory card
template <typename FlashT, typename SdT>
HybridFS(FlashT& flashFs, SdT& sdFs);

// connect the file system without a memory card
template <typename FlashT>
void setFS(FlashT& flashFs);

// connect the file system with a memory card
template <typename FlashT, typename SdT>
void setFS(FlashT& flashFs, SdT& sdFs);

// file-list
String listDir(const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// add a list of files to the line
void listDir(String& result, const char* path = "/", char pathDiv = ';', char sizeDiv = 0);

// delete
bool remove(const char* path);

// file
File open(const char* path, const char* mode);

// read out
File openRead(const char* path);

// record
File openWrite(const char* path);

// catalogue
bool mkdir(const char* path);

// delete
bool rmdir(const char* path);

// Get a pointer to the selected file system
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

Replace.`SS`on the GPIO CS line of your SD card if necessary

## ESP32 - LittleFS + SD via SPI

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

Specify the GPIO according to the wiring of your board or external SD module

## ESP32 — LittleFS + SD_MMC

Example for ESP32-S3 CAM with lines`CLK=39`, `CMD=38`, `D0=40`:

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

`true`includes a single-bit SDMMC mode

For classic ESP32-CAM do not call`setPins()`:

```cpp
bool sdOk = SD_MMC.begin("/sdcard", true);
```

## principal

```cpp
// memory
File flashFile = hfs.openWrite("/config.txt");
if (flashFile) {
    flashFile.print("value=1");
    flashFile.close();
}

// recording
File sdFile = hfs.openWrite("/sd/log.txt");
if (sdFile) {
    sdFile.println("hello");
    sdFile.close();
}

// reading
File file = hfs.openRead("/sd/log.txt");
if (file) {
    String text = file.readString();
    file.close();
}

// Directories are created on one level
hfs.mkdir("/data");
hfs.mkdir("/data/cache");
hfs.mkdir("/sd/logs");

// removal
hfs.remove("/config.txt");
hfs.rmdir("/data/cache");

// List of all LittleFS and SD files
String files = hfs.listDir("/", ';', ':');
Serial.println(files);
```

Example of results`listDir("/", ';', ':')`:

```text
/config.txt:12;/data/file.bin:128;/sd/log.txt:32;
```

## file-system size

```cpp
Serial.println(hfs.flash.totalSpace());
Serial.println(hfs.flash.usedSpace());
Serial.println(hfs.flash.freeSpace());

Serial.println(hfs.sd.totalSpace());
Serial.println(hfs.sd.usedSpace());
Serial.println(hfs.sd.freeSpace());
```

On ESP8266, the accuracy of SD statistics depends on the implementation of SDFS. You can check the availability of accurate values through:

```cpp
if (hfs.sd.spaceValid()) {
    Serial.println(hfs.sd.freeSpace());
}
```

## rules

```text
/config.json        LittleFS
/data/file.bin      LittleFS
/sd                 корень SD
/sd/log.txt         SD
/sd/logs/file.txt   SD
file.txt            некорректный путь
sd/file.txt         некорректный путь
```
