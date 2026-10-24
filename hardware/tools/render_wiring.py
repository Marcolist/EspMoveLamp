from pathlib import Path
from html import escape
import resvg_py
ROOT=Path(__file__).resolve().parents[1]
parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="1080" viewBox="0 0 1400 1080" role="img" aria-label="EspMoveLamp low voltage wiring and GPIO pinout"><rect width="1400" height="1080" fill="#091b2b"/><defs><marker id="a" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0 0L8 4L0 8Z" fill="#62dded"/></marker></defs><g font-family="Arial, sans-serif">']
def text(x,y,s,size=20,color='#dcecf7'):
 parts.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}">{escape(s)}</text>')
def box(x,y,w,h):
 parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="16" fill="#102b40" stroke="#31566f"/>')
def line(d,color='#62dded',arrow=False):
 parts.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="3"'+(' marker-end="url(#a)"' if arrow else '')+'/>')
text(50,57,'EspMoveLamp · pinout & wiring',34)
text(50,92,'LOW-VOLTAGE CONNECTIONS · FOUR GPIOs / FOUR STRIPS',17,'#87b8d4')
box(50,130,300,220);text(75,168,'LAMP ESP32',25)
text(75,205,'GPIO18 · top',19);text(75,234,'GPIO19 · front upper',19)
text(75,263,'GPIO23 · front lower',19);text(75,292,'GPIO25 · bottom',19)
text(75,337,'GPIO27 · servo signal',20,'#ffc678')
text(445,168,'DIRECT DATA CONNECTION',23)
text(445,337,'4 × 30 pixels · LED_OUTPUTS = 4',18)
for start,elbow,end in [(200,910,230),(229,890,320),(258,870,410),(287,850,500)]:
 line(f'M350 {start}H{elbow}V{end}H940',arrow=True)
text(942,148,'FOUR SEPARATE LED STRIPS',21)
for y,label in [(230,'TOP'),(320,'FRONT UPPER'),(410,'FRONT LOWER'),(500,'BOTTOM')]:
 box(940,y-30,400,60)
 text(1030,y+7,label,19)
 text(950,y+7,'DIN',15);text(1290,y+7,'OUT',15);line(f'M985 {y+20}H1290',arrow=True)
text(940,562,'Left → right · leave DOUT unconnected',17)
box(50,390,760,190);text(75,430,'DS3225 SERVO',24)
line('M350 335H382V370H790V390','#ffc678');text(75,469,'GPIO27 → signal (usually orange / yellow)',20,'#ffc678')
text(75,509,'+5V → servo power · GND → servo ground');text(75,548,'Confirm connector polarity on your exact servo.',17)
box(50,615,1290,205);text(75,658,'SHARED REGULATED 5 V SUPPLY',25)
line('M80 690H1305','#ff7a85');text(90,718,'+5V → each LED strip +5V, servo power, ESP32 board 5V input',21,'#ff9aa3')
line('M80 742H1305','#a6b6c4');text(90,774,'GND → each LED strip GND, servo GND, ESP32 GND',20)
text(75,804,'Parallel power branches; never route LED or servo power through the ESP32.',17)
box(50,850,620,175);text(75,890,'ACTIVE: FOUR DATA OUTPUTS',22)
text(75,925,'GPIO18 → top · GPIO19 → front upper',19);text(75,957,'GPIO23 → front lower · GPIO25 → bottom',19)
text(75,993,'One DIN per GPIO; no strip-to-strip data links.',17)
box(700,850,640,175);text(725,890,'CYD · SEPARATE USB POWER',22)
text(725,925,'CYD → Wi-Fi → lamp ESP32 → LED bus',20,'#62dded')
text(725,958,'No wired power connection to the lamp required.',17)
text(725,993,'4 A firmware budget estimates LEDs only.',17)
text(50,1055,'Add bulk capacitance near LEDs. Verify board power input and prevent USB / external 5 V backfeeding.',16)
parts.append('</g></svg>')
svg=''.join(parts)
(ROOT/'docs/wiring.svg').write_text(svg,encoding='utf-8')
(ROOT/'docs/wiring.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg))
