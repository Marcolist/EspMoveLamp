# EspMoveLamp

Zwei ESP32-Firmwares für eine drehbare Lampe aus einem 2040-Aluprofil mit DS3225-Servo und vier WS2812-Streifen à 30 LEDs. Ein CYD-Touchdisplay wählt über WLAN eine Szene aus; der Lampen-ESP32 berechnet Licht und Bewegung lokal.

## Projekt und Hardware

| Datei | Zweck |
| --- | --- |
| `src/lamp/main.cpp` | 120 LEDs, Servo-PWM, WLAN-Empfänger |
| `src/cyd/main.cpp` | Touchoberfläche, Kalibrierung, WLAN-Sender |
| `include/config.h` | GPIOs, LED-Anordnung, Servogrenzen, Netzwerkzeiten |
| `include/secrets.example.h` | Vorlage für WLAN und gemeinsamen Geräteschlüssel |
| `include/scenes.h` | Gemeinsame Szenen, Farben und Bewegungen |
| `include/protocol.h` | Befehlsprüfung und Behandlung von Wiederholungen |

Die Displaykonfiguration stammt aus dem vorhandenen Projekt `/home/marco/NeuraLiquid/CYD/platformio.ini`: **ST7789**, 240 × 320, Querformat, XPT2046-Touch. Das ist hier die Umgebung `cyd`. Das alte Projekt verwendet TFT BGR und ausgeschaltete Invertierung; beides ist übernommen. Eine Umgebung `cyd_ili9341` ist für abweichende CYD-Varianten enthalten. Die beiden SPI-Controller sind getrennt: HSPI fürs TFT, VSPI für Touch.

Vorgesehen ist ein klassischer ESP32-WROOM/DevKit mit 4 MB Flash als Lampencontroller. Für S3/C3 müssen Board, GPIOs und PWM-Ansteuerung angepasst werden.

## Verdrahtung der Lampe

Die endgültige Datenverdrahtung ist noch offen. Die Voreinstellung ist **eine Datenkette mit 120 LEDs**. Die vier Streifen behalten ihre getrennte Stromversorgung parallel an 5 V und GND; nur das Datensignal läuft von DOUT zu DIN weiter.

| Anschluss | Lampen-ESP32 / Belegung |
| --- | --- |
| LED-Daten | GPIO18 → 5-V-Pegelwandler → 330 Ω → DIN des ersten Streifens |
| Servo-Signal | GPIO27 → Signalleitung DS3225 |
| Streifen 0 / LEDs 0–29 | oben |
| Streifen 1 / LEDs 30–59 | unten |
| Streifen 2 / LEDs 60–89 | vorne, erste Reihe |
| Streifen 3 / LEDs 90–119 | vorne, zweite Reihe |
| Masse | ESP32, LED-Netzteil und Servo-Netzteil gemeinsam |

```text
GPIO18 -> Pegelwandler -> 330 Ohm -> OBEN -> UNTEN -> VORNE 1 -> VORNE 2
                                     DOUT -> DIN jeweils zum naechsten Streifen
GPIO27 ----------------------------> Servo-Signal
5 V LED-Netzteil -------------------> +5 V an alle vier Streifen
Servo-Versorgung ------------------> Servo-Plus
GND --------------------------------> alle GND gemeinsam
```

Für vier einzelne Datenleitungen `LED_OUTPUTS = 4` einstellen. Dann jeweils DIN eines Streifens an GPIO18, 19, 23, 25 anschließen, in derselben Reihenfolge oben/unten/vorne/vorne. Jeder Ausgang benötigt einen eigenen Pegelwandlerkanal und Serienwiderstand. `REVERSE_STRIP` dreht die logische Pixelrichtung einzelner Streifen um, wenn sie entgegengesetzt montiert sind. Beim vorhandenen Aufbau bleiben `SECTIONS = 4`, `ROWS_PER_SECTION = 1`, `LEDS_PER_ROW = 30`.

LEDs und Servo direkt aus ausreichend dimensionierten Versorgungen speisen, nicht über USB oder den 5-V-Pin des ESP32-Boards. Als konservative Rechnung können klassische WS2812 bei vollem Weiß zusammen bis zu 7,2 A benötigen; Varianten unterscheiden sich. Die Software begrenzt die **geschätzte RGB-Stromaufnahme** standardmäßig auf 4 A und startet bei etwa 24 % Helligkeit. Das ist keine Strommessung und berücksichtigt weder Ruheströme noch Servo/Controller. Netzteil, Leitungen und Absicherung müssen zur tatsächlichen Hardware passen.

