"""Bike-Mate GUI launcher."""
import customtkinter as ctk
from bikemate.app import App


if __name__ == "__main__":
    root = ctk.CTk()
    App(root)
    root.mainloop()
