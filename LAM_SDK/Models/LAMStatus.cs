using System.Text.Json.Serialization;

namespace LAM_SDK.Models
{
    public class LAMStatus
    {
        [JsonPropertyName("ip")]
        public string Ip { get; set; }

        [JsonPropertyName("remoteIP")]
        public string RemoteIP { get; set; }

        [JsonPropertyName("port")]
        public int Port { get; set; }

        [JsonPropertyName("streaming")]
        public bool Streaming { get; set; }

        [JsonPropertyName("samplingRate")]
        public int SamplingRate { get; set; }

        [JsonPropertyName("lastConnection")]
        public long LastConnection { get; set; }

        [JsonPropertyName("connected")]
        public bool Connected { get; set; }
    }
}
