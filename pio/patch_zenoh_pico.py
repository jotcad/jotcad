import os

# Path to zenoh-pico extra_script.py
script_path = ".pio/libdeps/esp8266/zenoh-pico/extra_script.py"

if os.path.exists(script_path):
    print("[Patch] Patching zenoh-pico extra_script.py for ESP8266 support...")
    with open(script_path, "r") as f:
        content = f.read()
    
    # Replace ststm32 check to include espressif8266
    target1 = "if PLATFORM == 'ststm32':"
    replacement1 = "if PLATFORM == 'ststm32' or PLATFORM == 'espressif8266':"
    
    target2 = "if BOARD == 'opencr':"
    replacement2 = "if BOARD == 'opencr' or PLATFORM == 'espressif8266':"
    
    target3 = 'default_args = ["-DFRAG_MAX_SIZE=4096", "-DBATCH_UNICAST_SIZE=2048", "-DBATCH_MULTICAST_SIZE=2048"]'
    replacement3 = 'default_args = ["-DFRAG_MAX_SIZE=4096", "-DBATCH_UNICAST_SIZE=2048", "-DBATCH_MULTICAST_SIZE=2048", "-DZ_FEATURE_MULTI_THREAD=0"]'
    
    modified = False
    if target1 in content:
        content = content.replace(target1, replacement1)
        modified = True
    if target2 in content:
        content = content.replace(target2, replacement2)
        modified = True
    if target3 in content:
        content = content.replace(target3, replacement3)
        modified = True
        
    if modified:
        with open(script_path, "w") as f:
            f.write(content)
        print("[Patch] Patching completed successfully.")
    else:
        print("[Patch] Target patterns not found or already patched.")
else:
    print("[Patch] zenoh-pico extra_script.py not found (yet).")

# Path to zenoh-pico udp_multicast_opencr.cpp
multicast_path = ".pio/libdeps/esp8266/zenoh-pico/src/link/transport/udp/udp_multicast_opencr.cpp"

if os.path.exists(multicast_path):
    print("[Patch] Patching zenoh-pico udp_multicast_opencr.cpp for ESP8266 multicast support...")
    with open(multicast_path, "r") as f:
        content = f.read()
        
    target_inc = "#include <WiFiUdp.h>"
    replacement_inc = "#include <WiFiUdp.h>\n#if defined(ESP8266)\n#include <ESP8266WiFi.h>\n#endif"
    
    target_call = "if (!sock->_udp->beginMulticast(*rep._iptcp._addr, rep._iptcp._port)) {"
    replacement_call = "#if defined(ESP8266)\n    if (!sock->_udp->beginMulticast(WiFi.localIP(), *rep._iptcp._addr, rep._iptcp._port)) {\n#else\n    if (!sock->_udp->beginMulticast(*rep._iptcp._addr, rep._iptcp._port)) {\n#endif"
    
    modified = False
    if target_inc in content and "ESP8266WiFi.h" not in content:
        content = content.replace(target_inc, replacement_inc)
        modified = True
    if target_call in content:
        content = content.replace(target_call, replacement_call)
        modified = True
        
    if modified:
        with open(multicast_path, "w") as f:
            f.write(content)
        print("[Patch] udp_multicast_opencr.cpp patched successfully.")
    else:
        print("[Patch] udp_multicast_opencr.cpp already patched or target not found.")
else:
    print("[Patch] udp_multicast_opencr.cpp not found.")

# Path to zenoh-pico system.c
system_path = ".pio/libdeps/esp8266/zenoh-pico/src/system/arduino/opencr/system.c"

if os.path.exists(system_path):
    print("[Patch] Patching zenoh-pico system.c for ESP8266 support...")
    with open(system_path, "r") as f:
        content = f.read()
        
    target_inc = "#include <FreeRTOS.h>\n#include <hw/driver/delay.h>"
    replacement_inc = "#if defined(ESP8266)\n#include <Arduino.h>\n#else\n#include <FreeRTOS.h>\n#include <hw/driver/delay.h>\n#endif"
    
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
    
    modified = False
    if target_inc in content:
        content = content.replace(target_inc, replacement_inc)
        modified = True
    if target_sleep in content:
        content = content.replace(target_sleep, replacement_sleep)
        modified = True
        
    target_rand = """/*------------------ Random ------------------*/
uint8_t z_random_u8(void) { return random(0xFF); }

uint16_t z_random_u16(void) { return random(0xFFFF); }

uint32_t z_random_u32(void) { return random(0xFFFFFFFF); }"""
    replacement_rand = """/*------------------ Random ------------------*/
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

    if target_rand in content:
        content = content.replace(target_rand, replacement_rand)
        modified = True
        
    if modified:
        with open(system_path, "w") as f:
            f.write(content)
        print("[Patch] system.c patched successfully.")
    else:
        print("[Patch] system.c already patched or target not found.")
else:
    print("[Patch] system.c not found.")
