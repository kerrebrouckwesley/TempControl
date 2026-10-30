// Logbestand in C:\ProgramData\TempMonitor, want de geïnstalleerde app heeft geen venster.
static class Log
{
    static readonly string Path = System.IO.Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "TempMonitor", "TempMonitor.log");

    public static void Start()
    {
        try
        {
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);
            File.WriteAllText(Path, "");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
    }

    public static void Write(string message)
    {
        string entry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss}  {message}";
        Console.WriteLine("\n" + entry);
        try { File.AppendAllText(Path, entry + Environment.NewLine); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
    }
}
