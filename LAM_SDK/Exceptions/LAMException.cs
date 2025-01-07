using System;

namespace LAM_SDK.Exceptions
{
    public class LAMException : Exception
    {
        public LAMException(string message) : base(message) { }
    }
}
