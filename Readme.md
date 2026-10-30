# TempControl

Rond systeemmonitor-scherm voor de pc. Een **Waveshare ESP32-S3-Touch-LCD-1.46** toont **CPU-temperatuur, GPU-temperatuur en FPS**, die via **USB-C** van Windows binnenkomen.

- **Boven 50 °C kleuren de waarde en de gloeiende ring rood**
- **Op of onder 50 °C blijven ze blauw**
- Bovenaan staan `QPRINTS //` en `SYSTEM MONITOR`. Krijgt het scherm geen data, dan staat daar `WACHTEN OP PC`.

```
firmware/          ESP32-S3 firmware (PlatformIO + Arduino_GFX)
pc-app/            Windows-app (.NET 10 + LibreHardwareMonitor + PresentMon)
pc-app/installer/  Inno Setup-installer
```

## 1. Firmware op het scherm zetten

1. Installeer **Visual Studio Code** en daarin de extensie **PlatformIO IDE**.
2. Open in VS Code de map `firmware/` (*File → Open Folder*).
3. Sluit het scherm met een USB-C-datakabel aan op de pc.
4. Klik onderaan in de blauwe balk op **→ (Upload)**. De eerste keer downloadt PlatformIO de ESP32-toolchain, dat duurt enkele minuten.
5. Op het scherm verschijnt `WACHTEN OP PC`.

> Lukt het uploaden niet? Hou de **BOOT**-knop ingedrukt, druk kort op **RESET**, laat BOOT los en upload opnieuw.

## 2. Windows-app installeren

Voer `TempMonitor-Setup-1.0.0.exe` uit. De installer:

- installeert de app in `C:\Program Files\TempMonitor`; .NET zit erin, dus dat hoef je niet apart te installeren
- installeert desgewenst de **PawnIO**-driver (vinkje). Die is nodig om de CPU-temperatuur van een Ryzen te lezen.
- zorgt dat de app onzichtbaar en met beheerdersrechten start bij elke aanmelding
- start de app meteen

Verwijderen gaat via *Instellingen → Apps → TempMonitor*.

**Werkt er iets niet?** Kijk in het logbestand `C:\ProgramData\TempMonitor\TempMonitor.log`. Daar staat of het scherm gevonden is en welke sensoren ontbreken.

### Installer zelf bouwen

Vereisten: [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0) en Inno Setup 6 (`winget install JRSoftware.InnoSetup`).

```powershell
cd pc-app
powershell -ExecutionPolicy Bypass -File .\build-installer.ps1
```

Het resultaat komt in `dist\TempMonitor-Setup-<versie>.exe`. De versie haalt het script uit `TempMonitor.csproj`. Met `-OutputDir E:\` komt de setup op een andere plek terecht, bijvoorbeeld rechtstreeks op een USB-stick.

> **Verkenner loopt vast of de setup is "geblokkeerd"?** Op beheerde pc's met Microsoft Defender for Endpoint blokkeert de ASR-regel *"Block executable files from running unless they meet a prevalence, age, or trusted list criterion"* (`01443614-cd74-433a-b99e-2ecdc07bfc25`) zelfgebouwde, niet-ondertekende exe-bestanden. Installeer dan op een pc zonder die regel, of voeg een ASR-uitzondering of een code-signing-certificaat toe.

### Ontwikkelen / testen zonder installer

In een terminal **als administrator**, in de map `pc-app/`:

```powershell
dotnet run -- --list        # toont alle temperatuursensoren
dotnet run                  # zoekt het scherm automatisch (USB VID 303A) en stuurt elke seconde data
dotnet run -- --port COM5   # als automatisch zoeken niet lukt
```

Voor FPS moet `PresentMon.exe` naast `TempMonitor.exe` staan. Zonder dat bestand toont het scherm `--` bij FPS.

## Hoe de FPS gemeten wordt

De app start [PresentMon](https://github.com/GameTechDev/PresentMon) (Intel, open source) op de achtergrond en telt elke seconde de frames van het venster **op de voorgrond**. Speel je een game, dan zie je dus de FPS van die game. Tekent het voorgrondvenster niets (bijvoorbeeld het bureaublad), dan staat er `--`.

## Protocol

De pc stuurt elke seconde één tekstregel (115200 baud):

```
CPU=42.5;GPU=48.0;FPS=165
```

Een onbekende waarde wordt `nan`. Stuur je `PING`, dan antwoordt het scherm met `TEMPMON`.

## Aanpassen

Bovenaan `firmware/src/main.cpp`:

| Instelling | Standaard | Betekenis |
|---|---|---|
| `BRAND` | `"QPRINTS"` | merknaam bovenaan |
| `HOT_THRESHOLD` | `50.0` | grens tussen blauw en rood |
| `ROTATION` | `0` | beeld draaien (0–3) als de USB-C-poort anders ligt |
| `DATA_TIMEOUT_MS` | `3000` | na zoveel ms zonder data toont het scherm "verbinding verloren" |
