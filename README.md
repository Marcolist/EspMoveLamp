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

## Webeditor und Szenenbibliothek

Das CYD stellt einen lokalen Webeditor bereit, auch wenn die Lampe noch nicht verbunden ist:

1. Handy oder PC mit **MoveLamp-Setup** verbinden, Passwort **MoveLamp1234**.
2. **http://192.168.8.1/** im Browser öffnen. Eine Meldung „kein Internet“ ist in diesem WLAN normal.
3. Eine Szene auswählen oder **+ Neu** antippen. Name, Symbol, Kachelfarbe, Lichtstil, Helligkeit und Servofreigabe einstellen. Bei eigenen Seitenfarben lassen sich **Oben**, **Unten** und **Vorne** sowie ein Zielwinkel einstellen. Schwarz schaltet eine Seite aus; beide vorderen Streifen bilden zusammen „Vorne“.
4. **Szene speichern** speichert nur die Einstellungen. **Szene starten** aktiviert eine gespeicherte Szene bei verbundener Lampe.
5. Unter **Vier Lieblingsmomente** vier verschiedene Szenen auswählen und **Favoriten speichern** antippen. Ihre Reihenfolge entspricht oben links, oben rechts, unten links und unten rechts auf dem CYD.

Der Editor und alle Schrift-/Symbolressourcen liegen auf dem CYD; es werden kein Konto, Internetdienst oder CDN benötigt. Ist das CYD zusätzlich mit dem Lampen-WLAN oder dem konfigurierten Heimnetz verbunden, ist derselbe Editor auch über seine dortige IP erreichbar. Das Editor-WLAN nutzt absichtlich `192.168.8.0/24`, das Lampen-WLAN `192.168.4.0/24`.

Es gibt Platz für **4 bis 32 Szenen**, Namen bis **32 UTF-8-Bytes**, acht Symboltypen und beliebige 24-Bit-Seiten-/Kachelfarben. Szenen und Favoriten werden gemeinsam als ein geprüftes, versioniertes NVS-Blob gespeichert und bleiben über Neustarts sowie normale Firmware-Uploads erhalten. Ein fehlgeschlagener Speichervorgang ersetzt die aktuelle Bibliothek nicht. Gleichzeitige Änderungen aus mehreren Browsern werden über eine Versionsnummer erkannt; bei Konflikten neu laden. Die vorhandene Touchkalibrierung liegt in einem anderen NVS-Namensraum.

**Exportieren/Importieren** sichert oder lädt die ganze Bibliothek als JSON. Eine als Favorit verwendete Szene muss zuerst in den Favoriten ersetzt werden, bevor sie gelöscht werden kann. Änderungen an einer gerade aktiven Szene ändern das Licht erst nach erneutem Starten; die Aktivmarkierung erlischt bis dahin, wenn die gespeicherten Werte nicht mehr zum bestätigten Lampenzustand passen.

### Vier vorbereitete Szenen

| Szene / Symbol | Licht | Bewegung |
| --- | --- | --- |
| **Meeting / Kamera** | weißes Frontlicht, oben und unten aus, ca. 75 % Helligkeit | Servo aus |
| **Abend / Herz** | rotes Licht oben, die anderen Seiten aus, ca. 35 % | Servo aus |
| **Fokus / indirektes Licht** | vorläufig lavendelfarbenes Licht oben, sonst aus, ca. 35 % | feste Position am oberen freigegebenen Winkel |
| **Show / Funkeln** | bewegter Regenbogen/Farbverlauf, ca. 39 % | langsamer Schwenk innerhalb der freigegebenen Grenzen |

Diese Szenen werden nur bei einer fehlenden oder ungültigen Bibliothek als Vorgaben geladen; ein Firmware-Update überschreibt keine gültige eigene Bibliothek. Die gewünschte Farbstimmung und Helligkeit lassen sich im Editor ändern. Bei animierten Lichtstilen bestimmen die eingebauten Effekte die Farben und gegebenenfalls den Schwenkverlauf.

