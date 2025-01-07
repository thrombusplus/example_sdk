using System;
using System.Linq;
using System.Text;
using System.Text.Json;             // For JSON deserialization
using System.Threading.Tasks;
using System.Collections.Generic;
using Zeroconf;                    // Zeroconf library
using LAM_SDK.Core;
using LAM_SDK.Models;
using System.Diagnostics;

namespace LAM_SDK
{
    /// <summary>
    /// Represents a discovered device via mDNS.
    /// </summary>
    public class LAMDiscoveredDevice
    {
        public string Hostname { get; set; }
        public string IPAddress { get; set; }
        public int Port { get; set; }
    }

    public class LAM : IDisposable
    {
        private readonly UdpClientWrapper _udpClient;
        private bool _isListening;
        private Timer _heartbeatTimer;   // For sending periodic heartbeats
        private long _lastStatusUpdate;  // Timestamp of last status update

        /// <summary>
        /// Current raw JSON status from the LAM device.
        /// </summary>
        public string StatusJson { get; private set; }

        /// <summary>
        /// Structured representation of the device status.
        /// </summary>
        public LAMStatus Status { get; private set; }

        /// <summary>
        /// Event triggered when a new status JSON is received.
        /// </summary>
        public event Action<string> OnLAMStatusUpdated;

        /// <summary>
        /// Event triggered when a full IMU frame (10 floats) is parsed.
        /// </summary>
        public event Action<LAMData> OnLAMDataReceived;

        // Heartbeat interval in milliseconds (2 seconds)
        private const int HEARTBEAT_INTERVAL_MS = 2000;

