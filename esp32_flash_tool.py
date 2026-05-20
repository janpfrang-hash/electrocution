"""
ESP32 Flash Tool
Flasht .bin Dateien auf ESP32 via esptool
Benötigt: pip install esptool
"""

import tkinter as tk
from tkinter import filedialog, ttk
import subprocess
import threading
import sys
import serial.tools.list_ports
import os

# ── Farben & Fonts ────────────────────────────────────────────────────────────
BG        = "#0d1117"
BG2       = "#161b22"
BG3       = "#21262d"
ACCENT    = "#238636"
ACCENT_H  = "#2ea043"
BORDER    = "#30363d"
TEXT      = "#e6edf3"
TEXT_DIM  = "#8b949e"
RED       = "#da3633"
YELLOW    = "#d29922"
MONO      = ("Consolas", 10)
SANS      = ("Segoe UI", 10)
SANS_B    = ("Segoe UI", 10, "bold")
TITLE_F   = ("Segoe UI", 13, "bold")


class FlashTool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESP32 Flash Tool")
        self.configure(bg=BG)
        self.resizable(True, True)
        self.minsize(680, 520)

        self.bin_path   = tk.StringVar()
        self.port_var   = tk.StringVar()
        self.flash_addr = tk.StringVar(value="0x0000")
        self.baud_var   = tk.StringVar(value="460800")
        self.flash_type = tk.StringVar(value="merged")

        self._build_ui()
        self._refresh_ports()

    # ── UI ────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        # Header
        hdr = tk.Frame(self, bg=BG, pady=16)
        hdr.pack(fill="x", padx=24)
        tk.Label(hdr, text="⚡ ESP32 Flash Tool",
                 font=TITLE_F, bg=BG, fg=TEXT).pack(side="left")
        tk.Label(hdr, text="via esptool v5",
                 font=("Segoe UI", 9), bg=BG, fg=TEXT_DIM).pack(side="left", padx=10, pady=3)

        # Separator
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")

        # Main content
        content = tk.Frame(self, bg=BG, padx=24, pady=16)
        content.pack(fill="x")

        # ── Binary File ───────────────────────────────────────────────────────
        self._section(content, "FIRMWARE")

        row1 = tk.Frame(content, bg=BG)
        row1.pack(fill="x", pady=(4, 12))

        entry_bg = tk.Frame(row1, bg=BORDER, padx=1, pady=1)
        entry_bg.pack(side="left", fill="x", expand=True)
        self.file_entry = tk.Entry(
            entry_bg, textvariable=self.bin_path,
            font=MONO, bg=BG2, fg=TEXT, insertbackground=TEXT,
            relief="flat", bd=6
        )
        self.file_entry.pack(fill="x")

        tk.Button(
            row1, text="  Datei wählen  ",
            font=SANS_B, bg=BG3, fg=TEXT,
            activebackground=BORDER, activeforeground=TEXT,
            relief="flat", bd=0, padx=12, pady=6,
            cursor="hand2", command=self._choose_file
        ).pack(side="left", padx=(8, 0))

        # Flash type radio
        type_row = tk.Frame(content, bg=BG)
        type_row.pack(fill="x", pady=(0, 4))
        tk.Label(type_row, text="Typ:", font=SANS, bg=BG, fg=TEXT_DIM).pack(side="left")
        for val, lbl, addr in [("merged", "merged.bin  →  0x0000", "0x0000"),
                                ("app",    "app .bin    →  0x10000", "0x10000")]:
            tk.Radiobutton(
                type_row, text=lbl, variable=self.flash_type, value=val,
                font=SANS, bg=BG, fg=TEXT, selectcolor=BG,
                activebackground=BG, activeforeground=TEXT,
                command=lambda a=addr: self.flash_addr.set(a)
            ).pack(side="left", padx=(12, 0))

        # ── Port & Baud ───────────────────────────────────────────────────────
        self._section(content, "VERBINDUNG")

        conn_row = tk.Frame(content, bg=BG)
        conn_row.pack(fill="x", pady=(4, 12))

        # Port
        tk.Label(conn_row, text="Port:", font=SANS, bg=BG, fg=TEXT_DIM).pack(side="left")
        self.port_cb = ttk.Combobox(
            conn_row, textvariable=self.port_var,
            font=MONO, width=14, state="readonly"
        )
        self.port_cb.pack(side="left", padx=(6, 0))
        self._style_combobox()

        tk.Button(
            conn_row, text="↻",
            font=("Segoe UI", 11), bg=BG3, fg=TEXT,
            activebackground=BORDER, activeforeground=TEXT,
            relief="flat", bd=0, padx=8, pady=4,
            cursor="hand2", command=self._refresh_ports
        ).pack(side="left", padx=(6, 24))

        # Baud
        tk.Label(conn_row, text="Baud:", font=SANS, bg=BG, fg=TEXT_DIM).pack(side="left")
        baud_cb = ttk.Combobox(
            conn_row, textvariable=self.baud_var,
            font=MONO, width=10,
            values=["115200", "230400", "460800", "921600"]
        )
        baud_cb.pack(side="left", padx=(6, 0))

        # Flash addr (manual override)
        tk.Label(conn_row, text="  Adresse:", font=SANS, bg=BG, fg=TEXT_DIM).pack(side="left")
        tk.Entry(
            conn_row, textvariable=self.flash_addr,
            font=MONO, bg=BG2, fg=YELLOW,
            insertbackground=TEXT, relief="flat", bd=4, width=10
        ).pack(side="left", padx=(6, 0))

        # ── Flash Button ──────────────────────────────────────────────────────
        btn_row = tk.Frame(self, bg=BG, pady=4)
        btn_row.pack(fill="x", padx=24)

        self.flash_btn = tk.Button(
            btn_row,
            text="  ⚡  FLASH STARTEN  ",
            font=("Segoe UI", 11, "bold"),
            bg=ACCENT, fg="#ffffff",
            activebackground=ACCENT_H, activeforeground="#ffffff",
            relief="flat", bd=0, padx=20, pady=10,
            cursor="hand2", command=self._start_flash
        )
        self.flash_btn.pack(side="left")

        self.status_label = tk.Label(
            btn_row, text="", font=SANS, bg=BG, fg=TEXT_DIM
        )
        self.status_label.pack(side="left", padx=16)

        # ── Log Output ────────────────────────────────────────────────────────
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x", pady=(8, 0))

        log_hdr = tk.Frame(self, bg=BG2, padx=24, pady=6)
        log_hdr.pack(fill="x")
        tk.Label(log_hdr, text="OUTPUT", font=("Segoe UI", 8, "bold"),
                 bg=BG2, fg=TEXT_DIM).pack(side="left")

        self.clear_btn = tk.Button(
            log_hdr, text="Löschen",
            font=("Segoe UI", 8), bg=BG2, fg=TEXT_DIM,
            activebackground=BG3, activeforeground=TEXT,
            relief="flat", bd=0, cursor="hand2",
            command=self._clear_log
        )
        self.clear_btn.pack(side="right")

        log_frame = tk.Frame(self, bg=BG, padx=24, pady=12)
        log_frame.pack(fill="both", expand=True)

        self.log = tk.Text(
            log_frame,
            font=MONO, bg=BG2, fg=TEXT,
            insertbackground=TEXT,
            relief="flat", bd=0,
            wrap="word",
            state="disabled"
        )
        self.log.pack(side="left", fill="both", expand=True)

        sb = tk.Scrollbar(log_frame, command=self.log.yview, bg=BG3)
        sb.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=sb.set)

        # Tag-Farben für Log
        self.log.tag_config("ok",    foreground="#3fb950")
        self.log.tag_config("err",   foreground=RED)
        self.log.tag_config("warn",  foreground=YELLOW)
        self.log.tag_config("info",  foreground="#58a6ff")
        self.log.tag_config("dim",   foreground=TEXT_DIM)
        self.log.tag_config("plain", foreground=TEXT)

    def _section(self, parent, title):
        f = tk.Frame(parent, bg=BG)
        f.pack(fill="x", pady=(8, 2))
        tk.Label(f, text=title, font=("Segoe UI", 8, "bold"),
                 bg=BG, fg=TEXT_DIM).pack(side="left")
        tk.Frame(f, bg=BORDER, height=1).pack(side="left", fill="x", expand=True, padx=(8, 0), pady=4)

    def _style_combobox(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox",
                        fieldbackground=BG2, background=BG3,
                        foreground=TEXT, bordercolor=BORDER,
                        arrowcolor=TEXT_DIM, selectbackground=BG3,
                        selectforeground=TEXT)

    # ── Actions ───────────────────────────────────────────────────────────────
    def _choose_file(self):
        path = filedialog.askopenfilename(
            title="Firmware .bin auswählen",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )
        if path:
            self.bin_path.set(path)
            # Auto-detect merged vs app
            if "merged" in os.path.basename(path).lower():
                self.flash_type.set("merged")
                self.flash_addr.set("0x0000")
            else:
                self.flash_type.set("app")
                self.flash_addr.set("0x10000")
            self._log(f"Datei: {path}\n", "info")

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb["values"] = ports
        if ports:
            self.port_var.set(ports[0])
            self._log(f"Ports gefunden: {', '.join(ports)}\n", "dim")
        else:
            self.port_var.set("")
            self._log("Keine seriellen Ports gefunden.\n", "warn")

    def _start_flash(self):
        bin_file = self.bin_path.get().strip()
        port     = self.port_var.get().strip()
        baud     = self.baud_var.get().strip()
        addr     = self.flash_addr.get().strip()

        if not bin_file:
            self._log("⚠ Keine Datei ausgewählt!\n", "err"); return
        if not port:
            self._log("⚠ Kein Port ausgewählt!\n", "err"); return
        if not os.path.isfile(bin_file):
            self._log(f"⚠ Datei nicht gefunden: {bin_file}\n", "err"); return

        self.flash_btn.configure(state="disabled", bg=BG3, text="  ⏳  Flashe...  ")
        self._set_status("Flashvorgang läuft...", YELLOW)
        self._log(f"\n{'─'*60}\n", "dim")
        self._log(f"Starte Flash\n", "info")
        self._log(f"  Datei : {bin_file}\n", "dim")
        self._log(f"  Port  : {port}  Baud: {baud}  Adresse: {addr}\n", "dim")
        self._log(f"{'─'*60}\n", "dim")

        cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "esp32",
            "--port", port,
            "--baud", baud,
            "write-flash", addr, bin_file
        ]

        threading.Thread(target=self._run_flash, args=(cmd,), daemon=True).start()

    def _run_flash(self, cmd):
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )
            for line in proc.stdout:
                self._log_auto(line)
            proc.wait()

            if proc.returncode == 0:
                self._log("\n✓ Flash erfolgreich!\n", "ok")
                self._set_status("✓ Erfolgreich", "#3fb950")
            else:
                self._log(f"\n✗ Fehler (exit code {proc.returncode})\n", "err")
                self._set_status("✗ Fehler", RED)

        except FileNotFoundError:
            self._log("✗ esptool nicht gefunden. Bitte: pip install esptool\n", "err")
            self._set_status("✗ esptool fehlt", RED)
        except Exception as e:
            self._log(f"✗ Fehler: {e}\n", "err")
            self._set_status("✗ Fehler", RED)
        finally:
            self.after(0, self._reset_button)

    def _reset_button(self):
        self.flash_btn.configure(
            state="normal", bg=ACCENT,
            text="  ⚡  FLASH STARTEN  "
        )

    def _log_auto(self, line):
        """Wählt automatisch die passende Farbe je nach Inhalt."""
        lo = line.lower()
        if any(w in lo for w in ["error", "fatal", "failed", "✗"]):
            tag = "err"
        elif any(w in lo for w in ["warning", "warn", "deprecated"]):
            tag = "warn"
        elif any(w in lo for w in ["writing", "wrote", "hash", "verified", "leaving", "hard reset"]):
            tag = "ok"
        elif any(w in lo for w in ["connecting", "chip", "features", "mac", "stub", "changing", "configured"]):
            tag = "info"
        else:
            tag = "plain"
        self._log(line, tag)

    def _log(self, text, tag="plain"):
        def _do():
            self.log.configure(state="normal")
            self.log.insert("end", text, tag)
            self.log.see("end")
            self.log.configure(state="disabled")
        self.after(0, _do)

    def _clear_log(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _set_status(self, msg, color):
        self.after(0, lambda: self.status_label.configure(text=msg, fg=color))


if __name__ == "__main__":
    app = FlashTool()
    app.mainloop()