## Touch und Bedienung

Beim ersten Start die drei Kreuze nacheinander kurz halten und jeweils loslassen. Die Kalibrierung wird nur im CYD gespeichert. Für eine neue Kalibrierung **BOOT gedrückt halten und kurz RESET drücken**, BOOT nach Erscheinen der Kalibrierung loslassen. Die Touchkoordinaten werden mit einer affinen Abbildung berechnet, die auch vertauschte/gespiegelte Achsen erfasst.

Der Homescreen ist ohne Überschrift gestaltet: vier leuchtende, abgerundete Favoritentasten mit gefüllten, mehrfarbigen Symbolen und einer kleinen Navigations-/An-Aus-Leiste. **Ein heller Rahmen und ein Häkchen markieren ausschließlich die von der Lampe bestätigte aktive Szene.** Ein gelber Rahmen bedeutet „Befehl wartet auf Bestätigung“. Offline, nach einem Lampenneustart oder bei abweichenden Szenenwerten wird keine aktive Favoritentaste angezeigt. Die Identität einer Szene wird im Protokoll mitgeführt, damit auch gleich konfigurierte Szenen eindeutig bleiben.

Im Tab **Manuell** die Seite wählen, eine Farbe antippen und Winkel/Helligkeit einstellen. Erst **Anwenden** sendet die vorbereiteten Werte. Die Servo-Taste bestimmt, ob dieser manuelle Befehl den Servo freigibt. Ein manueller Befehl gehört zu keiner gespeicherten Szene. Die Editor-Adresse steht auf dieser Seite.

**AN/AUS:** AUS schaltet die LEDs sofort aus, fährt anschließend mit begrenzter Sollgeschwindigkeit zur Referenzposition und schaltet das Servosignal nach einer kurzen Beruhigungszeit ab. Die Taste kann einen noch unbestätigten Szenenbefehl ersetzen. Während der Referenzfahrt zeigt die Anzeige `...`; der Browser zeigt „Referenzfahrt“. AN stellt die letzte Lichtauswahl dieser CYD-Sitzung wieder her und gibt den Servo nicht automatisch frei. Alternativ eine Favoritentaste antippen, um ihre gespeicherten Licht- und Bewegungseinstellungen zu starten.

**Referenzposition:** Vorläufig ist `SERVO_REFERENCE_DEG = SERVO_HOME_DEG` in `include/config.h` gesetzt (90 Grad nominal). Diese Position müssen wir später am Aufbau festlegen. Es gibt keinen Endschalter oder Positionssensor: Die Referenz ist ein Servo-Sollwert, keine gemessene Homing-Position. Die Firmware hält die aktuellen engen Grenzen von **81–99 Grad nominal / 1400–1600 µs** ein. Die echte Richtung „nach oben“, der gewünschte Stellweg und die Referenz müssen vor einer Erweiterung dieser Grenzen am Aufbau geprüft werden.

Nach dem Einschalten der Lampenversorgung bleiben Licht und Servo aus; es erfolgt keine automatische Referenzfahrt und keine Wiederherstellung der Szene. Erst ein empfangener AUS-Befehl löst die Referenzfahrt aus. Eine neue Szene kann diese Fahrt ersetzen. Nach deren Abschluss ist das Haltemoment wegen des abgeschalteten PWM-Signals nicht sichergestellt. AUS arbeitet über WLAN und ist kein garantierter Not-Aus bei Verbindungsverlust.

Das Display ist eine Fernbedienung; die Lampe spielt die letzte bestätigte Szene bei WLAN-Ausfall oder ausgeschaltetem Display weiter. Während eines Ausfalls angetippte Szenen werden nicht vorgemerkt. Die Helligkeit ist ein Wunschwert; das unveränderte Strombudget kann die tatsächliche RGB-Helligkeit weiter begrenzen.

## DS3225 einstellen

