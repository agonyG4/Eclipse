#!/usr/bin/env python3
"""Generate stroke-weight variants for the open-source Lucide SVG icon family.

These use Lucide geometry and are never Apple SF Symbols.
The original Lucide width (2) stays untouched; each weight lives in weights/<name>.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
WEIGHTS = {
    "ultralight": "0.6", "thin": "1", "light": "1.4",
    "regular": "2", "medium": "2.15", "semibold": "2.5",
    "bold": "2.85", "heavy": "3.2", "black": "3.6",
}

def main():
    total = 0
    for directory in (ROOT / "Settings/assets/icons/navigation/lucide",
                      ROOT / "Bar/assets/icons/lucide"):
        bases = sorted(directory.glob("*.svg"))
        if not bases:
            raise SystemExit(f"No source SVGs in {directory}")
        for base in bases:
            source = base.read_text()
            if source.count('stroke-width="2"') != 1:
                raise SystemExit(f"Expected unmodified Lucide SVG with one original stroke width: {base}")
            for name, width in WEIGHTS.items():
                output = directory / "weights" / name / base.name
                content = source.replace('stroke-width="2"', f'stroke-width="{width}"')
                ET.fromstring(content)
                output.parent.mkdir(parents=True, exist_ok=True)
                if not output.exists() or output.read_text() != content:
                    output.write_text(content)
                total += 1
    print(f"Generated {total} SVG weight variants; original stroke width 2 preserved.")

if __name__ == "__main__":
    main()
