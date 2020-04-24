#include "buffer.h"

void buffer_read_init(read_buffer_t *buffer, const char *data, size_t size) {
    buffer->buffer = data;
    buffer->size = size;
    buffer->index = 0;
}

void buffer_write_init(write_buffer_t *buffer, char *data, size_t size) {
    buffer->buffer = data;
    buffer->size = size;
    buffer->index = 0;
}

int buffer_read_buffer(read_buffer_t *buffer, char *data, size_t size) {
    int i;

    if (buffer->size - buffer->index < size) {
        return 1;
    }

    for (i = 0; i < size; i++) {
        data[i] = buffer->buffer[buffer->index++];
    }

    return 0;
}

int buffer_write_buffer(write_buffer_t *buffer, const char *data, size_t size) {
    int i;

    if (buffer->size - buffer->index < size) {
        return 1;
    }

    for (i = 0; i < size; i++) {
        buffer->buffer[buffer->index++] = data[i];
    }

    return 0;
}

int buffer_read_uint32(read_buffer_t *buffer, uint32_t *data) {
    if (buffer->size - buffer->index < sizeof(uint32_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 8;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 16;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 24;
    return 0;
}

int buffer_write_uint32(write_buffer_t *buffer, uint32_t data) {
    if (buffer->size - buffer->index < sizeof(uint32_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    buffer->buffer[buffer->index++] = (data >> 8) & 0xff;
    buffer->buffer[buffer->index++] = (data >> 16) & 0xff;
    buffer->buffer[buffer->index++] = (data >> 24) & 0xff;
    return 0;
}

int buffer_read_uint16(read_buffer_t *buffer, uint16_t *data) {
    if (buffer->size - buffer->index < sizeof(uint16_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 8;
    return 0;
}

int buffer_write_uint16(write_buffer_t *buffer, uint16_t data) {
    if (buffer->size - buffer->index < sizeof(uint16_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    buffer->buffer[buffer->index++] = (data >> 8) & 0xff;
    return 0;
}

int buffer_read_uint8(read_buffer_t *buffer, uint8_t *data) {
    if (buffer->size - buffer->index < sizeof(uint8_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    return 0;
}

int buffer_write_uint8(write_buffer_t *buffer, uint8_t data) {
    if (buffer->size - buffer->index < sizeof(uint8_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    return 0;
}

int buffer_read_int32(read_buffer_t *buffer, int32_t *data) {
    if (buffer->size - buffer->index < sizeof(int32_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 8;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 16;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 24;
    return 0;
}

int buffer_write_int32(write_buffer_t *buffer, int32_t data) {
    if (buffer->size - buffer->index < sizeof(int32_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    buffer->buffer[buffer->index++] = (data >> 8) & 0xff;
    buffer->buffer[buffer->index++] = (data >> 16) & 0xff;
    buffer->buffer[buffer->index++] = (data >> 24) & 0xff;
    return 0;
}

int buffer_read_int16(read_buffer_t *buffer, int16_t *data) {
    if (buffer->size - buffer->index < sizeof(int16_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    *data |= (buffer->buffer[buffer->index++] & 0xff) << 8;
    return 0;
}

int buffer_write_int16(write_buffer_t *buffer, int16_t data) {
    if (buffer->size - buffer->index < sizeof(int16_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    buffer->buffer[buffer->index++] = (data >> 8) & 0xff;
    return 0;
}

int buffer_read_int8(read_buffer_t *buffer, int8_t *data) {
    if (buffer->size - buffer->index < sizeof(int8_t)) {
        return 1;
    }

    *data = buffer->buffer[buffer->index++] & 0xff;
    return 0;
}

int buffer_write_int8(write_buffer_t *buffer, int8_t data) {
    if (buffer->size - buffer->index < sizeof(int8_t)) {
        return 1;
    }

    buffer->buffer[buffer->index++] = data & 0xff;
    return 0;
}
