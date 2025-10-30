/*
 * Simple IMA ADPCM Codec
 * 4:1 compression ratio (16-bit PCM -> 4-bit ADPCM)
 * 
 * Usage:
 *   adpcm_state_t state = {0}; // Initialize once
 *   adpcm_encode(pcm_input, pcm_samples, adpcm_output, &state);
 *   adpcm_decode(adpcm_input, adpcm_bytes, pcm_output, &state);
 */

#ifndef SIMPLE_ADPCM_H
#define SIMPLE_ADPCM_H

#include <stdint.h>
#include <stddef.h>

// ADPCM state (keep this between encode/decode calls)
typedef struct {
    int16_t predicted_sample;  // Last predicted value
    int8_t step_index;         // Current step size index
} adpcm_state_t;

// Step size table for IMA ADPCM
static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// Index adjustment table
static const int8_t index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

// Clamp value to int16_t range
static inline int16_t clamp_int16(int32_t val) {
    if (val > 32767) return 32767;
    if (val < -32768) return -32768;
    return (int16_t)val;
}

// Clamp step index to valid range
static inline int8_t clamp_step_index(int32_t index) {
    if (index < 0) return 0;
    if (index > 88) return 88;
    return (int8_t)index;
}

/**
 * Encode PCM samples to ADPCM
 * 
 * @param pcm_input     Input PCM samples (16-bit signed)
 * @param num_samples   Number of PCM samples
 * @param adpcm_output  Output ADPCM data (4-bit per sample, packed as bytes)
 * @param state         ADPCM state (persistent between calls)
 * @return              Number of bytes written to adpcm_output
 */
static inline size_t adpcm_encode(const int16_t *pcm_input, size_t num_samples, 
                                  uint8_t *adpcm_output, adpcm_state_t *state) {
    size_t output_bytes = 0;
    uint8_t nibble_buffer = 0;
    int nibble_count = 0;
    
    for (size_t i = 0; i < num_samples; i++) {
        int16_t sample = pcm_input[i];
        int16_t step = step_table[state->step_index];
        
        // Calculate difference
        int32_t diff = sample - state->predicted_sample;
        uint8_t code = 0;
        
        // Encode sign bit
        if (diff < 0) {
            code = 8;
            diff = -diff;
        }
        
        // Quantize the difference
        int32_t temp_step = step;
        if (diff >= temp_step) {
            code |= 4;
            diff -= temp_step;
        }
        temp_step >>= 1;
        if (diff >= temp_step) {
            code |= 2;
            diff -= temp_step;
        }
        temp_step >>= 1;
        if (diff >= temp_step) {
            code |= 1;
        }
        
        // Reconstruct the prediction
        int32_t diff_q = 0;
        if (code & 4) diff_q += step;
        if (code & 2) diff_q += step >> 1;
        if (code & 1) diff_q += step >> 2;
        diff_q += step >> 3;
        
        if (code & 8) {
            state->predicted_sample = clamp_int16(state->predicted_sample - diff_q);
        } else {
            state->predicted_sample = clamp_int16(state->predicted_sample + diff_q);
        }
        
        // Update step index
        state->step_index = clamp_step_index(state->step_index + index_table[code]);
        
        // Pack nibble
        if (nibble_count == 0) {
            nibble_buffer = code;
            nibble_count = 1;
        } else {
            adpcm_output[output_bytes++] = (nibble_buffer) | (code << 4);
            nibble_count = 0;
        }
    }
    
    // Handle odd number of samples
    if (nibble_count == 1) {
        adpcm_output[output_bytes++] = nibble_buffer;
    }
    
    return output_bytes;
}

/**
 * Decode ADPCM data to PCM
 * 
 * @param adpcm_input   Input ADPCM data (4-bit per sample, packed as bytes)
 * @param num_bytes     Number of ADPCM bytes
 * @param pcm_output    Output PCM samples (16-bit signed)
 * @param state         ADPCM state (persistent between calls)
 * @return              Number of PCM samples written
 */
static inline size_t adpcm_decode(const uint8_t *adpcm_input, size_t num_bytes,
                                  int16_t *pcm_output, adpcm_state_t *state) {
    size_t output_samples = 0;
    
    for (size_t i = 0; i < num_bytes; i++) {
        uint8_t byte = adpcm_input[i];
        
        // Decode low nibble
        uint8_t code = byte & 0x0F;
        int16_t step = step_table[state->step_index];
        
        int32_t diff_q = 0;
        if (code & 4) diff_q += step;
        if (code & 2) diff_q += step >> 1;
        if (code & 1) diff_q += step >> 2;
        diff_q += step >> 3;
        
        if (code & 8) {
            state->predicted_sample = clamp_int16(state->predicted_sample - diff_q);
        } else {
            state->predicted_sample = clamp_int16(state->predicted_sample + diff_q);
        }
        
        pcm_output[output_samples++] = state->predicted_sample;
        state->step_index = clamp_step_index(state->step_index + index_table[code]);
        
        // Decode high nibble
        code = (byte >> 4) & 0x0F;
        step = step_table[state->step_index];
        
        diff_q = 0;
        if (code & 4) diff_q += step;
        if (code & 2) diff_q += step >> 1;
        if (code & 1) diff_q += step >> 2;
        diff_q += step >> 3;
        
        if (code & 8) {
            state->predicted_sample = clamp_int16(state->predicted_sample - diff_q);
        } else {
            state->predicted_sample = clamp_int16(state->predicted_sample + diff_q);
        }
        
        pcm_output[output_samples++] = state->predicted_sample;
        state->step_index = clamp_step_index(state->step_index + index_table[code]);
    }
    
    return output_samples;
}

/**
 * Reset ADPCM state (call before encoding/decoding new audio stream)
 */
static inline void adpcm_reset(adpcm_state_t *state) {
    state->predicted_sample = 0;
    state->step_index = 0;
}

#endif // SIMPLE_ADPCM_H