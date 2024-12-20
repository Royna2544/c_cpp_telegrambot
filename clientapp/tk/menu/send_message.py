import tkinter
import tkinter.messagebox
from socket_interface import Sender

class SendMessage:
    def __init__(self, tk: tkinter.Toplevel, sender: Sender):
        self.tk = tk
        self.tk.title("Send Message")

        self.chat_label = tkinter.Label(self.tk, text="Chat:")
        self.chat_label.pack(pady=10)
        self.chat_entry = tkinter.Entry(self.tk, width=40)
        self.chat_entry.pack(pady=10)

        self.message_label = tkinter.Label(self.tk, text="Message:")
        self.message_label.pack(pady=10)
        self.message_entry = tkinter.Text(self.tk, height=10, width=40)
        self.message_entry.pack(pady=10)

        self.send_button = tkinter.Button(self.tk, text="Send", command=self.send_message)
        self.send_button.pack(pady=10)

        self.text_mode = True
        def command_func():
            self.text_mode = not self.text_mode
            self.text_mode_switch.config(text="Text Mode" if self.text_mode else "Binary Mode")
        self.text_mode_switch = tkinter.Button(self.tk, text="Text Mode", command=command_func)
        self.text_mode_switch.pack(pady=10)

        self.sender = sender
    
    def send_message(self):
        message = self.message_entry.get("1.0", tkinter.END)
        try:
            chatId = int(self.chat_entry.get())
        except ValueError:
            tkinter.messagebox.showerror("Error", f'Invalid Chat ID: {self.chat_entry.get()}')
            return
        if self.sender.send_message(message, chatId, text_mode=self.text_mode):
            tkinter.messagebox.showinfo("Success", "Message sent successfully")
            try:
                self.message_entry.delete("1.0", tkinter.END)
            except tkinter.TclError:
                pass  # Text widget already deleted by the user
        else:
            tkinter.messagebox.showerror("Error", "Failed to send message")