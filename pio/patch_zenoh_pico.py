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
