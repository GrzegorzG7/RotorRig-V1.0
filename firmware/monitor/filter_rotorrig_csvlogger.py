from platformio.public import DeviceMonitorFilterBase
import os
import re
from datetime import datetime

# Obsługa prefixu z monitor_filters=time:
# "17:18:12.035 > " oraz czasem samo "> "
_TS_PREFIX = re.compile(r"^\d{2}:\d{2}:\d{2}\.\d{3}\s*>\s*")

_FLOAT_RE = re.compile(r"""
^
[+-]?
(
    (?:\d+(?:\.\d*)?)   # 12 / 12. / 12.34
  | (?:\.\d+)           # .34
)
(?:[eE][+-]?\d+)?       # exponent
$
""", re.VERBOSE)

# Dokładnie wg src/csv.cpp (24 kolumn):
CSV_HEADER = [
    "t_ms",            # 0
    "test_id",         # 1
    "motor_id",        # 2
    "kv",              # 3
    "prop",            # 4
    "battery_s",       # 5
    "esc_fw",          # 6
    "pole_pairs",      # 7
    "step_id",         # 8
    "throttle_pct",    # 9
    "step_time_s",     # 10
    "is_steady",       # 11
    "eRPM",            # 12
    "RPM",             # 13
    "V_bus_V",         # 14
    "I_A",             # 15
    "P_in_W",          # 16
    "thrust_N",        # 17
    "thrust_g",        # 18
    "eff_g_per_W",     # 19
    "eff_N_per_W",     # 20
    "eff_g_per_A",     # 21
    "bdshot_err_pct",  # 22
    "notes",           # 23
]

def strip_prefix(line: str) -> str:
    line = _TS_PREFIX.sub("", line)
    if line.startswith(">"):
        line = line[1:].lstrip()
    return line

def is_int_token(s: str) -> bool:
    return bool(re.match(r"^[+-]?\d+$", s.strip()))

def is_float_or_special(s: str) -> bool:
    s = s.strip()
    if not s:
        return False
    sl = s.lower()
    if sl in ("nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity", "+infinity", "-infinity"):
        return True
    return bool(_FLOAT_RE.match(s))

