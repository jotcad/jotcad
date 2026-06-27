import os

libdeps_dir = ".pio/libdeps"

def patch_file(file_path, target, replacement):
    if not os.path.exists(file_path):
        return False
    with open(file_path, "r") as f:
        content = f.read()
    if target in content and replacement not in content:
        content = content.replace(target, replacement)
        with open(file_path, "w") as f:
            f.write(content)
        print(f"[Patch] Patched {file_path}")
        return True
    return False

if os.path.exists(libdeps_dir):
    for env_name in os.listdir(libdeps_dir):
        pico_dir = os.path.join(libdeps_dir, env_name, "zenoh-pico")
        if not os.path.exists(pico_dir):
            continue
            
        print(f"[Patch] Found zenoh-pico under {env_name}, patching...")
        
        # 1. extra_script.py
        script_path = os.path.join(pico_dir, "extra_script.py")
        patch_file(script_path, "if PLATFORM == 'ststm32':", "if PLATFORM == 'ststm32' or PLATFORM == 'espressif8266':")
        patch_file(script_path, "if BOARD == 'opencr':", "if BOARD == 'opencr' or PLATFORM == 'espressif8266':")
        patch_file(script_path, 
                   'default_args = ["-DFRAG_MAX_SIZE=4096", "-DBATCH_UNICAST_SIZE=2048", "-DBATCH_MULTICAST_SIZE=2048"]', 
                   'default_args = ["-DFRAG_MAX_SIZE=4096", "-DBATCH_UNICAST_SIZE=2048", "-DBATCH_MULTICAST_SIZE=2048", "-DZ_FEATURE_MULTI_THREAD=0"]')
                   
        # 2. udp_multicast_opencr.cpp
        multicast_path = os.path.join(pico_dir, "src/link/transport/udp/udp_multicast_opencr.cpp")
        patch_file(multicast_path, "#include <WiFiUdp.h>", "#include <WiFiUdp.h>\n#if defined(ESP8266)\n#include <ESP8266WiFi.h>\n#endif")
        patch_file(multicast_path, 
                   "if (!sock->_udp->beginMulticast(*rep._iptcp._addr, rep._iptcp._port)) {", 
                   "#if defined(ESP8266)\n    if (!sock->_udp->beginMulticast(WiFi.localIP(), *rep._iptcp._addr, rep._iptcp._port)) {\n#else\n    if (!sock->_udp->beginMulticast(*rep._iptcp._addr, rep._iptcp._port)) {\n#endif")

        # 3. system.c
        system_path = os.path.join(pico_dir, "src/system/arduino/opencr/system.c")
        patch_file(system_path, "#include <FreeRTOS.h>\n#include <hw/driver/delay.h>", "#if defined(ESP8266)\n#include <Arduino.h>\n#else\n#include <FreeRTOS.h>\n#include <hw/driver/delay.h>\n#endif")
        
        target_sleep = """z_result_t z_sleep_us(size_t time) {
    delay_us(time);
    return 0;
}

z_result_t z_sleep_ms(size_t time) {
    delay_ms(time);
    return 0;
}"""
        replacement_sleep = """z_result_t z_sleep_us(size_t time) {
#if defined(ESP8266)
    delayMicroseconds(time);
#else
    delay_us(time);
#endif
    return 0;
}

z_result_t z_sleep_ms(size_t time) {
#if defined(ESP8266)
    delay(time);
#else
    delay_ms(time);
#endif
    return 0;
}"""
        patch_file(system_path, target_sleep, replacement_sleep)

        target_rand = """/*------------------ Random ------------------*/
uint8_t z_random_u8(void) { return random(0xFF); }

uint16_t z_random_u16(void) { return random(0xFFFF); }

uint32_t z_random_u32(void) { return random(0xFFFFFFFF); }"""
        replacement_rand_exact = """/*------------------ Random ------------------*/
#if defined(ESP8266)
extern uint32_t os_random(void);
uint8_t z_random_u8(void) { return (uint8_t)(os_random() & 0xFF); }
uint16_t z_random_u16(void) { return (uint16_t)(os_random() & 0xFFFF); }
uint32_t z_random_u32(void) { return os_random(); }
#else
uint8_t z_random_u8(void) { return random(0xFF); }
uint16_t z_random_u16(void) { return random(0xFFFF); }
uint32_t z_random_u32(void) { return random(0xFFFFFFFF); }
#endif"""
        patch_file(system_path, target_rand, replacement_rand_exact)

        # 4. system.c clock gettime
        target_clock = """void __z_clock_gettime(z_clock_t *ts) {
    uint64_t m = millis();
    ts->tv_sec = m / (uint64_t)1000000;
    ts->tv_nsec = (m % (uint64_t)1000000) * (uint64_t)1000;
}"""
        replacement_clock = """void __z_clock_gettime(z_clock_t *ts) {
    uint64_t m = millis();
    ts->tv_sec = m / (uint64_t)1000;
    ts->tv_nsec = (m % (uint64_t)1000) * (uint64_t)1000000;
}"""
        patch_file(system_path, target_clock, replacement_clock)

        # 5. tcp_opencr.cpp
        tcp_path = os.path.join(pico_dir, "src/link/transport/tcp/tcp_opencr.cpp")
        
        target_open = """static z_result_t _z_tcp_opencr_open(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t endpoint, uint32_t tout) {
    _ZP_UNUSED(tout);
    z_result_t ret = _Z_RES_OK;

    sock->_tcp = new WiFiClient();
    if (!sock->_tcp->connect(*endpoint._iptcp._addr, endpoint._iptcp._port)) {"""
        replacement_open = """static z_result_t _z_tcp_opencr_open(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t endpoint, uint32_t tout) {
    _ZP_UNUSED(tout);
    z_result_t ret = _Z_RES_OK;

    sock->_tcp = new WiFiClient();
    sock->_tcp->setNoDelay(true); // Disable Nagle's algorithm
    if (!sock->_tcp->connect(*endpoint._iptcp._addr, endpoint._iptcp._port)) {"""
        patch_file(tcp_path, target_open, replacement_open)

        target_read_write = """static size_t _z_tcp_opencr_read(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    if (sock._tcp->available() > 0) {
        // flawfinder: ignore
        int rb = sock._tcp->read(ptr, len);
        if (rb < 0) {
            return SIZE_MAX;
        }
        return (size_t)rb;
    }
    return 0;
}

static size_t _z_tcp_opencr_read_exact(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    while (n < len && sock._tcp->connected()) {
        size_t rb = _z_tcp_opencr_read(sock, pos, len - n);
        if (rb == SIZE_MAX) {
            return SIZE_MAX;
        }
        if (rb > 0) {
            n += rb;
            pos = _z_ptr_u8_offset(pos, rb);
        } else {
            delay(1);
        }
    }

    return n;
}

static size_t _z_tcp_opencr_write(_z_sys_net_socket_t sock, const uint8_t *ptr, size_t len) {
    sock._tcp->write(ptr, len);
    return len;
}"""
        replacement_read_write = """static size_t _z_tcp_opencr_read(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    if (!sock._tcp->connected()) {
        return 0; // EOF (Connection closed)
    }
    if (sock._tcp->available() > 0) {
        // flawfinder: ignore
        int rb = sock._tcp->read(ptr, len);
        if (rb < 0) {
            return SIZE_MAX;
        }
        return (size_t)rb;
    }
    return SIZE_MAX; // EWOULDBLOCK (No data available right now)
}

static size_t _z_tcp_opencr_read_exact(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    while (n < len && sock._tcp->connected()) {
        if (sock._tcp->available() > 0) {
            int rb = sock._tcp->read(pos, len - n);
            if (rb > 0) {
                n += rb;
                pos = _z_ptr_u8_offset(pos, rb);
            } else if (rb < 0) {
                return SIZE_MAX;
            }
        } else {
            delay(1);
        }
    }

    return n;
}

static size_t _z_tcp_opencr_write(_z_sys_net_socket_t sock, const uint8_t *ptr, size_t len) {
    size_t bw = sock._tcp->write(ptr, len);
    if (bw != len) {
        Serial.printf("[TCP] Write mismatch: len=%d, written=%d\\n", len, bw);
    }
    sock._tcp->flush(); // Force transmission of outgoing bytes
    return bw;
}"""
        patch_file(tcp_path, target_read_write, replacement_read_write)
