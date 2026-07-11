from pathlib import Path

p = Path(__file__).resolve().parent.parent / "docs" / "s3.html"
t = p.read_text(encoding="utf-8")

s3_wiring = """    <!-- WIRING -->
    <div id="view-wiring" class="view">
      <div class="card">
        <div class="card-title">Waveshare ESP32-S3-RS485 → PS0600 (Cummins C22D5)</div>
        <p class="small muted" style="margin-bottom:14px">
          Modbus RTU 8N1 • <b>Ενσωματωμένο απομονωμένο RS485</b> — χωρίς εξωτερικό module<br>
          Firmware pins: RX=<b>GPIO18</b>, TX=<b>GPIO17</b>, DE=<b>GPIO21</b><br>
          Τροφοδοσία: <b>7–36V DC</b> (ή USB-C για δοκιμή)
        </p>
      </div>
      <div class="grid-2">
        <div class="card">
          <div class="card-title">RS485 terminals → PS0600 TB15</div>
          <table class="wire-table">
            <thead><tr><th>Πλακέτα S3</th><th>TB15</th><th>Σημείωση</th></tr></thead>
            <tbody>
              <tr><td>A+</td><td>Pin 3 (A+)</td><td>Data +</td></tr>
              <tr><td>B−</td><td>Pin 4 (B−)</td><td>Data −</td></tr>
              <tr><td>GND</td><td>Pin 1 (Common)</td><td>Προαιρετικό</td></tr>
            </tbody>
          </table>
        </div>
        <div class="card">
          <div class="card-title">GPIO outputs</div>
          <table class="wire-table">
            <thead><tr><th>Pin</th><th>Χρήση</th></tr></thead>
            <tbody>
              <tr><td>GPIO1</td><td>Έξοδος / relay 1</td></tr>
              <tr><td>GPIO2</td><td>Έξοδος / relay 2</td></tr>
            </tbody>
          </table>
        </div>
      </div>
      <div class="card">
        <div class="card-title">Troubleshooting</div>
        <ul class="small muted" style="padding-left:18px;line-height:1.7">
          <li><code>modbus 40009</code>: έλεγξε A+/B− → Pin3/Pin4 (δοκίμασε swap A↔B)</li>
          <li>Modbus Enabled στο PS0600 • baud 9600 • slave 1</li>
          <li>120Ω termination jumper (μακρύ καλώδιο)</li>
        </ul>
      </div>
    </div>

"""

i = t.index("    <!-- WIRING -->")
j = t.index("    <!-- DIAGNOSTICS -->")
t = t[:i] + s3_wiring + t[j:]

repls = [
    ("ESP32 Classic — Energy Hub", "ESP32-S3 RS485 — Energy Hub"),
    ("ESP32 Classic", "ESP32-S3 RS485"),
    ('XY-485 · <a href="index.html"', 'Waveshare · <a href="index.html"'),
    ("esp32_classic_device_id", "esp32_s3_device_id"),
    ("BOARD_PAGE = 'esp32'", "BOARD_PAGE = 's3'"),
    ('placeholder="000401000000"', 'placeholder="1CDBD47A3C50"'),
    ("OTA κωδικός: esp32ota", "OTA: firmware-s3.bin · esp32ota · USB upload-s3.ps1"),
    ("BLE / JK BMS", "BLE / BMS"),
]
for a, b in repls:
    t = t.replace(a, b, 1)

p.write_text(t, encoding="utf-8")
print("patched", p, "len", len(t))
