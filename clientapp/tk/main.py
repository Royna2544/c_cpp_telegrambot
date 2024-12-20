import tkinter as tk
import os
import ipaddress
import socket
import configparser
from typing import Callable, Optional
from tkinter import ttk
from tkinter import messagebox
from socket_interface import Sender
from logging_cfg import setup_logging
from pathlib import Path

from menu.send_message import SendMessage
from menu.get_uptime import GetUptime

setup_logging()
import logging

logger = logging.getLogger(__name__)

InterfaceNames = {
    socket.AF_INET: "IPv4",
    socket.AF_INET6: "IPv6",
}
if hasattr(socket, "AF_UNIX"):
    InterfaceNames[socket.AF_UNIX] = "LocalSocket"


class MainApp:
    class NetworkBackend:
        def __init__(
            self,
            name: str,
            type: socket.AddressFamily,
            needPort: bool,
            button: ttk.Radiobutton,
            addrVerify: Callable[[str], bool],
        ):
            self.name = name
            self.type = type
            self.needPort = needPort
            self.button = button
            self.addrVerify = addrVerify

    def verify_config(
        self, name: str, ip_path: str, port_str: str
    ) -> Optional[NetworkBackend]:
        if name not in InterfaceNames.values():
            logging.debug(f"Invalid network type: {name}")
            return None

        selected_backend = next(
            backend for backend in self.network_backends if name == backend.name
        )
        port = None
        if selected_backend.needPort:
            if not port_str:
                messagebox.showerror("Error", "Port number is required")
                return None

            # Validate port number (must be an integer between 1 and 65535)
            try:
                port = int(port_str)
            except ValueError:
                messagebox.showerror(
                    "Error", f"Invalid port number (must be an integer): {port_str}"
                )
                return None

            PORT_MAX = 1 << 16
            if port not in range(1, PORT_MAX + 1):
                messagebox.showerror(
                    "Error",
                    f"Invalid port number (must be between 1 and {PORT_MAX}): {port}",
                )
                return None

        # Verify IP address/path
        if not selected_backend.addrVerify(ip_path):
            messagebox.showerror("Error", f"Invalid IP address/path: {ip_path}")
            return None

        # Check if name is valid
        if name not in InterfaceNames.values():
            logging.debug(f"Invalid network type: {name}")
            return None

        af = None
        for key, value in InterfaceNames.items():
            if value == name:
                af = key
                break
        assert af

        # Create Sender instance
        self.sender = Sender(ip_path, af, port)
        return selected_backend

    def parse_file(self, filepath: Path):
        if not filepath.exists():
            logging.debug(f"File {filepath} does not exist")
            return None, None, None
        logging.debug(f"Reading file {filepath}")
        config = configparser.ConfigParser()
        config.read(filepath)
        if config.has_section(section="net"):
            ip_address = config.get(section="net", option="IP_ADDRESS", fallback=None)
            port_str = config.get(section="net", option="PORT", fallback=None)
            type_str = config.get(section="net", option="TYPE", fallback=None)
            logging.debug(
                f"Parsed: IP address: {ip_address} Port: {port_str} Type: {type_str}"
            )
            if not ip_address or not port_str or not type_str:
                return None, None, None

            if not self.verify_config(type_str, ip_address, port_str):
                logging.warning("Failed to verify parsed information")
                return None, None, None

            logging.info("Successfully parsed config file")
            assert self.sender
            return self.sender.address, self.sender.type, self.sender.port
        return None, None, None

    class WrapObj:
        def __init__(self, obj: Optional[tk.Toplevel]):
            self.obj = obj

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Telegram Bot Client")
        self.root.geometry("400x300")
        self.sender = None

        # Input for IP address or path
        ip_path_label = ttk.Label(self.root, text="IP Address/Path:")
        ip_path_label.pack(pady=5)
        ip_path_entry = ttk.Entry(self.root, width=40)
        ip_path_entry.pack(pady=5)

        ip_type_label = ttk.Label(self.root, text="Select Type:")
        ip_type_label.pack(pady=5)

        # radio buttons
        self.radio_frame = ttk.Frame(self.root)
        self.radio_frame.pack()
        self.network_backends = []
        self.radio_variable = tk.StringVar()

        def addBackend(type: socket.AddressFamily, addrVerify, needsPort: bool = True):
            # Create a radiobutton for the backend
            name = InterfaceNames[type]
            button = ttk.Radiobutton(
                self.radio_frame,
                text=name,
                variable=self.radio_variable,
                value=name,
                command=lambda: (
                    self.port_entry.config(state="disabled")
                    if not needsPort
                    else self.port_entry.config(state="normal")
                ),
            )
            button.grid(row=0, column=len(self.network_backends), padx=10)

            # Create a backend object
            backend = self.NetworkBackend(
                name=name,
                type=type,
                needPort=needsPort,
                button=button,
                addrVerify=addrVerify,
            )

            # Link the button to the backend and register it
            self.network_backends.append(backend)

        def ip_addr_verify(addr: str, version: int):
            try:
                return ipaddress.ip_address(addr).version == version
            except ValueError:
                return False

        def path_verify(addr: str):
            try:
                return Path(addr).exists()
            except (ValueError, OSError) as e:
                return False

        addBackend(socket.AF_INET, lambda s: ip_addr_verify(s, 4))
        addBackend(socket.AF_INET6, lambda s: ip_addr_verify(s, 6))
        if os.name != "nt":
            addBackend(socket.AF_UNIX, needsPort=False, addrVerify=path_verify)

        # Input for port number
        port_label = ttk.Label(self.root, text="Port:")
        port_label.pack(pady=5)
        self.port_entry = ttk.Entry(self.root, width=10)
        self.port_entry.pack(pady=5)

        # Load config file if it exists
        ipaddr, af, port = self.parse_file(Path.home() / "tgbotclient.ini")
        if ipaddr:
            ip_path_entry.insert(0, ipaddr)
        if af:
            self.radio_variable.set(InterfaceNames[af])
        if port:
            self.port_entry.insert(0, str(port))

        # Submit button
        def submit_action():
            ip_path = ip_path_entry.get()
            port_str = self.port_entry.get()

            if not ip_path:
                messagebox.showerror("Error", "IP address/path is required")
                return

            selected_backend = self.verify_config(
                self.radio_variable.get(), ip_path, port_str
            )
            if not selected_backend:
                messagebox.showerror("Error", "Failed to verify and create sender")
                return

            port = None
            if selected_backend.needPort:
                try:
                    port = int(port_str)
                except:
                    pass

            messagebox.showinfo("Success", f"Created {ip_path}:{port}")

        submit_button = ttk.Button(self.root, text="Submit", command=submit_action)
        submit_button.pack(pady=20)

        # Create a menu bar
        menu_bar = tk.Menu(self.root)

        # Create a File menu
        file_menu = tk.Menu(menu_bar, tearoff=0)
        file_menu.add_command(label="Exit", command=self.exit_application)
        file_menu.add_command(label="Save Config", command=self.save_config)
        menu_bar.add_cascade(label="File", menu=file_menu)

        # Create a Help menu
        help_menu = tk.Menu(menu_bar, tearoff=0)
        help_menu.add_command(label="About", command=self.show_about)
        menu_bar.add_cascade(label="Help", menu=help_menu)

        # Create a Commands menu
        send_message_menu = tk.Menu(menu_bar, tearoff=0)

        self.send_message_window = self.WrapObj(None)
        send_message_menu.add_command(
            label="Send Message",
            command=lambda: self.open_menu(self.send_message_window, SendMessage),
        )
        self.get_uptime_window = self.WrapObj(None)
        send_message_menu.add_command(
            label="Get uptime",
            command=lambda: self.open_menu(self.get_uptime_window, GetUptime),
        )
        menu_bar.add_cascade(label="Commands", menu=send_message_menu)

        # Attach the menu bar to the root window
        self.root.config(menu=menu_bar)

    def open_menu(self, win: WrapObj, cls):
        if not self.sender:
            messagebox.showerror("Error", "No network backend selected")
            return

        if win.obj:
            win.obj.deiconify()
            return
        win.obj = tk.Toplevel(self.root)

        def on_window_close():
            # Assert to slience pylance
            assert win.obj
            win.obj.destroy()
            win.obj = None

        win.obj.protocol("WM_DELETE_WINDOW", on_window_close)  # Bind close event
        cls(win.obj, self.sender)

    def show_about(self):
        messagebox.showinfo("About", "Telegram Bot Client\nVersion 1.0")

    def exit_application(self):
        self.root.quit()

    def save_config(self):
        if not self.sender:
            messagebox.showerror("Error", "No network backend selected")
            return
        config = configparser.ConfigParser()
        config["net"] = {
            "IP_ADDRESS": self.sender.address,
            "PORT": str(self.sender.port),
            "TYPE": InterfaceNames[self.sender.type],
        }
        with open(Path.home() / "tgbotclient.ini", "w") as configfile:
            config.write(configfile)
        messagebox.showinfo("Success", "Config saved")

    def loop(self):
        self.root.mainloop()


if __name__ == "__main__":
    MainApp().loop()
