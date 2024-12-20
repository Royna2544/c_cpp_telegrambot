import tkinter
import tkinter.messagebox
from socket_interface import Sender

class GetUptime:
    def __init__(self, tk: tkinter.Toplevel, sender: Sender):
        self.tk = tk
        self.tk.title("Get Uptime")

        self.label = tkinter.Label(self.tk, text="Get server uptime")
        self.label.pack(pady=10)
        
        self.get_button = tkinter.Button(self.tk, text='Get', command=self.get_uptime)
        self.get_button.pack(pady=10)
        self.sender = sender
    
    def get_uptime(self):
        uptime = self.sender.get_uptime()
        if uptime:
            tkinter.messagebox.showinfo("Uptime", f"Current server uptime: {uptime.uptime}")
        else:
            tkinter.messagebox.showerror("Error", "Failed to get server uptime")