Für die LED-Daten eignet sich ein 74AHCT125 bei 5 V; etwa 1000 µF an der LED-Einspeisung puffern Lastwechsel. Anschlüsse kurz halten und mehrere Versorgungspunkte vorsehen. Siehe [Adafruit: NeoPixel-Anschlussregeln](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices).

Der DS3225 ist laut [Herstellerdatenblatt](https://dsservo.com/d_file/DS3225%20datasheet.pdf) für 5–6,8 V spezifiziert und kann bei 6,8 V etwa 2,3 A Blockierstrom aufnehmen. Für ihn eine Versorgung mit ausreichender Stromreserve verwenden; eine höhere Servospannung darf nicht auf die 5-V-LEDs gelangen. Die mechanische Lagerung sollte das Profil tragen, der Servo den Antrieb übernehmen.

## WLAN und Flashen

Mit PlatformIO, z. B. der PlatformIO-Erweiterung in VS Code:

1. `include/secrets.example.h` nach `include/secrets.h` kopieren.
2. Für das Heimnetz SSID und Passwort eintragen. Beide Firmwares mit denselben Werten bauen.
3. Gemeinsamen `LAMP_PAIRING_KEY` und das AP-Passwort bei Bedarf ändern.
4. Zuerst Lampen-ESP32, dann CYD flashen.

```bash
pio run -e lamp
pio run -e cyd

pio device list
pio run -e lamp -t upload --upload-port /dev/ttyUSB0
pio run -e cyd -t upload --upload-port /dev/ttyUSB1
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Die Ports sind Beispiele; tatsächliche Ports aus `pio device list` einsetzen. Falls das TFT mit ST7789 leer bleibt, die alternative Umgebung testen:

```bash
pio run -e cyd_ili9341 -t upload --upload-port /dev/ttyUSB1
```

Ohne eingetragene Heimnetz-SSID eröffnet die Lampe sofort das WLAN **EspMoveLamp**, Standardpasswort **EspMove1234**. Das CYD verbindet sich damit automatisch. Mit konfiguriertem Heimnetz versuchen beide zuerst dieses; nach etwa 25 Sekunden ohne Verbindung stellt die Lampe zusätzlich den AP bereit und das CYD wechselt auf diesen. Im AP-Betrieb hat die Lampe normalerweise `192.168.4.1`. Nach dem Wechsel auf den Lampen-AP bleibt das CYD dort bis zum Neustart.

Im Heimnetz müssen beide im gleichen IPv4-Subnetz sein. UDP 4210 zur Lampe und Antworten auf 4211 zum Display müssen erlaubt sein; Gastnetz-Isolierung verhindert die Verbindung. Die Lampe wird automatisch per Broadcast gefunden. Falls Broadcast blockiert ist, `LAMP_IP` in `config.h` auf eine feste/reservierte Lampen-IP setzen.

## Touch und Bedienung

Beim ersten Start die drei Kreuze nacheinander kurz halten und jeweils loslassen. Die Kalibrierung wird nur im CYD gespeichert. Für eine neue Kalibrierung **BOOT gedrückt halten und kurz RESET drücken**, BOOT nach Erscheinen der Kalibrierung loslassen. Die Touchkoordinaten werden mit einer affinen Abbildung berechnet, die auch vertauschte/gespiegelte Achsen erfasst.

Die Startseite trägt die Überschrift **Meine Lampe** und zeigt im Tab **Szenen** vier große Kacheln ohne Beschriftung: **Sonne = Warm, Buch = Lesen, Wellen = Ozean, Kerze = Kerzenlicht** (oben links/rechts, unten links/rechts). Ein Tippen aktiviert das Preset. Ein heller Rahmen und ein hellgrünes Symbol markieren das bestätigte Preset; gelb bedeutet, dass eine Bestätigung noch aussteht. Die obere Taste **AN/AUS** schaltet die Lampe. AUS schaltet auch das Servosignal ab und kann einen noch unbestätigten Befehl ersetzen. Beim Einschalten wird die zuletzt verwendete Lichtauswahl dieser CYD-Sitzung wiederhergestellt, der Servo bleibt zunächst aus.

Im Tab **Manuell** zuerst **Oben**, **Unten** oder **Vorne** auswählen, dann eine der acht Farben antippen. Die durchgestrichene schwarze Kachel schaltet diese Seite aus. Die beiden vorderen LED-Streifen erhalten dieselbe Farbe. Winkel in Ein-Grad-Schritten und Helligkeit in etwa zehn Prozentpunkten einstellen. **Anwenden** aktiviert das manuelle Licht und sendet die vorbereiteten Werte gemeinsam. Reines Bearbeiten oder Wechseln des Tabs sendet keine Befehle. Der Winkel bleibt auf die konfigurierten Grenzen beschränkt (vorläufig 81–99 Grad nominal, siehe unten). Die Helligkeit ist der gewünschte Wert; das Strombudget kann sie intern weiter begrenzen.

**Der Servo ist nach jedem Lampenneustart deaktiviert.** Im manuellen Tab `Servo aus` antippen und anschließend **Anwenden**, um ihn bewusst einzuschalten. Er fährt dann zum eingestellten Zielwinkel. Bei einem anschließenden Presetwechsel bleibt die bestätigte Servofreigabe erhalten; das Preset bestimmt Position und gegebenenfalls Schwenkbewegung. `Servo an` antippen und **Anwenden** schaltet das PWM-Signal wieder ab; damit ist das Haltemoment nicht sichergestellt. **AUS** oben schaltet Licht und Servosignal ab. Diese Taste arbeitet über WLAN und kann bei Verbindungsverlust keine Abschaltung garantieren.

| ID | Szene | Licht | Bewegung bei aktiviertem Servo |
| --- | --- | --- | --- |
| 0 | Aus | aus | aktuelle Sollposition halten |
| 1 | Warm | warme Front, gedimmtes Licht oben/unten | Mittelstellung |
| 2 | Lesen | helle Front, wenig Streulicht | 25 % des freigegebenen Weges |
| 3 | Sonnenbad | orangefarbener Verlauf | sanftes Schwenken, 24 s |
| 4 | Ozean | versetzte blaue Wellen | Schwenken, 18 s |
| 5 | Regenbogen | laufende Farben über vier Flächen | Schwenken, 30 s |
| 6 | Kerze | warmes Flackern | feste Position |
| 7 | Nacht | sehr dezentes Rot nur unten | 80 % des freigegebenen Weges |

Das Display ist eine Fernbedienung; die Lampe spielt die letzte bestätigte Szene bei WLAN-Ausfall oder ausgeschaltetem Display weiter. Es werden keine während eines Ausfalls angetippten Szenen vorgemerkt. Nach einem Lampenneustart bleiben Licht und Servo aus; es erfolgt kein automatisches Wiederherstellen der Szene.

## DS3225 einstellen

Den DS3225 gibt es als **180°- und 270°-Variante**. Die rote Gehäusefarbe bestimmt die Variante nicht. Laut Datenblatt entspricht 500–2500 µs jeweils dem vollständigen Stellweg, mit 1500 µs als Mitte und 50–330 Hz PWM; die Firmware verwendet 50 Hz.

Am vorhandenen Servo steht **DS3225 MG**, die Winkelvariante ist noch ungeklärt. MG bezeichnet das Metallgetriebe und legt den Stellwinkel nicht fest. In `include/config.h` ist vorläufig `SERVO_RANGE_DEG = 180` gesetzt; freigegeben sind nur ±9° nominal um die Mitte, also **1400–1600 µs**. Bei einer tatsächlichen 270°-Variante entspricht das etwa ±13,5°. Die konfigurierte Geschwindigkeit beträgt 18°/s nominal und weicht bei falscher Winkelvariante ebenfalls ab.

Wenn die Variante feststeht, `SERVO_RANGE_DEG` auf 180 oder 270 setzen und anschließend die mechanisch zulässigen `SERVO_MIN_DEG`/`SERVO_MAX_DEG` festlegen. Für die Inbetriebnahme die engen Grenzen beibehalten; eine echte Winkelmessung am abgekoppelten Servo oder die Bestelldaten klären die Variante.

Den ersten Test mit abgekoppeltem Profil/Servohorn machen: Licht prüfen, dann Servo aktivieren und Mitte sowie Drehrichtung feststellen. Beim erstmaligen Aktivieren fährt ein gewöhnlicher Positionsservo zur angenommenen Mitte; seine echte Startposition ist dem ESP32 unbekannt, daher ist dieser erste Weg nicht durch die Softwaregeschwindigkeit begrenzbar. Erst danach Profil passend montieren und `SERVO_MIN_DEG`, `SERVO_MAX_DEG` und gegebenenfalls `SERVO_REVERSED` auf die Mechanik abstimmen. Die gemeldete `angle` ist ein Sollwert, keine gemessene Position. Leitungen brauchen genügend Spiel für den gesamten Stellweg.

Eigene Szenen werden in `include/scenes.h` ergänzt: Farben in `color()`, Position/Amplitude/Periode in `SCENES`. Die vier sichtbaren Presets werden in `src/cyd/main.cpp` über `PRESETS` ausgewählt; `sceneIcon()` zeichnet die zugehörigen Symbole direkt auf dem TFT. Die übrigen bestehenden Szenen bleiben im Lampencode verfügbar.

## Protokoll und Prüfung

UDP/JSON, **Version 2**. CYD und Lampe müssen beide mit dieser Version geflasht werden. Das CYD sucht mit `discover`, die Lampe antwortet mit `status`. `set` enthält Boot-ID der Lampe, Sitzungs-ID des Displays, Sequenznummer, Szenennummer, Helligkeit und Servoaktivierung. Hinzu kommen `manual` (bool), `target` (endlicher Zielwinkel innerhalb der konfigurierten Grenzen) und `colors` (genau drei 24-Bit-RGB-Zahlen für oben/unten/vorne). Statusantworten enthalten dieselben manuellen Einstellungen; `angle` ist der aktuelle Servo-Sollwert während der Fahrt. Szene 0 schaltet das Licht auch bei gesetztem `manual` aus. Befehle werden höchstens viermal mit derselben Nummer versandt. Doppelte Pakete starten Szenen nicht erneut; ältere Nummern derselben Sitzung werden verworfen. Die Boot-ID verhindert die Ausführung alter Befehle nach einem Lampenneustart. Die Anwendung ist für eine Lampe und eine Fernbedienung ausgelegt.

Der gemeinsame Schlüssel dient zur Zuordnung im privaten WLAN. UDP-Pakete enthalten ihn im Klartext; das Protokoll bietet keine Verschlüsselung oder kryptografische Replay-Abwehr. Keine Portfreigabe ins Internet einrichten.

```bash
pio run -e lamp -e cyd -e cyd_ili9341
python3 tests/run.py
```

Die Hosttests verwenden dieselbe Szenen- und Protokolllogik wie die Firmware, einschließlich ArduinoJson. Sie prüfen Pixelzuordnung, Bewegungsgrenzen, Effekte, Stromschätzung, ungültige Pakete, manuelle Farben/Zielwinkel, Wiederholungen und Zählerüberlauf. Auf Unix laufen zusätzlich AddressSanitizer/UndefinedBehaviorSanitizer; unter Windows wird ohne diese Sanitizer getestet. Dazu werden `g++` und die durch den Lampenbuild installierten ArduinoJson-Header benötigt.

Stand 15.10.2026: `lamp` und `cyd` mit der neuen Oberfläche und Protokollversion 2 erfolgreich kompiliert; Hosttests unter Windows erfolgreich. Das ST7789-CYD wurde an dem durch Abziehen identifizierten COM8 geflasht und der geschriebene Inhalt per Hash verifiziert. Display und Touchkalibrierung der ersten Firmware wurden am Gerät bestätigt; die neue Oberfläche ist noch vom Benutzer zu prüfen. Der Lampen-ESP32 an COM11 benötigt für den ersten Flash einen manuellen Wechsel in den Downloadmodus. Licht, Servo und Zusammenspiel beider Geräte sind noch nicht am Aufbau geprüft.

Hardwaretest nach dem Flashen: Touch kalibrieren, WLAN-Status prüfen, alle vier LED-Flächen kontrollieren, Helligkeit ändern, Servo zunächst ohne Last testen, Szenenwechsel und Stopp prüfen, beide Geräte einzeln neu starten und WLAN-Unterbrechung testen. Ein erfolgreicher Build ersetzt diesen Test nicht.
