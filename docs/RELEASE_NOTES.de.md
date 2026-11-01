**Vorabversion.** Diese Firmware wurde in GitHub Actions gebaut. Die W5500-Hardwaretests sind noch nicht abgeschlossen (siehe `docs/TESTPLAN.de.md`). Ein erfolgreicher Build ist kein Nachweis für eine funktionierende Ethernet-Anbindung.

- Basis: ESP-Miner v2.15.4, ESP-IDF v6.0.2, Ziel ESP32-S3
- Geeignet nur für **Bitaxe Gamma 601**
- Ethernet ist standardmäßig aus (AxeOS → LAN / Ethernet)

## Installation

1. **Original sichern:** `esptool.py --chip esp32s3 -p <PORT> -b 460800 read_flash 0x0 0x1000000 backup.bin`
2. Empfohlen: `esp-miner.bin` über AxeOS → Update hochladen (behält Einstellungen)
3. Alternativ seriell: `esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 esp-miner-factory-601-lan-<Version>.bin`

Details, Verdrahtung, Wiederherstellung und bekannte Einschränkungen stehen in der `README.de.md`. Wichtig: Spannungsversorgung deines W5500-Moduls vor dem Anschluss prüfen.
