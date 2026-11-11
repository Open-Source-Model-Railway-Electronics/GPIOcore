# guiwidgets.py

import tkinter as tk

class InputBlock(tk.Frame):
    def __init__(self, master, inputObj):
        super().__init__(master)
        self.inputObj = inputObj
        self.label = tk.Label(self, text=f"Input {inputObj.index}")
        self.label.pack()
        # Add more entry widgets for each property as needed

class OutputBlock(tk.Frame):
    def __init__(self, master, outputObj):
        super().__init__(master)
        self.outputObj = outputObj
        self.label = tk.Label(self, text=f"Output {outputObj.index}")
        self.label.pack()
        # Add more entry widgets for each property as needed
