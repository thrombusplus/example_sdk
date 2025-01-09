namespace LAM_SDK.Models
{
    public class LAMData
    {
        public float AccelX { get; set; }
        public float AccelY { get; set; }
        public float AccelZ { get; set; }

        public float GyroX { get; set; }
        public float GyroY { get; set; }
        public float GyroZ { get; set; }

        public float MagX { get; set; }
        public float MagY { get; set; }
        public float MagZ { get; set; }

        public float TimeStamp { get; set; }
        public override string ToString()
        {
            return $"{TimeStamp},{AccelX:F2},{AccelY:F2},{AccelZ:F2},{GyroX:F2},{GyroY:F2},{GyroZ:F2},{MagX:F2},{MagY:F2},{MagZ:F2}";
        }
    }
}
