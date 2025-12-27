# bbTalkie [English](https://github.com/RealCorebb/bbTalkie/blob/main/README_EN.md "English")

![image](https://github.com/RealCorebb/bbTalkie/blob/main/IMG/bbTalkie.jpg?raw=true)

## 不太一样的迷你对讲机

普通对讲机又大又笨，还得按键才能讲话。 有天骑行时，我想——能不能做一款便携、免按键的对讲机？ 于是我 DIY 了一款不太一样的对讲机，使用嵌入式神经网络🧠，能自动检测人声🗣️、识别关键词触发动画，其它对话也能转文字气泡显示，非常好玩。

😄[爱发电](https://afdian.com/a/kuruibb "爱发电")   
🐧QQ 群（仅供交流）：647186542  
🧵[Threads](https://www.threads.net/@coreoobb "@coreoobb") @coreoobb  
▶️ 本期视频(Video): [Youtube](https://youtu.be/v0QcsWsoYbw "Youtube") | [ Bilibili](https://www.bilibili.com/video/BV1qtBBBwEWv " Bilibili")

# 禁止搬运到 Gitee

Designed By Corebb With Love From bbRealm!

# 目录结构：

**CAD** 3D模型、DXF切割图纸  
**PCB** PCB 原理图、Gerber 制板文件、BOM 、坐标文件、EDA工程
**assets** 资源文件，如动画图像、音频、设计文件  
**esp-idf** 软件主程序，使用ESP-IDF V5.4.3版本，烧录到 ESP32 中  
**tests** 开发过程中用到的测试程序  
**tools** 软件部分开发辅助工具，如字体取模、图形转换、音频转换等  

# 如何使用 & FAQ：
需要用ESP32-S3 16MB Flash 8MB PSRAM的版本（只有这个才能用上Octal PSRAM，不然PSRAM速度跟不上会导致ESP-SR这套音频框架卡死）  
建议使用外置天线的版本：ESP32-S3-WROOM-1U-N16R8  
使用ESP-IDF开发环境编译并烧录  

# 参考资料：

开发框架  
[ESP-IDF快速入门](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.3/esp32/get-started/index.html)
[ESP-SR](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32/getting_started/readme.html)
