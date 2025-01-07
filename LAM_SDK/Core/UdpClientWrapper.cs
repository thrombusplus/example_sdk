using System;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;

namespace LAM_SDK.Core
{
    public class UdpClientWrapper : IDisposable
    {
        private UdpClient _udpClient;
        private IPEndPoint _remoteEndPoint;
        private bool _isListening;

        /// <summary>
        /// Fired whenever a UDP packet is received asynchronously.
        /// </summary>
        public event Action<byte[]> DataReceived;

        /// <summary>
        /// Initialize the UDP client with a remote IP/port for sending commands,
        /// and optionally a local port for receiving data.
        /// </summary>
        public void Initialize(string remoteIp, int remotePort, int localPort = 0)
        {
            _remoteEndPoint = new IPEndPoint(IPAddress.Parse(remoteIp), remotePort);

            // If localPort = 0, the OS picks a free port automatically
            _udpClient = new UdpClient(localPort);
            // Optional: allow socket reuse
            _udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        }

        /// <summary>
        /// Begin asynchronous receive loop. On each packet, DataReceived is raised.
        /// </summary>
        public async void StartListeningAsync()
        {
            if (_udpClient == null)
                throw new InvalidOperationException("UDP client not initialized. Call Initialize() first.");

            _isListening = true;

            while (_isListening)
            {
                try
                {
                    var result = await _udpClient.ReceiveAsync();
                    DataReceived?.Invoke(result.Buffer);
                }
                catch (ObjectDisposedException)
                {
                    // Occurs if _udpClient is closed during receive
                    break;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"UDP Receive Error: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// Stop the listening loop gracefully.
        /// </summary>
        public void StopListening()
        {
            _isListening = false;
        }

        /// <summary>
        /// Send raw bytes to the remote endpoint.
        /// </summary>
        public void Send(byte[] data)
        {
            if (_udpClient == null)
                throw new InvalidOperationException("UDP client not initialized.");

            _udpClient.Send(data, data.Length, _remoteEndPoint);
        }

        public void Dispose()
        {
            StopListening();
            _udpClient?.Close();
            _udpClient?.Dispose();
        }
    }
}
