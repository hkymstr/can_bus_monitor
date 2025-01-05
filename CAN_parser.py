import tkinter as tk
from tkinter import filedialog, ttk, messagebox
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import numpy as np

class CANViewer(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("CAN Log Viewer")
        self.state('zoomed')  # Start maximized

        # Data storage
        self.data = None
        self.unique_ids = []
        self.id_vars = {}  # Store checkbutton variables
        self.byte_vars = {}  # Store byte selection variables
        self.current_file = None
        self.selected_byte = tk.IntVar(value=0)  # Default to first byte
        self.show_all_bytes = tk.BooleanVar(value=False)  # Toggle for showing all bytes

        # Create main menu
        self.menu_bar = tk.Menu(self)
        self.config(menu=self.menu_bar)

        # File menu
        file_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Open Log File", command=self.load_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.quit)

        # Create main layout
        self.create_layout()

    def create_layout(self):
        # Create main container
        main_container = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        main_container.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Create left panel for controls
        left_panel = ttk.Frame(main_container)
        main_container.add(left_panel, weight=1)

        # Create ID selection frame with scrollbar
        id_frame = ttk.LabelFrame(left_panel, text="CAN IDs")
        id_frame.pack(fill=tk.BOTH, expand=True)

        # Add scrollbar to ID frame
        canvas = tk.Canvas(id_frame)
        scrollbar = ttk.Scrollbar(id_frame, orient="vertical", command=canvas.yview)
        self.scrollable_frame = ttk.Frame(canvas)

        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        # Pack scrollbar components
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # Create buttons for ID selection
        select_frame = ttk.Frame(left_panel)
        select_frame.pack(fill=tk.X, pady=5)
        ttk.Button(select_frame, text="Select All", command=self.select_all_ids).pack(side=tk.LEFT, padx=2)
        ttk.Button(select_frame, text="Deselect All", command=self.deselect_all_ids).pack(side=tk.LEFT, padx=2)

        # Add byte selection
        byte_frame = ttk.LabelFrame(left_panel, text="Byte Selection")
        byte_frame.pack(fill=tk.X, pady=5)

        # Add "Show All Bytes" checkbox
        ttk.Checkbutton(
            byte_frame,
            text="Show All Bytes",
            variable=self.show_all_bytes,
            command=self.update_plot
        ).grid(row=0, column=0, columnspan=2, padx=5, pady=5)

        # Add radio buttons for byte selection
        self.byte_radios = []
        for i in range(8):  # CAN messages have 8 bytes
            radio = ttk.Radiobutton(
                byte_frame,
                text=f"Byte {i}",
                variable=self.selected_byte,
                value=i,
                command=self.update_plot
            )
            radio.grid(row=1, column=i, padx=2)

        # Create right panel for plot
        self.right_panel = ttk.Frame(main_container)
        main_container.add(self.right_panel, weight=3)

        # Initialize empty plot
        self.fig = plt.figure(figsize=(10, 6))
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.right_panel)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Add matplotlib toolbar
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.right_panel)
        self.toolbar.update()

    def load_file(self):
        filename = filedialog.askopenfilename(
            filetypes=[("Text Files", "*.txt"), ("CSV Files", "*.csv"), ("All Files", "*.*")]
        )
        if filename:
            self.current_file = filename
            self.load_data(filename)

    def load_data(self, filename):
        try:
            df = pd.read_csv(filename)
            self.data = df

            # Get unique CAN IDs
            self.unique_ids = sorted(df['ID'].unique())

            # Clear existing checkbuttons
            for widget in self.scrollable_frame.winfo_children():
                widget.destroy()

            # Create new checkbuttons for each ID
            self.id_vars = {}
            for can_id in self.unique_ids:
                var = tk.BooleanVar(value=True)
                self.id_vars[can_id] = var
                frame = ttk.Frame(self.scrollable_frame)
                frame.pack(fill=tk.X)

                # Add checkbutton
                ttk.Checkbutton(
                    frame,
                    text=str(can_id),
                    variable=var,
                    command=self.update_plot
                ).pack(side=tk.LEFT)

                # Add message count label
                count = len(df[df['ID'] == can_id])
                ttk.Label(frame, text=f"({count} msgs)").pack(side=tk.LEFT)

            self.update_plot()

        except Exception as e:
            messagebox.showerror("Error", f"Failed to load file: {str(e)}")

    def select_all_ids(self):
        for var in self.id_vars.values():
            var.set(True)
        self.update_plot()

    def deselect_all_ids(self):
        for var in self.id_vars.values():
            var.set(False)
        self.update_plot()

    def get_byte_value(self, data_str, byte_index):
        # Split the hex string and get the specified byte
        bytes_list = data_str.split()
        if byte_index < len(bytes_list):
            return int(bytes_list[byte_index], 16)
        return None

    def get_combined_value(self, data_str):
        # Combine all bytes into a single value
        bytes_list = data_str.split()
        try:
            # Convert each byte to integer and combine them
            combined_value = 0
            for i, byte in enumerate(bytes_list):
                combined_value |= (int(byte, 16) << (8 * (len(bytes_list) - 1 - i)))
            return combined_value
        except (ValueError, IndexError):
            return None

    def update_plot(self):
        if self.data is None:
            return

        plt.figure(self.fig.number)
        self.fig.clear()
        ax = self.fig.add_subplot(111)

        if self.show_all_bytes.get():
            # Show combined value for each CAN ID
            for can_id in self.unique_ids:
                if self.id_vars[can_id].get():
                    id_data = self.data[self.data['ID'] == can_id]
                    timestamps = id_data['Timestamp'].tolist()
                    values = [self.get_combined_value(d) for d in id_data['Data']]
                    valid_data = [(t, v) for t, v in zip(timestamps, values) if v is not None]
                    if valid_data:
                        t, v = zip(*valid_data)
                        ax.plot(t, v, label=f'ID:{can_id}', linewidth=1, marker='.', markersize=2)

            ax.set_xlabel('Time (ms)')
            ax.set_ylabel('Combined Value')
            ax.set_title('CAN Bus Data - Combined Bytes')

        else:
            # Single byte view (original code)
            byte_idx = self.selected_byte.get()

            for can_id in self.unique_ids:
                if self.id_vars[can_id].get():
                    id_data = self.data[self.data['ID'] == can_id]
                    values = [self.get_byte_value(d, byte_idx) for d in id_data['Data']]
                    timestamps = id_data['Timestamp'].tolist()
                    valid_data = [(t, v) for t, v in zip(timestamps, values) if v is not None]
                    if valid_data:
                        t, v = zip(*valid_data)
                        ax.plot(t, v, label=str(can_id), linewidth=1, marker='.', markersize=2)

            ax.set_xlabel('Time (ms)')
            ax.set_ylabel(f'Byte {byte_idx} Value')
            ax.set_title(f'CAN Bus Data - Byte {byte_idx}')

        ax.grid(True)

        # Add legend with better positioning
        if len(ax.get_legend_handles_labels()[0]) > 15:
            # If many legend items, place it to the right of the plot
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
            self.fig.subplots_adjust(right=0.85)  # Make room for legend
        else:
            # If few legend items, place it inside the plot
            ax.legend(loc='best')

        self.fig.tight_layout()
        self.canvas.draw()

if __name__ == "__main__":
    app = CANViewer()
    app.mainloop()
