"""Extract Cummins fault bitmap table from PDF into JSON for dashboard decode."""
import json
import re
import sys
from pathlib import Path

try:
    import pypdf
except ImportError:
    sys.exit("pip install pypdf")

ROOT = Path(__file__).resolve().parents[1]
PDF = ROOT / "MODBUS REGISTER MAP.pdf"
OUT = ROOT / "docs" / "cummins_fault_bits.json"

NFPA_BITS = {
    15: "Common Alarm", 14: "Genset Supplying Load", 13: "Genset Running", 12: "Not in Auto",
    11: "High Battery Voltage", 10: "Low Battery Voltage", 9: "Charger AC Failure", 8: "Fail to Start",
    7: "Low Coolant Temperature", 6: "Pre-high Engine Temperature", 5: "High Engine Temperature",
    4: "Pre-low Oil Pressure", 3: "Low Oil Pressure", 2: "Overspeed", 1: "Low Coolant Level", 0: "Low Fuel Level",
}
EXT_BITS = {
    15: "Check Genset", 14: "Ground Fault", 13: "High AC Voltage", 12: "Low AC Voltage",
    11: "Under Frequency", 10: "Overload", 9: "Overcurrent", 8: "Short Circuit", 7: "Reverse kW",
    6: "Reverse kVAR", 5: "Fail to Sync", 4: "Fail to Close", 3: "Load Demand",
    2: "Genset Circuit Breaker Tripped", 1: "Utility Circuit Breaker Tripped", 0: "Emergency Stop",
}


def main():
    r = pypdf.PdfReader(str(PDF))
    text = "".join((p.extract_text() or "") for p in r.pages[430:478])
    text = re.sub(r"\s+", " ", text)

    faults = []
    for m in re.finditer(
        r"(404\d{2})\s+(\d{1,2})\s+Fault Status Bitmap \d+\s+(\d+)\s+(.+?)\s+(Shutdown|Warning|Derate|None|Alarm|Lamp)",
        text,
    ):
        reg, bit, code, name, resp = m.groups()
        bit = int(bit)
        if bit > 15:
            continue
        name = name.strip()[:80]
        faults.append({"reg": int(reg), "bit": bit, "code": int(code), "name": name, "resp": resp})

    by_reg = {}
    for f in faults:
        by_reg.setdefault(str(f["reg"]), []).append(f)

    out = {"nfpa": NFPA_BITS, "ext": EXT_BITS, "fault_bitmap": by_reg}
    OUT.write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"Wrote {OUT} ({len(faults)} fault bits, {len(by_reg)} registers)")


if __name__ == "__main__":
    main()
