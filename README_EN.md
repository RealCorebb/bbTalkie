# bbTalkie [English](https://github.com/RealCorebb/bbTalkie/blob/main/README_EN.md "English")

![image](https://github.com/RealCorebb/bbTalkie/blob/main/IMG/bbTalkie.jpg?raw=true)

## A Mini Walkie-Talkie That’s a Bit Different

Traditional walkie-talkies are usually bulky, clunky, and require you to press a button to talk. One day while cycling, I wondered—could I make a portable, hands-free walkie-talkie instead?

So I DIYed a **not-so-ordinary walkie-talkie**. It uses an **embedded neural network 🧠** to automatically detect human voice 🗣️, recognize keywords to trigger animations, and even convert other conversations into text bubbles on the screen. It’s surprisingly fun to use!

😄 [Afdian (Support the creator)](https://afdian.com/a/kuruibb "爱发电")  
🐧 QQ Group (for discussion only): 647186542  
🧵 [Threads](https://www.threads.net/@coreoobb "@coreoobb") @coreoobb  
▶️ Video: [YouTube](https://youtu.be/v0QcsWsoYbw "YouTube") | [Bilibili](https://www.bilibili.com/video/BV1qtBBBwEWv "Bilibili")

## Directory Structure

**CAD**
3D models, DXF files for laser cutting

**PCB**
Schematics, Gerber files, BOM, pick-and-place files, EDA project files

**assets**
Resource files such as animations, images, audio, and design assets

**esp-idf**
Main firmware source code, built with **ESP-IDF v5.4.3**, flashed to the ESP32

**tests**
Test programs used during development

**tools**
Helper tools for software development, such as font bitmap generation, graphics conversion, and audio conversion

## How to Use & FAQ

* You need an **ESP32-S3 with 16MB Flash and 8MB PSRAM**.
  Only this configuration supports **Octal PSRAM**—otherwise, PSRAM speed is insufficient and may cause the ESP-SR audio framework to freeze.

* The **external antenna version is recommended**:
  **ESP32-S3-WROOM-1U-N16R8**

* Compile and flash the firmware using the **ESP-IDF development environment**.

* Bluetooth camera control is implemented in a very basic form and currently supports **SONY cameras**.
  Since Bluetooth consumes a large amount of memory and is not commonly used, it is maintained in a separate branch named **`ble-camera`**.

* More content and updates may be added in the future as needed.

## References

**Development Frameworks**

* [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.3/esp32/get-started/index.html)
* [ESP-SR](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32/getting_started/readme.html)
