// TempMonitor - leest CPU/GPU-temperatuur en FPS en stuurt die via USB naar het ronde ESP32-scherm.
//
// Gebruik:
//   TempMonitor.exe                 automatisch de ESP32 zoeken (USB VID 303A)
//   TempMonitor.exe --port COM5     vaste COM-poort gebruiken
//   TempMonitor.exe --list          alle gevonden temperatuursensoren tonen

using System.Globalization;
using System.IO.Ports;
using System.Management;
using System.Text.RegularExpressions;
using LibreHardwareMonitor.Hardware;

const int IntervalMs = 1000;
const string EspressifVid = "VID_303A";

string? fixedPort = null;
bool listOnly = false;
for (int i = 0; i < args.Length; i++)
{
    if (args[i] == "--port" && i + 1 < args.Length) fixedPort = args[++i];
    else if (args[i] == "--list") listOnly = true;
}

var computer = new Computer { IsCpuEnabled = true, IsGpuEnabled = true };
computer.Open();
AppDomain.CurrentDomain.ProcessExit += (_, _) => computer.Close();

if (listOnly)
{
    UpdateAll(computer);
    foreach (var hw in AllHardware(computer))
        foreach (var s in hw.Sensors.Where(s => s.SensorType == SensorType.Temperature))
            Console.WriteLine($"[{hw.HardwareType}] {hw.Name} / {s.Name}: {s.Value:0.0} °C");
    return;
}

Log.Start();
Log.Write("TempMonitor gestart.");
var fpsMonitor = new FpsMonitor();
AppDomain.CurrentDomain.ProcessExit += (_, _) => fpsMonitor.Dispose();
SerialPort? port = null;
string? lastStatus = null;

while (true)
{
    UpdateAll(computer);
    float? cpu = ReadCpuTemp(computer);
    float? gpu = ReadGpuTemp(computer);
    float? fps = fpsMonitor.TakeFps();
    string line = $"CPU={Fmt(cpu)};GPU={Fmt(gpu)};FPS={(fps.HasValue ? fps.Value.ToString("0", CultureInfo.InvariantCulture) : "nan")}";

    try
    {
        port ??= OpenDisplayPort(fixedPort);
        if (port != null)
        {
            port.Write(line + "\n");
            Console.Write($"\r{port.PortName} -> {line}      ");
        }
        else if (lastStatus != "notfound")
        {
            Log.Write($"Scherm niet gevonden, blijf zoeken... ({line})");
            lastStatus = "notfound";
        }

        // Ontbrekende sensoren eenmalig melden
        string sensors = $"cpu:{cpu.HasValue} gpu:{gpu.HasValue}";
        if (port != null && lastStatus != sensors)
        {
            Log.Write($"Verstuurd: {line}" +
                (cpu.HasValue ? "" : "  (geen CPU-temperatuur: draait de app als administrator en is PawnIO geinstalleerd?)") +
                (gpu.HasValue ? "" : "  (geen GPU-temperatuur gevonden)"));
            lastStatus = sensors;
        }
    }
    catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException or TimeoutException)
    {
        Log.Write($"Verbinding verbroken: {ex.Message}");
        lastStatus = null;
        try { port?.Dispose(); } catch { }
        port = null;
    }

    Thread.Sleep(IntervalMs);
}

static string Fmt(float? v) => v.HasValue ? v.Value.ToString("0.0", CultureInfo.InvariantCulture) : "nan";

static IEnumerable<IHardware> AllHardware(Computer c) =>
    c.Hardware.SelectMany(h => h.SubHardware.Prepend(h));

static void UpdateAll(Computer c)
{
    foreach (var hw in c.Hardware)
    {
        hw.Update();
        foreach (var sub in hw.SubHardware) sub.Update();
    }
}

static float? PickTemp(IEnumerable<IHardware> hardware, string[] preferred)
{
    var temps = hardware
        .SelectMany(h => h.Sensors)
        .Where(s => s.SensorType == SensorType.Temperature && s.Value is > 0 and < 150)
        .ToList();

    foreach (var name in preferred)
    {
        var match = temps.FirstOrDefault(s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        if (match != null) return match.Value;
    }
    return temps.Count > 0 ? temps.Max(s => s.Value) : null;
}

static float? ReadCpuTemp(Computer c) => PickTemp(
    AllHardware(c).Where(h => h.HardwareType == HardwareType.Cpu),
    ["Core (Tctl/Tdie)", "CPU Package", "Core (Tdie)", "Core (Tctl)", "Core Average"]);

static float? ReadGpuTemp(Computer c)
{
    // Eerst een losse videokaart (NVIDIA/AMD), anders de iGPU
    foreach (var type in new[] { HardwareType.GpuNvidia, HardwareType.GpuAmd, HardwareType.GpuIntel })
    {
        var t = PickTemp(AllHardware(c).Where(h => h.HardwareType == type), ["GPU Core", "GPU Hot Spot"]);
        if (t.HasValue) return t;
    }
    return null;
}

static SerialPort? OpenDisplayPort(string? fixedPort)
{
    string? name = fixedPort ?? FindEspressifPort();
    if (name == null) return null;

    var port = new SerialPort(name, 115200)
    {
        NewLine = "\n",
        WriteTimeout = 500,
        // DTR/RTS uit laten: anders kan de ESP32-S3 resetten bij openen
        DtrEnable = false,
        RtsEnable = false,
    };
    port.Open();
    Log.Write($"Verbonden met scherm op {name}");
    return port;
}

static string? FindEspressifPort()
{
    using var searcher = new ManagementObjectSearcher(
        $"SELECT Name FROM Win32_PnPEntity WHERE PNPDeviceID LIKE '%{EspressifVid}%' AND Name LIKE '%(COM%'");
    foreach (var obj in searcher.Get())
    {
        var m = Regex.Match(obj["Name"]?.ToString() ?? "", @"\((COM\d+)\)");
        if (m.Success) return m.Groups[1].Value;
    }
    return null;
}
