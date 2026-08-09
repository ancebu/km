/**
 * @file ring_buffer.c
 * @brief Ring buffer implementation for data logging
 */

#include "types.h"
#include <stdlib.h>
#include <string.h>

error_code_t ring_buffer_init(ring_buffer_t* buffer, uint16_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return ERR_INVALID_ARG;
    }
    buffer->data = (battery_pack_state_t*)calloc(capacity, sizeof(battery_pack_state_t));
    if (!buffer->data) {
        return ERR_MEMORY_ALLOC;
    }
    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->full = false;
    return ERR_OK;
}

void ring_buffer_destroy(ring_buffer_t* buffer) {
    if (buffer && buffer->data) {
        free(buffer->data);
        buffer->data = NULL;
    }
}

bool ring_buffer_push(ring_buffer_t* buffer, const battery_pack_state_t* data) {
    if (buffer == NULL || data == NULL || buffer->data == NULL) {
        return false;
    }
    
    // If buffer is full, overwrite oldest entry (move tail forward)
    if (buffer->full) {
        buffer->tail = (buffer->tail + 1) % buffer->capacity;
    }
    
    // Copy data to head position
    memcpy(&buffer->data[buffer->head], data, sizeof(battery_pack_state_t));
    
    // Advance head
    buffer->head = (buffer->head + 1) % buffer->capacity;
    
    // Update count and full flag
    if (buffer->count < buffer->capacity) {
        buffer->count++;
    }
    if (buffer->head == buffer->tail) {
        buffer->full = true;
    }
    
    return true;
}

bool ring_buffer_pop(ring_buffer_t* buffer, battery_pack_state_t* data) {
    if (buffer == NULL || data == NULL || buffer->data == NULL) {
        return false;
    }
    
    // If buffer is empty, nothing to pop
    if (buffer->count == 0) {
        return false;
    }
    
    // Copy data from tail position
    memcpy(data, &buffer->data[buffer->tail], sizeof(battery_pack_state_t));
    
    // Advance tail
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    
    // Update count and full flag
    buffer->count--;
    buffer->full = false;
    
    return true;
}

uint16_t ring_buffer_count(const ring_buffer_t* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    return buffer->count;
}

bool ring_buffer_is_full(const ring_buffer_t* buffer) {
    if (buffer == NULL) {
        return false;
    }
    return buffer->full;
}

bool ring_buffer_is_empty(const ring_buffer_t* buffer) {
    if (buffer == NULL) {
        return true;
    }
    return buffer->count == 0;
}