class RotorRigCsvLogger(DeviceMonitorFilterBase):
    """
    - zapisuje tylko prawdziwe linie CSV do .csv (24 pola wg csv.cpp)
    - rotuje plik po markerach RX:
        OK LOG 1 -> start nowego pliku (z nagłówkiem)
        OK LOG 0 -> stop (flush+fsync)
    - opcjonalnie zapisuje RAW do .txt obok
    """

    NAME = "rotorrig_csvlogger"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # Konfiguracja przez zmienne środowiskowe (opcjonalnie):
        self.fields = int(os.getenv("ROTORRIG_CSV_FIELDS", "24"))
        self.delim = os.getenv("ROTORRIG_CSV_DELIM", ",")
        self.log_root = os.getenv("ROTORRIG_LOG_DIR", os.path.join(os.getcwd(), "logs", "rotorrig"))
        self.tag = os.getenv("ROTORRIG_TAG", "").strip()
        self.fsync_every = max(1, int(os.getenv("ROTORRIG_FSYNC_EVERY", "1")))
        self.write_raw = os.getenv("ROTORRIG_WRITE_RAW", "1").strip() != "0"
        self.write_header = os.getenv("ROTORRIG_WRITE_HEADER", "1").strip() != "0"

        self._rx_buf = ""
        self._csv_f = None
        self._raw_f = None
        self._csv_lines = 0
        self._session_idx = 0
        self._logging = False

        if self.write_raw:
            self._raw_open()

    def _today_dir(self) -> str:
        return os.path.join(self.log_root, datetime.now().strftime("%Y-%m-%d"))

    def _ensure_dir(self, p: str):
        os.makedirs(p, exist_ok=True)

    def _base_name(self) -> str:
        self._session_idx += 1
        t = datetime.now().strftime("%H%M%S")
        s = f"s{self._session_idx:03d}"
        if self.tag:
            return f"{t}_{self.tag}_{s}"
        return f"{t}_{s}"

    def _raw_open(self):
        self._ensure_dir(self._today_dir())
        raw_path = os.path.join(self._today_dir(), f"{datetime.now().strftime('%H%M%S')}_raw.txt")
        self._raw_f = open(raw_path, "a", encoding="utf-8", newline="\n", buffering=1)

    def _sync_file(self, f):
        try:
            f.flush()
            os.fsync(f.fileno())
        except Exception:
            pass

    def _write_csv_header(self):
        if not (self.write_header and self._csv_f):
            return

        header = CSV_HEADER
        if len(header) != self.fields:
            # fallback gdybyś kiedyś zmienił liczbę kolumn
            header = [f"col{i}" for i in range(self.fields)]

        self._csv_f.write(self.delim.join(header) + "\n")
        self._sync_file(self._csv_f)

    def _csv_start(self, reason: str):
        self._ensure_dir(self._today_dir())
        base = self._base_name()
        csv_path = os.path.join(self._today_dir(), f"{base}.csv")

        # jeśli coś było otwarte, zamykamy
        self._csv_stop(reason="rotate")

        self._csv_f = open(csv_path, "a", encoding="utf-8", newline="\n", buffering=1)
        self._csv_lines = 0
        self._logging = True

        # >>> nagłówek na początku każdego pliku
        self._write_csv_header()

        if self._raw_f:
            self._raw_f.write(
                f"\n### START_CSV {datetime.now().isoformat(timespec='seconds')} reason={reason} file={csv_path}\n"
            )
            self._sync_file(self._raw_f)

    def _csv_stop(self, reason: str):
        if self._raw_f:
            self._raw_f.write(f"### STOP_CSV  {datetime.now().isoformat(timespec='seconds')} reason={reason}\n")
            self._sync_file(self._raw_f)

        self._logging = False
        if self._csv_f:
            self._sync_file(self._csv_f)
            try:
                self._csv_f.close()
            except Exception:
                pass
            self._csv_f = None

    def _looks_like_csv(self, line: str) -> bool:
        """
        Dokładnie wg src/csv.cpp:
        24 kolumn:
        0 t_ms (int)
        1 test_id (str)
        2 motor_id (str)
        3 kv (int)
        4 prop (str)
        5 battery_s (int)
        6 esc_fw (str)
        7 pole_pairs (int)
        8 step_id (int)
        9 throttle_pct (float/NaN)
        10 step_time_s (float/NaN)
        11 is_steady (int)
        12 eRPM (int)
        13 RPM (int)
        14 V_bus_V (float/NaN)
        15 I_A (float/NaN)
        16 P_in_W (float/NaN)
        17 thrust_N (float/NaN)
        18 thrust_g (float/NaN)
        19 eff_g_per_W (float/NaN)
        20 eff_N_per_W (float/NaN)
        21 eff_g_per_A (float/NaN)
        22 bdshot_err_pct (float/NaN)
        23 notes (str)
        """
        line = strip_prefix(line).strip()
        if not line:
            return False

        parts = [p.strip() for p in line.split(self.delim)]
        if len(parts) != self.fields:
            return False

        # 0: t_ms
        if not is_int_token(parts[0]):
            return False

        # inty: 3,5,7,8,11,12,13
        for idx in (3, 5, 7, 8, 11, 12, 13):
            if not is_int_token(parts[idx]):
                return False

        # floaty: 9,10,14..22
        for idx in (9, 10, 14, 15, 16, 17, 18, 19, 20, 21, 22):
            tok = parts[idx]
            # firmware drukuje NaN; NA też tolerujemy awaryjnie
            if tok.upper() == "NA":
                continue
            if not is_float_or_special(tok):
                return False

        # stringi: 1,2,4,6,23 (nie mogą być puste; mogą być "NA")
        for idx in (1, 2, 4, 6, 23):
            if parts[idx] == "":
                return False

        return True

    def _write_csv(self, line: str):
        if not (self._logging and self._csv_f):
            return
        line = strip_prefix(line).strip()
        self._csv_f.write(line + "\n")
        self._csv_f.flush()
        self._csv_lines += 1
        if (self._csv_lines % self.fsync_every) == 0:
            self._sync_file(self._csv_f)

    def rx(self, text):
        # zapis RAW (wszystko)
        if self._raw_f:
            self._raw_f.write(text)
            self._sync_file(self._raw_f)

        # składamy linie
        self._rx_buf += text
        while "\n" in self._rx_buf:
            line, self._rx_buf = self._rx_buf.split("\n", 1)
            line = line.rstrip("\r")
            s = strip_prefix(line).strip()

            # markery logowania z firmware
            if s == "OK LOG 1":
                self._csv_start(reason="rx:OK LOG 1")
                continue
            if s == "OK LOG 0":
                self._csv_stop(reason="rx:OK LOG 0")
                continue

            # same CSV
            if self._looks_like_csv(line):
                self._write_csv(line)

        return text  # nie filtrujemy wyświetlania w monitorze