Den DS3225 gibt es als **180°- und 270°-Variante**. Die rote Gehäusefarbe bestimmt die Variante nicht. Laut Datenblatt entspricht 500–2500 µs jeweils dem vollständigen Stellweg, mit 1500 µs als Mitte und 50–330 Hz PWM; die Firmware verwendet 50 Hz.

Am vorhandenen Servo steht **DS3225 MG**, die Winkelvariante ist noch ungeklärt. MG bezeichnet das Metallgetriebe und legt den Stellwinkel nicht fest. In `include/config.h` ist vorläufig `SERVO_RANGE_DEG = 180` gesetzt; freigegeben sind nur ±9° nominal um die Mitte, also **1400–1600 µs**. Bei einer tatsächlichen 270°-Variante entspricht das etwa ±13,5°. Die konfigurierte Geschwindigkeit beträgt 18°/s nominal und weicht bei falscher Winkelvariante ebenfalls ab.

Wenn die Variante feststeht, `SERVO_RANGE_DEG` auf 180 oder 270 setzen und anschließend die mechanisch zulässigen `SERVO_MIN_DEG`/`SERVO_MAX_DEG` festlegen. Für die Inbetriebnahme die engen Grenzen beibehalten; eine echte Winkelmessung am abgekoppelten Servo oder die Bestelldaten klären die Variante.

Den ersten Test mit abgekoppeltem Profil/Servohorn machen: Licht prüfen, dann Servo aktivieren und Mitte sowie Drehrichtung feststellen. Beim erstmaligen Aktivieren fährt ein gewöhnlicher Positionsservo zur angenommenen Mitte; seine echte Startposition ist dem ESP32 unbekannt, daher ist dieser erste Weg nicht durch die Softwaregeschwindigkeit begrenzbar. Erst danach Profil passend montieren und `SERVO_MIN_DEG`, `SERVO_MAX_DEG` und gegebenenfalls `SERVO_REVERSED` auf die Mechanik abstimmen. Die gemeldete `angle` ist ein Sollwert, keine gemessene Position. Leitungen brauchen genügend Spiel für den gesamten Stellweg.

Eigene Szenen werden in `include/scenes.h` ergänzt: Farben in `color()`, Position/Amplitude/Periode in `SCENES`. Die vier sichtbaren Favoriten werden im Webeditor ausgewählt; `sceneIcon()` zeichnet die zugehörigen Symbole direkt auf dem TFT. Die Vorgabebibliothek liegt in `include/library.h`. Die übrigen bestehenden Szenen bleiben im Lampencode verfügbar.

## Protokoll und Prüfung

UDP/JSON, **Version 3**. CYD und Lampe müssen beide mit dieser Version geflasht werden. `discover` sucht die Lampe, `status` meldet ihren Zustand. `set` enthält Boot-ID, Display-Sitzung, Sequenznummer, Szenen-/Effektnummer, Helligkeit, Servofreigabe, `selection` (ID der gespeicherten Szene, 0 für manuell/aus) sowie `manual`, `target` und genau drei 24-Bit-RGB-Werte in `colors`. Zielwinkel müssen endlich und innerhalb der konfigurierten Grenzen liegen. Status enthält diese Werte, den aktuellen Servo-Sollwert `angle` und `parking` für die Referenzfahrt. Szene 0 schaltet das Licht auch bei gesetztem `manual` aus. Die Lampe benötigt keine Kopie der Bibliothek: Das CYD schickt beim Start einer Szene ihre Werte.

Befehle werden höchstens viermal mit derselben Sequenznummer versandt. Duplikate werden bestätigt, ohne Effekte oder Referenzfahrten neu zu starten; ältere Nummern derselben Sitzung werden verworfen. Die Lampen-Boot-ID verhindert das Ausführen alter Befehle nach einem Neustart. Es ist eine Lampe mit einer CYD-Fernbedienung vorgesehen; mehrere Browser können deren Editor nutzen.

