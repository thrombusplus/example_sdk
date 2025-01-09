using System;
using System.Threading.Tasks;
using LAM_SDK;

class Program
{
    static async Task Main()
    {
        // 1. Discover devices via mDNS
        var foundDevices = await LAM.DiscoverDevicesAsync("_imu._udp.local.");
        if (foundDevices.Count == 0)
        {
            Console.WriteLine("No devices found. Exiting...");
            return;
        }

        // 2. Pick the first discovered device
        var device = foundDevices[0];
        Console.WriteLine($"Connecting to {device.Hostname} ({device.IPAddress}:{device.Port})");

        // 3. Create the LAM object
        var lam = new LAM();

        // Subscribe to events
        lam.OnDataReceived += (data) =>
        {
            Console.WriteLine($"[IMU] A=({data.AccelX:F2},{data.AccelY:F2},{data.AccelZ:F2}), " +
                              $"G=({data.GyroX:F2},{data.GyroY:F2},{data.GyroZ:F2}), " +
                              $"Timestamp={data.TimeStamp}");
        };

        lam.OnStatusUpdated += (json) =>
        {
            Console.WriteLine("[Status JSON] " + json);
            // If lam.Status != null, we can read structured fields
            if (lam.Status != null)
            {
                Console.WriteLine($"Streaming={lam.Status.Streaming}, Rate={lam.Status.SamplingRate}");
            }
        };

        // 4. Initialize & Start Listening
        lam.Initialize(device.IPAddress, device.Port);
        lam.StartListening();

        // 5. Optionally send commands
        lam.GetStatus();
        lam.SetSamplingRate(50);
        lam.StartStreaming();

        Console.WriteLine("Press ENTER to stop...");
        Console.ReadLine();

        // 6. Cleanup
        lam.StopStreaming();
        lam.Dispose();
    }
}
