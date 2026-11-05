using QuickTouch.HID;
using QuickTouch.Util;

namespace QuickTouch.I2C
{
    /// <summary>
    /// Selects the I2C bridge of the touch protocols: E-OCD (HID) or A31G336 DAPLink (WinUSB).
    /// </summary>
    public static class TouchTransportFactory
    {
        /// <summary>true = A31G336 DAPLink, false = E-OCD.</summary>
        public static bool UseDapLink = true;

        /// <summary>DAPLink SCL in Hz (the A96T418 answers at 100 kHz, not at 400 kHz).</summary>
        public static int DapLinkClockHz = 100000;

        public const int EOCD_VID = 0x1A29;
        public const int EOCD_PID = 0xAA06;

        public static int VendorId
        {
            get { return UseDapLink ? DapLinkTransport.DEFAULT_VID : EOCD_VID; }
        }

        public static int ProductId
        {
            get { return UseDapLink ? DapLinkTransport.DEFAULT_PID : EOCD_PID; }
        }

        /// <param name="slaveId">E-OCD style 8-bit write address (0x40)</param>
        public static ITouchTransport Create(byte slaveId)
        {
            if (UseDapLink)
            {
                DapLinkTransport.Log = s => QTUtil.PrintLog(s);
                return new DapLinkTransport((byte)(slaveId >> 1), DapLinkClockHz);     // 7-bit address
            }
            return new I2CTransport(new HIDController(), slaveId);
        }
    }
}
