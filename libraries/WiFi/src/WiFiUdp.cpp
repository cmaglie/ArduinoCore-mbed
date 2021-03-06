#include "WiFiUdp.h"

extern WiFiClass WiFi;

#ifndef WIFI_UDP_BUFFER_SIZE
#define WIFI_UDP_BUFFER_SIZE        508
#endif

arduino::WiFiUDP::WiFiUDP() {
    _packet_buffer = (uint8_t*)malloc(WIFI_UDP_BUFFER_SIZE);
    _current_packet = NULL;
    _current_packet_size = 0;
    // if this malloc fails then ::begin will fail
}

uint8_t arduino::WiFiUDP::begin(uint16_t port) {
    // success = 1, fail = 0

    nsapi_error_t rt = _socket.open(WiFi.getNetwork());
    if (rt != NSAPI_ERROR_OK) {
        return 0;
    }

    if (!_packet_buffer) {
        return 0;
    }

    // do not block when trying to read from socket
    _socket.set_blocking(false);

    return 1;
}

void arduino::WiFiUDP::stop() {
    _socket.close();
}

// we should get the octets out of the IPAddress... there's nothing for it right now
// int arduino::WiFiUDP::beginPacket(IPAddress ip, uint16_t port) {
// }

int arduino::WiFiUDP::beginPacket(const char *host, uint16_t port) {
    _host = host;
    _port = port;

    return 1;
}

int arduino::WiFiUDP::endPacket() {
    return 1;
}

// Write a single byte into the packet
size_t arduino::WiFiUDP::write(uint8_t byte) {
    uint8_t buffer[1] = { byte };
    SocketAddress addr(_host, _port);
    return _socket.sendto(addr, buffer, 1);
}

// Write size bytes from buffer into the packet
size_t arduino::WiFiUDP::write(const uint8_t *buffer, size_t size) {
    SocketAddress addr(_host, _port);
    return _socket.sendto(addr, buffer, size);
}

int arduino::WiFiUDP::parsePacket() {
    nsapi_size_or_error_t ret = _socket.recvfrom(NULL, _packet_buffer, WIFI_UDP_BUFFER_SIZE);

    if (ret == NSAPI_ERROR_WOULD_BLOCK) {
        // no data
        return 0;
    }
    // error codes below zero are errors
    else if (ret <= 0) {
        // something else went wrong, need some tracing info...
        return 0;
    }

    // set current packet states
    _current_packet = _packet_buffer;
    _current_packet_size = ret;

    return 1;
}

// Read a single byte from the current packet
int arduino::WiFiUDP::read() {
    // no current packet...
    if (_current_packet == NULL) {
        // try reading the next frame, if there is no data return
        if (parsePacket() == 0) return -1;
    }

    _current_packet++;

    // check for overflow
    if (_current_packet > _packet_buffer + _current_packet_size) {
        // try reading the next packet...
        if (parsePacket() == 1) {
            // if so, read first byte of next packet;
            return read();
        }
        else {
            // no new data... not sure what to return here now
            return -1;
        }
    }

    return _current_packet[0];
}

// Read up to len bytes from the current packet and place them into buffer
// Returns the number of bytes read, or 0 if none are available
int arduino::WiFiUDP::read(unsigned char* buffer, size_t len) {
    // Q: does Arduino read() function handle fragmentation? I won't for now...
    if (_current_packet == NULL) {
        if (parsePacket() == 0) return 0;
    }

    // how much data do we have in the current packet?
    int offset = _current_packet - _packet_buffer;
    if (offset < 0) {
        return 0;
    }

    int max_bytes = _current_packet_size - offset;
    if (max_bytes < 0) {
        return 0;
    }

    // at the end of the packet?
    if (max_bytes == 0) {
        // try read next packet...
        if (parsePacket() == 1) {
            return read(buffer, len);
        }
        else {
            return 0;
        }
    }

    if (len > max_bytes) len = max_bytes;

    // copy to target buffer
    memcpy(buffer, _current_packet, len);

    _current_packet += len;

    return len;
}