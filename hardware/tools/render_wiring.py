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
text(50,92,'LOW-VOLTAGE CONNECTIONS · DEFAULT: ONE GPIO / FOUR STRIPS',17,'#87b8d4')
box(50,130,300,220);text(75,168,'LAMP ESP32',25)
text(75,211,'GPIO18 · LED data');text(75,251,'GPIO27 · servo signal',20,'#ffc678')
text(75,292,'5V input / GND');text(75,322,'Never connect 5 V to 3V3',16)
box(420,130,390,220);text(445,168,'SN74AHCT125 · 5 V',25)
text(445,211,'Pin 2 / 1A → pin 3 / 1Y');text(445,250,'Pin 1 / 1OE → GND')
text(445,287,'Pin 14 → +5V · pin 7 → GND');text(445,322,'100 nF across VCC and GND',17)
line('M350 205H420',arrow=True)
line('M810 205H830M865 205H880V230H940',arrow=True);parts.append('<rect x="830" y="198" width="35" height="14" fill="#102b40" stroke="#62dded" stroke-width="2"/>');text(833,183,'330 Ω',18)
text(942,148,'LED CHAIN · 4 × 30 PIXELS',21)
for y,label,direction in [(230,'TOP','left'),(320,'FRONT UPPER','right'),(410,'FRONT LOWER','left'),(500,'BOTTOM','right')]:
 box(940,y-30,400,60)
 text(1030,y+7,label,19)
 if direction=='left':
  text(950,y+7,'DIN',15);text(1290,y+7,'OUT',15);line(f'M985 {y+20}H1290',arrow=True)
 else:
  text(950,y+7,'OUT',15);text(1300,y+7,'DIN',15);line(f'M1290 {y+20}H985',arrow=True)
line('M1340 230H1360V320H1340',arrow=True)
line('M940 320H915V410H940',arrow=True)
line('M1340 410H1360V500H1340',arrow=True)
text(940,562,'Same viewing side for all four strips',17)
box(50,390,760,190);text(75,430,'DS3225 SERVO',24)
line('M350 245H382V370H790V390','#ffc678');text(75,469,'GPIO27 → signal (usually orange / yellow)',20,'#ffc678')
text(75,509,'+5V → servo power · GND → servo ground');text(75,548,'Confirm connector polarity on your exact servo.',17)
box(50,615,1290,205);text(75,658,'SHARED REGULATED 5 V SUPPLY',25)
line('M80 690H1305','#ff7a85');text(90,718,'+5V → each LED strip +5V, servo power, ESP32 board 5V input',21,'#ff9aa3')
line('M80 742H1305','#a6b6c4');text(90,774,'GND → each LED strip GND, servo GND, ESP32 GND, level shifter GND',20)
text(75,804,'Parallel power branches; never route LED or servo power through the ESP32.',17)
box(50,850,620,175);text(75,890,'OPTIONAL: FOUR DATA OUTPUTS',22)
text(75,925,'GPIO18 → top · GPIO19 → front upper',19);text(75,957,'GPIO23 → front lower · GPIO25 → bottom',19)
text(75,993,'Set LED_OUTPUTS = 4; all strips left → right.',17)
box(700,850,640,175);text(725,890,'CYD · SEPARATE USB POWER',22)
text(725,925,'CYD → Wi-Fi → lamp ESP32 → LED bus',20,'#62dded')
text(725,958,'No wired power connection to the lamp required.',17)
text(725,993,'4 A firmware budget estimates LEDs only.',17)
text(50,1055,'Add bulk capacitance near LEDs. Verify board power input and prevent USB / external 5 V backfeeding.',16)
parts.append('</g></svg>')
svg=''.join(parts)
(ROOT/'docs/wiring.svg').write_text(svg,encoding='utf-8')
(ROOT/'docs/wiring.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg))
