import socket
import sys
import os

PORT = 1

class Color:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def find_esp32_mac():
    """Queries Windows Registry for paired Bluetooth devices to find ESP32_Bridge_Logger MAC address."""
    if os.name != 'nt':
        return None
    try:
        import winreg
        reg_path = r"SYSTEM\CurrentControlSet\Services\BTHPORT\Parameters\Devices"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, reg_path) as devices_key:
            for i in range(winreg.QueryInfoKey(devices_key)[0]):
                mac_raw = winreg.EnumKey(devices_key, i)
                try:
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, f"{reg_path}\\{mac_raw}") as device_key:
                        name, _ = winreg.QueryValueEx(device_key, "Name")
                        if isinstance(name, bytes):
                            try:
                                name_str = name.decode('utf-8').strip('\x00')
                            except Exception:
                                name_str = name.decode('utf-16').strip('\x00')
                        else:
                            name_str = str(name)
                        
                        if "ESP32_Bridge_Logger" in name_str:
                            return ":".join(mac_raw[j:j+2] for j in range(0, 12, 2)).upper()
                except Exception:
                    continue
    except Exception:
        pass
    return None

if os.name == 'nt':
    import msvcrt
    def get_key_bytes():
        """Reads a single keypress byte from Windows console."""
        return msvcrt.getch()
else:
    import tty
    import termios
    def get_key_bytes():
        """Reads a single keypress byte from Unix console."""
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            ch = sys.stdin.read(1)
            return ch.encode('utf-8')
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

def main():
    clear_screen()
    detected_mac = find_esp32_mac()
    if len(sys.argv) > 1:
        mac = sys.argv[1]
        print(f"Using MAC address from arguments: {mac}")
    elif detected_mac:
        mac = detected_mac
        print(f"Auto-detected ESP32_Bridge_Logger: {mac}")
    else:
        mac = "4C:C3:82:0C:94:72"
        print(f"Using default fallback MAC: {mac}")

    sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
    try:
        print(f"{Color.CYAN}Connecting to {mac} on port {PORT}...{Color.ENDC}")
        sock.connect((mac, PORT))
        print(f"{Color.GREEN}Connected! Press keys to send immediately. Ctrl+Q to quit.{Color.ENDC}\n")
        
        while True:
            ch = get_key_bytes()
            if ch == b'\x11':
                print(f"\n{Color.RED}Ctrl+Q pressed. Exiting...{Color.ENDC}")
                break
            
            sock.sendall(ch)
            
            printable = ""
            try:
                char_str = ch.decode('utf-8')
                if char_str.isprintable() and len(char_str) == 1:
                    printable = f"'{char_str}'"
            except Exception:
                pass
            
            hex_str = " ".join(f"0x{b:02x}" for b in ch)
            if printable:
                print(f"[SENT] {hex_str} ({printable})")
            else:
                print(f"[SENT] {hex_str}")

    except Exception as e:
        print(f"\n{Color.RED}Error: {e}{Color.ENDC}")
    finally:
        sock.close()
        print(f"{Color.CYAN}Connection closed.{Color.ENDC}")

if __name__ == '__main__':
    main()