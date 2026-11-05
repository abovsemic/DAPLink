using System;

namespace QuickTouch.I2C
{
    /// <summary>
    /// Touch IC register access through an I2C bridge: E-OCD (I2CTransport, HID) or
    /// A31G336 DAPLink (DapLinkTransport, WinUSB). The slave address is given at creation.
    /// </summary>
    public interface ITouchTransport
    {
        /// <summary>The bridge was unplugged.</summary>
        event EventHandler Removed;

        bool Open(int vendorId, int productId);
        void Close();
        bool IsOpen();

        /// <summary>Writes 'size' bytes (1..4) of 'value', MSB first, from register 'address'.</summary>
        bool Write(byte address, int size, int value);

        /// <summary>Writes data[0..size-1] from register 'address'.</summary>
        bool Write(byte address, int size, byte[] data);

        /// <summary>Reads 'size' bytes from register 'address'.</summary>
        bool Read(byte address, int size, out byte[] data);

        /// <summary>Target power off, then on (on = true) or left off. No-op without power control.</summary>
        bool PowerSet(bool on);
    }
}
