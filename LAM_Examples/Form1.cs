using LAM_SDK;
using LAM_SDK.Models;
using System.Diagnostics;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;
using System.Text.Json;

namespace LAM_Examples
{
    public partial class Form1 : Form
    {
        private LAM lamInstance; 
        private float _pitchDegrees = 0f;
        private float _rollDegrees = 0f;
        public Form1()
        {
            InitializeComponent();

            // Create LAM
            lamInstance = new LAM();

            // 2. Subscribe to events
            lamInstance.OnLAMDataReceived += (data) =>
            {
                Debug.WriteLine(
                    $"IMU => A=({data.AccelX:F2}, {data.AccelY:F2}, {data.AccelZ:F2}), " +
                    $"G=({data.GyroX:F2}, {data.GyroY:F2}, {data.GyroZ:F2})");

                // Accelerometer values: data.AccelX, data.AccelY, data.AccelZ
                // Convert to pitch/roll in degrees. 
                // Adjust signs or axes as needed for your sensor orientation.

                double ax = data.AccelX;
                double ay = data.AccelY;
                double az = data.AccelZ;

                // Typical approach:
                double pitchRad = Math.Atan2(-ax, Math.Sqrt(ay * ay + az * az));
                double rollRad = Math.Atan2(ay, az);

                _pitchDegrees = (float)(pitchRad * 180.0 / Math.PI);
                _rollDegrees = (float)(rollRad * 180.0 / Math.PI);

                // Force the PictureBox to redraw
                pictureBox1.Invalidate();
            };

            lamInstance.OnLAMStatusUpdated += (status) =>
            {
                Debug.WriteLine($"[Status/Info] {status}");
                // update the textbox_status
                if ($"{status}" == "Disconnected")
                {   
                    InitializeLAM();
                } else
                {
                    // Attempt to parse as JSON for structured status
                    try
                    {
                        var lamStatus = JsonSerializer.Deserialize<LAMStatus>(status);
                        if (lamStatus != null)
                        {
                            UpdateStatusText($"Connected: {lamStatus?.Connected},Streaming: {lamStatus?.Streaming}");
                        }
                    }
                    catch
                    {

                    }

                }

                
            };


            InitializeLAM();
        }

        private async void InitializeLAM()
        {

            Debug.WriteLine("InitializeLAM...");
            // 1. Discover devices via mDNS
            UpdateStatusText("Waiting for device...");
            var foundDevices = await LAM.DiscoverDevicesAsync("_imu._udp.local.");
            while(foundDevices.Count == 0)
            {
                foundDevices = await LAM.DiscoverDevicesAsync("_imu._udp.local.");
                Debug.WriteLine("No devices found. Trying again...");
            }

            // 2. Pick the first discovered device
            var device = foundDevices[0];
            Debug.WriteLine($"Found device {device.ToString()}");

            lamInstance.Initialize(device.IPAddress, device.Port);

            //lamInstance.SendInitialize(); // Not needed, only for demonstration

            lamInstance.StartListening();
        }


        private void UpdateStatusText(string text)
        {
            if (this.textbox_status.InvokeRequired)
            {
                this.textbox_status.Invoke(new Action(() => this.textbox_status.Text = text));
            }
            else
            {
                this.textbox_status.Text = text;
            }
        }

        private void pictureBox1_Paint(object sender, PaintEventArgs e)
        {
            // 1. Basic setup
            Graphics g = e.Graphics;
            g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            int width = pictureBox1.Width;
            int height = pictureBox1.Height;

            // 2. Translate to center so we can rotate around the midpoint
            g.TranslateTransform(width / 2f, height / 2f);

            // 3. Apply ROLL rotation around center
            g.RotateTransform(_rollDegrees);

            // 4. Apply PITCH as a vertical shift. 
            //    For example, 1 degree of pitch => some # of pixels up/down. 
            //    We'll pick 2 pixels/degree just as a guess.
            float pitchOffsetPixels = _pitchDegrees * 2f;
            g.TranslateTransform(0, pitchOffsetPixels);

            // 5. Draw the horizon rectangle
            //    We'll define a large rectangle that extends beyond the visible area 
            //    so that when it rotates, we don't see corners.
            int rectWidth = width * 2;
            int rectHeight = height * 2;

            // The rectangle’s top half is sky, bottom half is ground
            Rectangle horizonRect = new Rectangle(-rectWidth / 2, -rectHeight / 2, rectWidth, rectHeight);

            // Let's subdivide the horizonRect in the middle (the horizon line):
            // We can do this by painting two rectangles: sky (top) and ground (bottom).

            // 5a: Draw sky in top half
            using (SolidBrush skyBrush = new SolidBrush(Color.LightSkyBlue))
            {
                Rectangle topRect = new Rectangle(horizonRect.Left, horizonRect.Top,
                                                  horizonRect.Width, horizonRect.Height / 2);
                g.FillRectangle(skyBrush, topRect);
            }

            // 5b: Draw ground in bottom half
            using (SolidBrush groundBrush = new SolidBrush(Color.BurlyWood))
            {
                Rectangle bottomRect = new Rectangle(horizonRect.Left,
                                                     horizonRect.Top + horizonRect.Height / 2,
                                                     horizonRect.Width, horizonRect.Height / 2);
                g.FillRectangle(groundBrush, bottomRect);
            }

            // 6. Optionally, draw a reference line for the horizon
            Pen horizonPen = new Pen(Color.Black, 2);
            g.DrawLine(horizonPen, -rectWidth / 2, 0, rectWidth / 2, 0);

            // 7. Reset transforms if you plan to draw more
            g.ResetTransform();
        }

        private void samplingRateCombo_ValueChanged(object sender, EventArgs e)
        {
            int samplingRate = int.Parse(this.samplingRateCombo.Text);
            lamInstance.SetSamplingRate(samplingRate);
        }

        private void button_getStatus_Click(object sender, EventArgs e)
        {
            lamInstance.GetStatus();
        }

        private void button_start_imu_Click(object sender, EventArgs e)
        {
            lamInstance.StartStreaming();
            lamInstance.GetStatus();

        }

        private void button_stop_imu_Click(object sender, EventArgs e)
        {
            lamInstance.StopStreaming();
            lamInstance.GetStatus();
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }
        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            lamInstance.StopListening();
            lamInstance.Dispose();
        }
    }
}
