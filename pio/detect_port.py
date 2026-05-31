#!/usr/bin/env python3
"""
Detects connected USB-to-Serial development boards using PlatformIO's device listing.
Prints only the chosen port to stdout, and all interactive menus to stderr.
Supports optional hint-based filtering to auto-select specific boards.
"""
import sys
import json
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(description="Smart port detector for PlatformIO")
    parser.add_argument("port", nargs="?", default=None, help="Direct port override (e.g., /dev/ttyUSB0)")
    parser.add_argument("--hint", default=None, help="Case-insensitive keyword to match in port description or HWID")
    
    args = parser.parse_args()

    # If a port override is specified directly (and is not an empty string), return it immediately
    if args.port:
        print(args.port)
        return 0

    try:
        # Run pio device list --json-output
        result = subprocess.run(
            ["pio", "device", "list", "--json-output"],
            capture_output=True,
            text=True,
            check=True
        )
        devices = json.loads(result.stdout)
    except Exception as e:
        sys.stderr.write(f"Error querying PlatformIO devices: {e}\n")
        return 1

    # Filter for USB serial ports
    usb_devices = []
    for d in devices:
        port = d.get("port", "")
        desc = d.get("description", "").lower()
        hwid = d.get("hwid", "").lower()
        
        # Exclude legacy motherboard/virtual serial ports (/dev/ttyS* on Linux)
        if "/dev/ttyS" in port and not ("usb" in desc or "usb" in hwid):
            continue
            
        # Match standard USB serial devices
        is_usb = (
            "usb" in port.lower() or 
            "acm" in port.lower() or 
            "com" in port.lower() or
            "usb" in desc or 
            "uart" in desc or 
            "silicon" in desc or 
            "ch34" in desc or 
            "ch91" in desc or 
            "cp21" in desc or 
            "ftdi" in desc
        )
        if is_usb:
            usb_devices.append(d)

    if not usb_devices:
        sys.stderr.write("[WARNING] No active USB-to-Serial devices detected!\n")
        # Fallback to standard platform default if none found
        default_port = "/dev/ttyUSB0"
        sys.stderr.write(f"Defaulting to: {default_port}\n")
        print(default_port)
        return 0

    # Apply hint matching if a hint is provided
    candidates = usb_devices
    if args.hint:
        hint_lower = args.hint.lower()
        hint_matches = []
        for d in usb_devices:
            port = d.get("port", "").lower()
            desc = d.get("description", "").lower()
            hwid = d.get("hwid", "").lower()
            if hint_lower in port or hint_lower in desc or hint_lower in hwid:
                hint_matches.append(d)
        
        if hint_matches:
            sys.stderr.write(f"[HINT] Found {len(hint_matches)} port(s) matching keyword '{args.hint}'\n")
            candidates = hint_matches
        else:
            sys.stderr.write(f"[HINT] No ports matched keyword '{args.hint}'. Showing all active USB devices.\n")

    if len(candidates) == 1:
        chosen = candidates[0]["port"]
        sys.stderr.write(f"[DETECTED] Selected serial port: {chosen} ({candidates[0].get('description', 'USB Serial')})\n")
        print(chosen)
        return 0

    # Multiple devices found in candidate list
    sys.stderr.write("\nMultiple USB serial devices detected. Please select one:\n")
    for idx, d in enumerate(candidates):
        sys.stderr.write(f" [{idx + 1}] {d['port']} - {d.get('description', 'N/A')} ({d.get('hwid', 'N/A')})\n")
    
    sys.stderr.write("\nEnter index [1]: ")
    sys.stderr.flush()
    
    # Read selection from stdin
    try:
        try:
            with open('/dev/tty', 'r') as tty:
                line = tty.readline().strip()
        except Exception:
            line = sys.stdin.readline().strip()
            
        if not line:
            choice = 0
        else:
            choice = int(line) - 1
            if choice < 0 or choice >= len(candidates):
                sys.stderr.write("Invalid selection, defaulting to first option.\n")
                choice = 0
    except ValueError:
        sys.stderr.write("Invalid number, defaulting to first option.\n")
        choice = 0
    except KeyboardInterrupt:
        sys.stderr.write("\nSelection cancelled.\n")
        return 1

    chosen = candidates[choice]["port"]
    sys.stderr.write(f"[SELECTED] Using serial port: {chosen}\n")
    print(chosen)
    return 0

if __name__ == "__main__":
    sys.exit(main())
