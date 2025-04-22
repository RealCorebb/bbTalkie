#define I2S_PIN_BCK 2
#define I2S_PIN_WS  1
#define I2S_DATA_OUT_PIN 5
#define I2S_PIN_DIN 4
#define SD_MODE 6

#include <ESP_I2S.h>

const int frequency = 440;    // frequency of square wave in Hz
const int amplitude = 500;    // amplitude of square wave
const int sampleRate = 8000;  // sample rate in Hz

i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_32BIT;
i2s_mode_t mode = I2S_MODE_STD;
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;

const int halfWavelength = (sampleRate / frequency);  // half wavelength of square wave

int32_t sample = amplitude;  // current sample value
int count = 0;

I2SClass i2s;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S simple tone and microphone input");
  pinMode(SD_MODE,OUTPUT);
  digitalWrite(SD_MODE,HIGH);
  // start I2S at the sample rate with 16-bits per sample
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_DATA_OUT_PIN, I2S_PIN_DIN);
  if (!i2s.begin(mode, sampleRate, bps, slot)) {
    Serial.println("Failed to initialize I2S!");
    while (1);  // do nothing
  }
}

void loop() {
  if (count % halfWavelength == 0) {
    // invert the sample every half wavelength count multiple to generate square wave
    sample = -1 * sample;
  }

  i2s.write(0);  // Right channel
  i2s.write(sample);  // Left channel   ?? maybe the comment right and left is incorrect?

  // Read microphone data from the right channel
  int L = i2s.read();
  int R = i2s.read();
  Serial.print("L=");
  Serial.print(L); // Print microphone data
  Serial.print(",R=");
  Serial.println(R); // Print microphone data 
 
  // increment the counter for the next sample
  count++;
}