Web-API: `GET /api/library`, `PUT /api/library`, `GET /api/config`, `GET /api/status`, `POST /api/activate` mit `{ "id": 1 }` und `POST /api/power` mit `{ "on": false }`. Schreibanfragen benötigen JSON und den Header `X-MoveLamp: 1`. Der Browsereditor setzt ihn selbst. Die Bibliotheksrevision schützt vor dem Überschreiben zwischenzeitlicher Änderungen. Die API begrenzt die Bibliothek auf 32 Szenen, vier eindeutige Favoriten und 24 KB Anfragegröße; Eingaben werden vor dem Speichern geprüft.

Der gemeinsame Schlüssel dient zur Zuordnung im privaten WLAN. UDP-Pakete enthalten ihn im Klartext; das Protokoll bietet keine Verschlüsselung oder kryptografische Replay-Abwehr. Keine Portfreigabe ins Internet einrichten.

```bash
pio run -e lamp -e cyd -e cyd_ili9341
python3 tests/run.py
```

Die Hosttests verwenden dieselbe Szenen- und Protokolllogik wie die Firmware, einschließlich ArduinoJson. Sie prüfen Pixelzuordnung, Bewegungsgrenzen, Effekte, Stromschätzung, ungültige Pakete, manuelle Farben/Zielwinkel, Wiederholungen und Zählerüberlauf. Auf Unix laufen zusätzlich AddressSanitizer/UndefinedBehaviorSanitizer; unter Windows wird ohne diese Sanitizer getestet. Dazu werden `g++` und die durch den Lampenbuild installierten ArduinoJson-Header benötigt.

Stand 15.10.2026: `lamp` und `cyd` mit Webeditor, Szenenbibliothek, Favoriten, Protokollversion 3 und Referenzfahrt erfolgreich kompiliert. Hosttests unter Windows erfolgreich: Szenen-/Protokollprüfung, vollständige 32-Szenen-Bibliothek, Favoritenvalidierung, NVS-Speichern/Wiederladen mit simulierten Fehlern, Prüfsumme und Referenzfahrt einschließlich Zeitüberlauf. NVS-Tests verwenden einen Testadapter; dies ersetzt keinen Stromausfalltest am Gerät.

Die echte Weboberfläche wurde lokal im Browser mit dem produktiven JSON-Validator getestet: neue Szene speichern, Symbol ändern, Favorit zuordnen, nach Neuladen prüfen, Aktivmarkierung und Ablehnung doppelter Favoriten. Mobil- und Desktopansicht visuell geprüft. Dieser lokale Browser-Test simuliert die Lampenverbindung; er ist kein WLAN-/Servo-Hardwaretest.

Das ST7789-CYD ist durch einen Abziehtest COM8 zugeordnet; Uploads erfolgen ausschließlich über diesen expliziten Port mit 115200 Baud und anschließender Hash-Prüfung. Display und Touch funktionieren laut Benutzer. Der Lampen-ESP32 ist an COM11 erkannt, benötigt aber weiterhin einen manuellen Wechsel in den Downloadmodus (BOOT halten, EN/RESET kurz drücken, BOOT loslassen). Die neue Lampen-Firmware ist gebaut; ein erfolgreicher Lampen-Upload und der Hardwaretest von LEDs, Servo und Funkzusammenspiel stehen noch aus. Andere USB-Ports werden nicht automatisch gesucht oder geöffnet.

Hardwaretest nach dem Flashen beider Geräte: Editor-WLAN verbinden, Szenen und Favoriten speichern, CYD neu starten und Speicherung prüfen. Alle vier LED-Streifen sowie die gemeinsame Frontfläche kontrollieren. Servo zunächst ohne Last testen; Drehrichtung, Referenz und zulässigen Stellweg festlegen. Anschließend Szenenwechsel, Show, AUS-Referenzfahrt, Neustarts und WLAN-Unterbrechung prüfen. Ein erfolgreicher Build ersetzt diesen Test nicht.
