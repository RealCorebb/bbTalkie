#define I2S_PIN_BCK 2
#define I2S_PIN_WS  1
#define I2S_DATA_OUT_PIN 5
#define I2S_PIN_DIN 4
#define SD_MODE 6

#include "ESP_SR.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ESP_I2S.h>
#include "FS.h"
#include "FFat.h" // For FAT filesystem support

// I2S configuration
i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_32BIT;
i2s_mode_t mode = I2S_MODE_STD;
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;
int sampleRate = 16000;  // sample rate in Hz

static const sr_cmd_t sr_commands[] = {
  {0, "qi chuang la"},
  {1, "qian mian you zhuan"},
  {2, "qian mian zuo zhuan"},
};

I2SClass i2s;
File mp3File;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S MP3 player from FATFS");
  
  // Initialize SD card
  pinMode(SD_MODE, OUTPUT);
  digitalWrite(SD_MODE, HIGH);
  
  // Initialize I2S
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_DATA_OUT_PIN, I2S_PIN_DIN);
  if (!i2s.begin(mode, sampleRate, bps, slot)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
  
  // Configure I2S RX for speech recognition
  if (!i2s.configureRX(16000, bps, slot, I2S_RX_TRANSFORM_32_TO_16)) {
    Serial.println("Failed to configure I2S RX transformation!");
    return;
  }
  
  // Initialize ESP_SR for speech recognition
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_CHANNELS_STEREO, SR_MODE_WAKEWORD);
  Serial.println("Start ESP_SR");
  
  // Initialize filesystem
  if (!FFat.begin()) {
    Serial.println("FFat Mount Failed");
    return;
  }
  Serial.println("FFat Mounted Successfully");

  // Open MP3 file
  mp3File = FFat.open("/test.mp3", "r");
  if (!mp3File) {
    Serial.println("Failed to open test.mp3");
    return;
  }
  
  Serial.print("MP3 file size: ");
  Serial.println(mp3File.size());
}

void loop() {
  // Check if file is available and play it
  if (mp3File) {
    // Read the entire MP3 file into memory
    // Note: This method will only work for MP3 files that fit in available RAM
    size_t fileSize = mp3File.size();
    uint8_t *mp3Buffer = (uint8_t *)malloc(fileSize);
    
    if (mp3Buffer) {
      Serial.println("Reading MP3 file...");
      size_t bytesRead = mp3File.read(mp3Buffer, fileSize);
      
      if (bytesRead == fileSize) {
        Serial.println("Playing MP3...");
        // Play the MP3 file
        bool result = i2s.playMP3(mp3Buffer, fileSize);
        
        if (!result) {
          Serial.println("Error playing MP3");
        } else {
          Serial.println("MP3 playback complete");
        }
      } else {
        Serial.println("Failed to read complete MP3 file");
      }
      
      free(mp3Buffer);
    } else {
      Serial.println("Not enough memory to load MP3 file");
    }
    
    // Close the file
    mp3File.close();
    
    // Don't repeat immediately
    delay(5000);
    
    // Reopen the file for next playback
    mp3File = FFat.open("/test.mp3", "r");
  } else {
    Serial.println("MP3 file not available");
    delay(5000);
  }
}