        public LAM()
        {
            _udpClient = new UdpClientWrapper();
            _udpClient.DataReceived += HandleDataReceived;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  mDNS / Zeroconf Discovery
        // ─────────────────────────────────────────────────────────────────────────────

        /// <summary>
        /// Discovers ESP32 devices that advertise a service type, default "_imu._udp.local.".
        /// Returns a list of discovered devices, each with Hostname, IP, and Port.
        /// </summary>
        /// <param name="serviceType">The service to query, e.g. "_imu._udp.local."</param>
        /// <returns>List of discovered LAMDiscoveredDevice objects.</returns>
        public static async Task<IList<LAMDiscoveredDevice>> DiscoverDevicesAsync(string serviceType = "_imu._udp.local.")
        {
            var devices = new List<LAMDiscoveredDevice>();

            Console.WriteLine($"[mDNS] Discovering service: {serviceType}");
            var responses = await ZeroconfResolver.ResolveAsync(serviceType);

            if (!responses.Any())
            {
                Console.WriteLine("[mDNS] No devices found.");
                return devices;
            }

            foreach (var resp in responses)
            {
                // Each response can have multiple services, typically 1 in this case
                foreach (var svc in resp.Services.Values)
                {
                    var device = new LAMDiscoveredDevice
                    {
                        Hostname = resp.DisplayName,
                        IPAddress = resp.IPAddress,
                        Port = svc.Port
                    };
                    devices.Add(device);
                }
            }

            Console.WriteLine($"[mDNS] Found {devices.Count} device(s).");
            return devices;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Connection Management
        // ─────────────────────────────────────────────────────────────────────────────

        /// <summary>
        /// Initialize the UDP connection to the ESP32.
        /// remoteIp = IP of the ESP32 (printed in its serial monitor or discovered via mDNS).
        /// remotePort = 5000 (per your ESP32 code).
        /// localPort = optional; if 0, OS picks a free port.
        /// </summary>
        public void Initialize(string remoteIp, int remotePort, int localPort = 0)
        {

            _udpClient.Initialize(remoteIp, remotePort, localPort);

            // 2. Start listening
            StartListening();

            // 3. Immediately send a heartbeat (includes the GetStatus command)
            SendHeartbeat();

            // 5. Start a periodic heartbeat timer (every 2s)
            _heartbeatTimer?.Dispose(); // just in case
            _heartbeatTimer = new Timer(HeartbeatCallback, null, HEARTBEAT_INTERVAL_MS, HEARTBEAT_INTERVAL_MS);

        }

        /// <summary>
        /// Send a "ping" command (heartbeat).
        /// </summary>
        public void SendHeartbeat()
        {
            SendCommand("ping");
            GetStatus();
        }

        /// <summary>
        /// Send a "ping" command (heartbeat).
        /// </summary>
        public void HeartbeatCallback(Object? state)
        {
            //Debug.Print($"Checking last status update {DateTimeOffset.UtcNow.ToUnixTimeSeconds()} - {_lastStatusUpdate}");
            SendHeartbeat(); // which calls SendCommand("ping") and GetStatus()

            if (Status != null)
            {
                Status.Connected = false;
                Status.Streaming = false;
            }

            // If last status update was > 4s ago, mark as disconnected
            if (_lastStatusUpdate>0 && DateTimeOffset.UtcNow.ToUnixTimeSeconds() - _lastStatusUpdate > 4)
            {
                OnLAMStatusUpdated?.Invoke("Disconnected");
                StopListening();
            }
        }

        /// <summary>
        /// Start listening for incoming data asynchronously.
        /// </summary>
        public void StartListening()
        {
            if (_isListening) return;
            _isListening = true;
            _udpClient.StartListeningAsync();
        }

        /// <summary>
        /// Stop listening for data.
        /// </summary>
        public void StopListening()
        {
            _isListening = false;
            _udpClient.StopListening();

            // Stop the heartbeat timer
            _heartbeatTimer?.Dispose();
            _heartbeatTimer = null;
            _lastStatusUpdate = 0;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Command Methods
        // ─────────────────────────────────────────────────────────────────────────────

        public void SendInitialize() => SendCommand("initialize"); // does not have any effect on the ESP32 so its just for demonstration purposes
        public void Connect(string portArg) => SendCommand($"connect:{portArg}");
        public void SetSamplingRate(int rate) => SendCommand($"setSamplingRate:{rate}");
        //public void EnableAxes(bool x, bool y, bool z)
        //    => SendCommand($"enableAxes:{(x ? 1 : 0)},{(y ? 1 : 0)},{(z ? 1 : 0)}");
        //public void SetRange(int accelRange, int gyroRange)
        //    => SendCommand($"setRange:{accelRange},{gyroRange}");
        public void StartStreaming() => SendCommand("startStreaming");
        public void StopStreaming() => SendCommand("stopStreaming");
        //public void CalibrateGyroscope() => SendCommand("calibrateGyroscope");
        //public void CalibrateAccelerometer() => SendCommand("calibrateAccelerometer");
        public void GetStatus() => SendCommand("getStatus");
        public void ResetIMU() => SendCommand("reset");
        public void Disconnect() => SendCommand("disconnect");

        // ─────────────────────────────────────────────────────────────────────────────
        //  Internal Logic
        // ─────────────────────────────────────────────────────────────────────────────

        private void SendCommand(string cmd)
        {
            var bytes = Encoding.UTF8.GetBytes(cmd);
            _udpClient.Send(bytes);
        }

        /// <summary>
        /// Parses incoming data from the ESP32.
        /// If exactly 40 bytes, treat as IMU data (10 floats).
        /// Otherwise, treat it as JSON status or text response.
        /// </summary>
        private void HandleDataReceived(byte[] data)
        {
            // 10 floats => 40 bytes, includes timestamp
            if (data.Length == 40)
            {
                // Parse IMU data
                var imu = ParseLAMData(data);
                OnLAMDataReceived?.Invoke(imu);
            }
            else
            {
                // Assume JSON status or a text-based response
                string msg = Encoding.UTF8.GetString(data);
                Console.WriteLine($"[Status/Info] {msg}");

                // Store raw JSON
                StatusJson = msg;

                // Attempt to parse as JSON for structured status
                try
                {
                    var lamStatus = JsonSerializer.Deserialize<LAMStatus>(msg);
                    if (lamStatus != null)
                    {
                        Status = lamStatus;
                        // Update the last status update timestamp in seconds
                        _lastStatusUpdate = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                    }
                }
                catch
                {
                    // If parsing fails, keep the raw JSON in StatusJson
                }

                OnLAMStatusUpdated?.Invoke(msg);
            }
        }

        /// <summary>
        /// Converts 40 bytes (10 floats) into an LAMData object (including timestamp).
        /// </summary>
        private LAMData ParseLAMData(byte[] bytes)
        {
            float[] floats = new float[10];
            Buffer.BlockCopy(bytes, 0, floats, 0, 40);

            return new LAMData
            {
                AccelX = floats[0],
                AccelY = floats[1],
                AccelZ = floats[2],
                GyroX = floats[3],
                GyroY = floats[4],
                GyroZ = floats[5],
                MagX = floats[6],
                MagY = floats[7],
                MagZ = floats[8],
                TimeStamp = floats[9]
            };
        }

        public void Dispose()
        {
            StopListening();
            _udpClient.Dispose();
        }
    }
}
