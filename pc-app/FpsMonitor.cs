using System.Diagnostics;
using System.Runtime.InteropServices;

// Meet de FPS van het venster op de voorgrond (je game) met PresentMon (Intel, open source).
// PresentMon schrijft per frame een CSV-regel; wij tellen de regels per seconde.
sealed class FpsMonitor : IDisposable
{
    const string SessionName = "TempMonitor";
    const int MaxLoggedErrors = 20;

    readonly Process? _process;
    readonly object _lock = new();
    readonly Dictionary<(int Pid, string SwapChain), int> _frames = new();
    readonly Stopwatch _window = Stopwatch.StartNew();
    int _pidCol = -1, _swapChainCol = -1;
    int _loggedErrors;

    public FpsMonitor()
    {
        string exe = Path.Combine(AppContext.BaseDirectory, "PresentMon.exe");
        if (!File.Exists(exe))
        {
            Log.Write("PresentMon.exe niet gevonden: FPS wordt niet gemeten.");
            return;
        }

        var psi = new ProcessStartInfo(exe,
            $"--output_stdout --no_console_stats --stop_existing_session --session_name {SessionName} " +
            "--no_track_display --no_track_gpu --no_track_input")
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };

        try
        {
            _process = Process.Start(psi)!;
            _process.OutputDataReceived += (_, e) => OnCsvLine(e.Data);
            _process.ErrorDataReceived += (_, e) =>
            {
                if (!string.IsNullOrWhiteSpace(e.Data) && Interlocked.Increment(ref _loggedErrors) <= MaxLoggedErrors)
                    Log.Write($"PresentMon: {e.Data}");
            };
            _process.EnableRaisingEvents = true;
            _process.Exited += (_, _) => Log.Write("PresentMon is gestopt: FPS wordt niet meer gemeten.");
            _process.BeginOutputReadLine();
            _process.BeginErrorReadLine();
            Log.Write("PresentMon gestart voor FPS-meting.");
        }
        catch (Exception ex) when (ex is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
            Log.Write($"PresentMon kon niet starten: {ex.Message}");
            _process = null;
        }
    }

    // FPS van het voorgrondvenster sinds de vorige aanroep, of null als dat venster geen frames tekent.
    public float? TakeFps()
    {
        int foreground = ForegroundProcessId();
        lock (_lock)
        {
            double seconds = _window.Elapsed.TotalSeconds;
            _window.Restart();

            int best = 0;
            foreach (var (key, count) in _frames)
                if (key.Pid == foreground && count > best) best = count;
            _frames.Clear();

            return best > 0 && seconds > 0 ? (float)(best / seconds) : null;
        }
    }

    void OnCsvLine(string? line)
    {
        if (string.IsNullOrEmpty(line)) return;
        var cols = line.Split(',');

        lock (_lock)
        {
            if (_pidCol < 0)
            {
                _pidCol = Array.IndexOf(cols, "ProcessID");
                _swapChainCol = Array.IndexOf(cols, "SwapChainAddress");
                return;
            }
            if (cols.Length <= Math.Max(_pidCol, _swapChainCol) || !int.TryParse(cols[_pidCol], out int pid))
                return;

            var key = (pid, _swapChainCol >= 0 ? cols[_swapChainCol] : "");
            _frames[key] = _frames.GetValueOrDefault(key) + 1;
        }
    }

    static int ForegroundProcessId()
    {
        GetWindowThreadProcessId(GetForegroundWindow(), out uint pid);
        return (int)pid;
    }

    public void Dispose()
    {
        try { _process?.Kill(entireProcessTree: true); } catch (InvalidOperationException) { }
        _process?.Dispose();
    }

    [DllImport("user32.dll")]
    static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
}
