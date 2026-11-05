// DapLinkTransport.cs
//
// Touch IC register access through the A31G336 DAPLink I2C vendor commands
// (source/hic_hal/abov/a31g33x/i2c_vendor_a31g33x.c), WinUSB bulk.
// ITouchTransport like QuickTouch I2C/I2CTransport.cs (E-OCD HID), so the protocol classes
// can use either one. No external library: winusb.dll and setupapi.dll through P/Invoke.
// Written in C# 5 (builds with the .NET Framework csc.exe and with newer compilers).
//
// USB interface:
//  - "A-Link I2C" interface (own GUID, 128-byte packets): used when the firmware has it. Keil or
//    OpenOCD can debug at the same time through the CMSIS-DAP v2 interface.
//  - otherwise the CMSIS-DAP v2 interface (older firmware, 512-byte packets): not usable while a
//    debugger is connected.
//
// Differences to the E-OCD transport:
//  - slave address is 7 bit (E-OCD 0x40 -> 0x20)
//  - every command has a response with a status: no endless wait on a missing slave
//  - register read = write reg, STOP, START, read (the touch IC does not answer a repeated START)
//  - no target power switch: PowerSet only logs

using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace QuickTouch.I2C
{
    public class DapLinkTransport : ITouchTransport, IDisposable
    {
        // DAP vendor commands
        private const byte ID_DAP_INFO        = 0x00;
        private const byte ID_I2C_CONFIG      = 0xA0;
        private const byte ID_I2C_WRITE       = 0xA1;
        private const byte ID_I2C_WRITEREAD   = 0xA3;
        private const byte ID_I2C_SCAN        = 0xA4;
        private const byte ID_I2C_INTSTATE    = 0xA5;
        private const byte ID_STREAM_START    = 0xA6;
        private const byte ID_STREAM_READ     = 0xA7;
        private const byte ID_STREAM_STOP     = 0xA8;

        public const byte FLAG_NO_STOP        = 0x01;
        public const byte FLAG_STOP_START     = 0x02;

        public const byte STREAM_PERIODIC     = 0;
        public const byte STREAM_INT_FALLING  = 1;
        public const byte STREAM_INT_RISING   = 2;
        public const byte STREAM_INT_BOTH     = 3;

        public const int DEFAULT_VID          = 0x1A29;
        public const int DEFAULT_PID          = 0xA809;

        private const int DAP_PACKET_SIZE     = 512;    // DAP_PACKET_SIZE of the a31g336 build
        private const int I2C_PACKET_SIZE     = 128;    // I2CU_PACKET_SIZE (i2c_vendor_a31g33x.c)
        private const int RX_BUFFER_SIZE      = 512;    // >= every response (the device ends it with a short packet)
        private const uint USB_TIMEOUT_MS     = 1000;
        private const uint DRAIN_TIMEOUT_MS   = 20;

        // WinUSB interfaces (MS OS 2.0 DeviceInterfaceGUIDs)
        private static readonly Guid I2C_GUID          = new Guid("6E0C9B7A-3F42-4D1E-A5B8-91C7D2E4F036");
        private static readonly Guid CMSIS_DAP_V2_GUID = new Guid("CDB3B5AD-293B-4663-AA36-1AAE46463776");

        /// <summary>Log output (TouchTransportFactory sets QTUtil.PrintLog).</summary>
        public static Action<string> Log = s => System.Diagnostics.Debug.WriteLine(s);

        /// <summary>The DAPLink was unplugged (USB transfer failed with a "device gone" error).</summary>
        public event EventHandler Removed;

        private readonly object _sync = new object();
        private readonly byte _slave;
        private readonly int _clockHz;
        private SafeFileHandle _file;
        private IntPtr _winusb = IntPtr.Zero;
        private byte _pipeOut;
        private byte _pipeIn;
        private bool _gone;                 // set by TransferFailed, Removed raised by Command
        private bool _i2cInterface;         // A-Link I2C interface (false: CMSIS-DAP v2 interface)
        private int _packetSize = DAP_PACKET_SIZE;

        private int WriteMax { get { return _packetSize - 5; } }
        private int ReadMax { get { return _packetSize - 4; } }

        /// <summary>True: own I2C interface, a debugger can use the CMSIS-DAP interface at the same time.</summary>
        public bool IsI2CInterface { get { return _i2cInterface; } }

        /// <summary>Stream record: [status][count][timestamp:4][data:count]</summary>
        public class StreamRecord
        {
            public byte Status;
            public uint Timestamp;              // TIMESTAMP_CLOCK ticks (StreamStart result)
            public byte[] Data;
        }

        /// <param name="slaveId">7-bit address (E-OCD 8-bit 0x40 = 0x20)</param>
        /// <param name="clockHz">SCL 100000..400000 (the A96T418 answers at 100 kHz)</param>
        public DapLinkTransport(byte slaveId, int clockHz)
        {
            _slave = slaveId;
            _clockHz = clockHz;
        }

        public DapLinkTransport(byte slaveId) : this(slaveId, 100000)
        {
        }

        public static string StatusText(byte status)
        {
            switch (status)
            {
                case 0x00: return "OK";
                case 0x01: return "NACK_ADDR";
                case 0x02: return "NACK_DATA";
                case 0x03: return "ARB_LOST";
                case 0x04: return "BUS_BUSY";
                case 0x05: return "TIMEOUT";
                case 0x06: return "NOT_CONFIG";
                case 0x07: return "STREAMING";
                case 0xFF: return "PARAM";
                default:   return string.Format("0x{0:X2}", status);
            }
        }

        //******************************************************************************************
        // Open / close

        public bool Open(int vendorId, int productId)
        {
            lock (_sync)
            {
                CloseHandles();
                _gone = false;
                _i2cInterface = true;
                string path = FindDevicePath(I2C_GUID, vendorId, productId);
                if (path == null)
                {
                    _i2cInterface = false;
                    path = FindDevicePath(CMSIS_DAP_V2_GUID, vendorId, productId);
                }
                if (path == null)
                {
                    Log(string.Format("DAPLink 0x{0:x4}/0x{1:x4} not found.", vendorId, productId));
                    return false;
                }
                _packetSize = _i2cInterface ? I2C_PACKET_SIZE : DAP_PACKET_SIZE;
                if (!OpenWinUsb(path))
                {
                    return false;
                }
            }

            uint clock;
            if (!Config(_clockHz, out clock))
            {
                Close();
                return false;
            }
            if (_i2cInterface)
            {
                Log(string.Format("[opened] A-Link I2C interface, I2C {0} Hz", clock));
            }
            else
            {
                Log(string.Format("[opened] {0} (CMSIS-DAP interface: debugger not usable at the same time), I2C {1} Hz",
                                  GetInfoString(0x02), clock));
            }
            return true;
        }

        public bool Open()
        {
            return Open(DEFAULT_VID, DEFAULT_PID);
        }

        public bool IsOpen()
        {
            return _winusb != IntPtr.Zero;
        }

        public void Close()
        {
            lock (_sync)
            {
                if (_winusb != IntPtr.Zero)
                {
                    byte[] rsp;
                    Transfer(new byte[] { ID_STREAM_STOP }, out rsp);
                    Transfer(new byte[] { ID_I2C_CONFIG, 0, 0, 0, 0 }, out rsp);    // I2C off, pins to inputs
                }
                CloseHandles();
            }
        }

        public void Dispose()
        {
            Close();
        }

        //******************************************************************************************
        // QuickTouch I2CTransport methods

        /// <summary>Writes 'size' bytes (1..4) of 'value', MSB first, from register 'address'.</summary>
        public bool Write(byte address, int size, int value)
        {
            if (size <= 0 || size > 4)
            {
                Log(string.Format("your I2C write size={0} must be between 1 and 4.", size));
                return false;
            }
            byte[] data = new byte[size];
            for (int i = 0; i < size; i++)
            {
                data[i] = (byte)(value >> ((size - 1 - i) * 8));
            }
            return Write(address, size, data);
        }

        /// <summary>Writes data[0..size-1] from register 'address'.</summary>
        public bool Write(byte address, int size, byte[] data)
        {
            if (data == null || size <= 0 || size > data.Length || size + 1 > WriteMax)
            {
                Log(string.Format("invalid I2C write size={0}.", size));
                return false;
            }

            int len = size + 1;                                 // register + data
            byte[] req = new byte[5 + len];
            req[0] = ID_I2C_WRITE;
            req[1] = _slave;
            req[2] = 0;
            req[3] = (byte)len;
            req[4] = (byte)(len >> 8);
            req[5] = address;
            Buffer.BlockCopy(data, 0, req, 6, size);

            byte[] rsp;
            if (!Command(req, 4, out rsp))
            {
                return false;
            }
            int count = rsp[2] | (rsp[3] << 8);
            if (rsp[1] != 0 || count != len)
            {
                Log(string.Format("I2C write 0x{0:X2}/0x{1:X2}: {2}, {3}/{4} bytes ACKed",
                                  _slave, address, StatusText(rsp[1]), count, len));
                return false;
            }
            return true;
        }

        /// <summary>Reads 'size' bytes from register 'address' (write reg, STOP, START, read).</summary>
        public bool Read(byte address, int size, out byte[] data)
        {
            return ReadReg(address, size, FLAG_STOP_START, out data);
        }

        /// <summary>No target power switch on the DAPLink board: nothing to do.</summary>
        public bool PowerSet(bool on)
        {
            Log("PowerSet: no target power control on the DAPLink board (skipped).");
            return true;
        }

        //******************************************************************************************
        // DAPLink extras

        public bool ReadReg(byte address, int size, byte flags, out byte[] data)
        {
            data = null;
            if (size <= 0 || size > ReadMax)
            {
                Log(string.Format("invalid I2C read size={0}.", size));
                return false;
            }

            byte[] req = new byte[] {
                ID_I2C_WRITEREAD, _slave, flags, 1, 0, (byte)size, (byte)(size >> 8), address
            };
            byte[] rsp;
            if (!Command(req, 4, out rsp))
            {
                return false;
            }
            int count = rsp[2] | (rsp[3] << 8);
            if (rsp[1] != 0 || count != size || rsp.Length < 4 + size)
            {
                Log(string.Format("I2C read 0x{0:X2}/0x{1:X2}: {2}, {3}/{4} bytes",
                                  _slave, address, StatusText(rsp[1]), count, size));
                return false;
            }
            data = new byte[size];
            Buffer.BlockCopy(rsp, 4, data, 0, size);
            return true;
        }

        /// <summary>SCL in Hz (100000..400000), 0 = I2C off. actualHz = programmed SCL.</summary>
        public bool Config(int clockHz, out uint actualHz)
        {
            actualHz = 0;
            byte[] req = new byte[] {
                ID_I2C_CONFIG, (byte)clockHz, (byte)(clockHz >> 8), (byte)(clockHz >> 16), (byte)(clockHz >> 24)
            };
            byte[] rsp;
            if (!Command(req, 6, out rsp))
            {
                return false;
            }
            actualHz = BitConverter.ToUInt32(rsp, 2);
            if (rsp[1] != 0)
            {
                Log(string.Format("I2C config {0} Hz: {1}", clockHz, StatusText(rsp[1])));
                return false;
            }
            return true;
        }

        /// <summary>7-bit addresses (0x08..0x77) that ACK, null on error.</summary>
        public byte[] Scan()
        {
            byte[] rsp;
            if (!Command(new byte[] { ID_I2C_SCAN }, 18, out rsp) || rsp[1] != 0)
            {
                return null;
            }
            var found = new System.Collections.Generic.List<byte>();
            for (int a = 0x08; a <= 0x77; a++)
            {
                if ((rsp[2 + (a >> 3)] & (1 << (a & 7))) != 0)
                {
                    found.Add((byte)a);
                }
            }
            return found.ToArray();
        }

        /// <summary>Touch INT (PA10) level, -1 on error.</summary>
        public int GetIntLevel()
        {
            byte[] rsp;
            if (!Command(new byte[] { ID_I2C_INTSTATE }, 3, out rsp) || rsp[1] != 0)
            {
                return -1;
            }
            return rsp[2];
        }

        /// <summary>
        /// Background register read: every 'periodMs' (mode STREAM_PERIODIC) or on a touch INT edge.
        /// tsClock = timestamp clock of the records in Hz.
        /// </summary>
        public bool StreamStart(byte mode, int periodMs, byte address, int size, byte flags, out uint tsClock)
        {
            tsClock = 0;
            byte[] req = new byte[] {
                ID_STREAM_START, _slave, flags, mode, (byte)periodMs, (byte)(periodMs >> 8), 1, (byte)size, address
            };
            byte[] rsp;
            if (!Command(req, 6, out rsp))
            {
                return false;
            }
            tsClock = BitConverter.ToUInt32(rsp, 2);
            if (rsp[1] != 0)
            {
                Log(string.Format("stream start: {0}", StatusText(rsp[1])));
                return false;
            }
            return true;
        }

        public bool StreamStart(byte mode, int periodMs, byte address, int size, out uint tsClock)
        {
            return StreamStart(mode, periodMs, address, size, FLAG_STOP_START, out tsClock);
        }

        /// <summary>Queued records (oldest first). overrun/missed: since the last call.</summary>
        public StreamRecord[] StreamRead(out int overrun, out int missed)
        {
            overrun = 0;
            missed = 0;
            byte[] rsp;
            if (!Command(new byte[] { ID_STREAM_READ }, 7, out rsp))
            {
                return null;
            }
            overrun = rsp[2] | (rsp[3] << 8);
            missed = rsp[4] | (rsp[5] << 8);
            int n = rsp[6];
            var records = new StreamRecord[n];
            int pos = 7;
            for (int i = 0; i < n; i++)
            {
                if (pos + 6 > rsp.Length)
                {
                    Log("stream read: truncated response.");
                    return null;
                }
                var rec = new StreamRecord();
                rec.Status = rsp[pos];
                int count = rsp[pos + 1];
                rec.Timestamp = BitConverter.ToUInt32(rsp, pos + 2);
                rec.Data = new byte[count];
                Buffer.BlockCopy(rsp, pos + 6, rec.Data, 0, count);
                records[i] = rec;
                pos += 6 + count;
            }
            return records;
        }

        public bool StreamStop()
        {
            byte[] rsp;
            return Command(new byte[] { ID_STREAM_STOP }, 2, out rsp) && rsp[1] == 0;
        }

        /// <summary>
        /// CMSIS-DAP DAP_Info string (0x02 product, 0x04 protocol, 0x09 firmware version).
        /// "" on the I2C interface (I2C commands only).
        /// </summary>
        public string GetInfoString(byte id)
        {
            byte[] rsp;
            if (_i2cInterface || !Command(new byte[] { ID_DAP_INFO, id }, 2, out rsp))
            {
                return "";
            }
            int len = Math.Min(rsp[1], rsp.Length - 2);
            return Encoding.ASCII.GetString(rsp, 2, len).TrimEnd('\0');
        }

        //******************************************************************************************
        // Transport

        /// <summary>One request/response; the response must echo the command and have minLen bytes.</summary>
        private bool Command(byte[] request, int minLen, out byte[] response)
        {
            bool ok;
            bool removed = false;
            lock (_sync)
            {
                ok = Transfer(request, out response);
                if (!ok && _gone)
                {
                    _gone = false;
                    removed = true;
                }
            }
            if (removed)
            {
                EventHandler handler = Removed;
                if (handler != null)
                {
                    handler(this, EventArgs.Empty);
                }
            }
            if (!ok)
            {
                return false;
            }
            if (response.Length < minLen || response[0] != request[0])
            {
                Log(string.Format("bad response to 0x{0:X2} ({1} bytes).", request[0], response.Length));
                return false;
            }
            return true;
        }

        private bool Transfer(byte[] request, out byte[] response)
        {
            response = null;
            if (_winusb == IntPtr.Zero)
            {
                Log("DAPLink is not open.");
                return false;
            }

            uint done;
            if (!WinUsb_WritePipe(_winusb, _pipeOut, request, (uint)request.Length, out done, IntPtr.Zero) ||
                done != request.Length)
            {
                TransferFailed("usb write failed: ");
                return false;
            }

            byte[] rx = new byte[RX_BUFFER_SIZE];
            if (!WinUsb_ReadPipe(_winusb, _pipeIn, rx, (uint)rx.Length, out done, IntPtr.Zero) || done == 0)
            {
                TransferFailed("usb read failed: ");
                return false;
            }
            response = new byte[done];
            Buffer.BlockCopy(rx, 0, response, 0, (int)done);
            return true;
        }

        /// <summary>Logs the error; a "device gone" error closes the handles and reports Removed.</summary>
        private void TransferFailed(string what)
        {
            int err = Marshal.GetLastWin32Error();
            Log(what + new Win32Exception(err).Message);
            switch (err)
            {
                case 2:         // ERROR_FILE_NOT_FOUND
                case 6:         // ERROR_INVALID_HANDLE
                case 22:        // ERROR_BAD_COMMAND
                case 31:        // ERROR_GEN_FAILURE
                case 433:       // ERROR_NO_SUCH_DEVICE
                case 1167:      // ERROR_DEVICE_NOT_CONNECTED
                    _gone = true;
                    CloseHandles();
                    break;
            }
        }

        private static string FindDevicePath(Guid interfaceGuid, int vendorId, int productId)
        {
            Guid guid = interfaceGuid;
            string match = string.Format("vid_{0:x4}&pid_{1:x4}", vendorId, productId);
            IntPtr devs = SetupDiGetClassDevs(ref guid, IntPtr.Zero, IntPtr.Zero, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            if (devs == INVALID_HANDLE_VALUE)
            {
                return null;
            }
            try
            {
                var ifData = new SP_DEVICE_INTERFACE_DATA();
                ifData.cbSize = Marshal.SizeOf(typeof(SP_DEVICE_INTERFACE_DATA));
                for (uint i = 0; SetupDiEnumDeviceInterfaces(devs, IntPtr.Zero, ref guid, i, ref ifData); i++)
                {
                    uint needed;
                    SetupDiGetDeviceInterfaceDetail(devs, ref ifData, IntPtr.Zero, 0, out needed, IntPtr.Zero);
                    IntPtr detail = Marshal.AllocHGlobal((int)needed);
                    try
                    {
                        // SP_DEVICE_INTERFACE_DETAIL_DATA_W: cbSize = 8 (x64) / 6 (x86), then the path
                        Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
                        if (SetupDiGetDeviceInterfaceDetail(devs, ref ifData, detail, needed, out needed, IntPtr.Zero))
                        {
                            string path = Marshal.PtrToStringUni(new IntPtr(detail.ToInt64() + 4));
                            if (path.ToLowerInvariant().Contains(match))
                            {
                                return path;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(detail);
                    }
                }
                return null;
            }
            finally
            {
                SetupDiDestroyDeviceInfoList(devs);
            }
        }

        private bool OpenWinUsb(string path)
        {
            _file = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               IntPtr.Zero, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, IntPtr.Zero);
            if (_file.IsInvalid)
            {
                Log((_i2cInterface ? "DAPLink I2C interface open failed (in use by another program?): "
                                   : "DAPLink open failed (in use by OpenOCD/Keil?): ") +
                    new Win32Exception(Marshal.GetLastWin32Error()).Message);
                CloseHandles();
                return false;
            }
            if (!WinUsb_Initialize(_file, out _winusb))
            {
                Log("WinUsb_Initialize failed: " + new Win32Exception(Marshal.GetLastWin32Error()).Message);
                _winusb = IntPtr.Zero;
                CloseHandles();
                return false;
            }

            USB_INTERFACE_DESCRIPTOR intf;
            _pipeOut = 0;
            _pipeIn = 0;
            if (WinUsb_QueryInterfaceSettings(_winusb, 0, out intf))
            {
                for (byte i = 0; i < intf.bNumEndpoints; i++)
                {
                    WINUSB_PIPE_INFORMATION pipe;
                    if (!WinUsb_QueryPipe(_winusb, 0, i, out pipe) || pipe.PipeType != UsbdPipeTypeBulk)
                    {
                        continue;
                    }
                    if ((pipe.PipeId & 0x80) != 0)
                    {
                        if (_pipeIn == 0) _pipeIn = pipe.PipeId;
                    }
                    else
                    {
                        if (_pipeOut == 0) _pipeOut = pipe.PipeId;
                    }
                }
            }
            if (_pipeIn == 0 || _pipeOut == 0)
            {
                Log("DAPLink bulk endpoints not found.");
                CloseHandles();
                return false;
            }

            // A request that ends on a packet boundary is terminated by a zero-length packet
            byte terminate = 1;
            WinUsb_SetPipePolicy(_winusb, _pipeOut, SHORT_PACKET_TERMINATE, 1, ref terminate);

            // Stale responses of an earlier session (program closed before reading): read until timeout
            uint timeout = DRAIN_TIMEOUT_MS;
            WinUsb_SetPipePolicy(_winusb, _pipeIn, PIPE_TRANSFER_TIMEOUT, 4, ref timeout);
            WinUsb_FlushPipe(_winusb, _pipeIn);
            byte[] rx = new byte[RX_BUFFER_SIZE];
            uint done;
            for (int i = 0; i < 8 && WinUsb_ReadPipe(_winusb, _pipeIn, rx, (uint)rx.Length, out done, IntPtr.Zero); i++)
            {
            }

            timeout = USB_TIMEOUT_MS;
            WinUsb_SetPipePolicy(_winusb, _pipeOut, PIPE_TRANSFER_TIMEOUT, 4, ref timeout);
            WinUsb_SetPipePolicy(_winusb, _pipeIn, PIPE_TRANSFER_TIMEOUT, 4, ref timeout);
            return true;
        }

        private void CloseHandles()
        {
            if (_winusb != IntPtr.Zero)
            {
                WinUsb_Free(_winusb);
                _winusb = IntPtr.Zero;
            }
            if (_file != null)
            {
                _file.Dispose();
                _file = null;
            }
        }

        //******************************************************************************************
        // Win32

        private const uint DIGCF_PRESENT = 0x02;
        private const uint DIGCF_DEVICEINTERFACE = 0x10;
        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint FILE_SHARE_READ = 0x01;
        private const uint FILE_SHARE_WRITE = 0x02;
        private const uint OPEN_EXISTING = 3;
        private const uint FILE_ATTRIBUTE_NORMAL = 0x80;
        private const uint FILE_FLAG_OVERLAPPED = 0x40000000;
        private const uint SHORT_PACKET_TERMINATE = 0x01;
        private const uint PIPE_TRANSFER_TIMEOUT = 0x03;
        private const int UsbdPipeTypeBulk = 2;
        private static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

        [StructLayout(LayoutKind.Sequential)]
        private struct SP_DEVICE_INTERFACE_DATA
        {
            public int cbSize;
            public Guid InterfaceClassGuid;
            public int Flags;
            public IntPtr Reserved;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct USB_INTERFACE_DESCRIPTOR
        {
            public byte bLength;
            public byte bDescriptorType;
            public byte bInterfaceNumber;
            public byte bAlternateSetting;
            public byte bNumEndpoints;
            public byte bInterfaceClass;
            public byte bInterfaceSubClass;
            public byte bInterfaceProtocol;
            public byte iInterface;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct WINUSB_PIPE_INFORMATION
        {
            public int PipeType;
            public byte PipeId;
            public ushort MaximumPacketSize;
            public byte Interval;
        }

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, IntPtr enumerator, IntPtr hwndParent, uint flags);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiEnumDeviceInterfaces(IntPtr devs, IntPtr devInfo, ref Guid guid, uint index,
                                                               ref SP_DEVICE_INTERFACE_DATA ifData);

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr devs, ref SP_DEVICE_INTERFACE_DATA ifData,
                                                                   IntPtr detail, uint detailSize, out uint required,
                                                                   IntPtr devInfo);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiDestroyDeviceInfoList(IntPtr devs);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr security,
                                                        uint creation, uint flags, IntPtr template);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_Initialize(SafeFileHandle device, out IntPtr winusb);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_Free(IntPtr winusb);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_QueryInterfaceSettings(IntPtr winusb, byte altSetting,
                                                                 out USB_INTERFACE_DESCRIPTOR desc);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_QueryPipe(IntPtr winusb, byte altSetting, byte pipeIndex,
                                                    out WINUSB_PIPE_INFORMATION pipe);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_SetPipePolicy(IntPtr winusb, byte pipeId, uint policy, uint length, ref uint value);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_SetPipePolicy(IntPtr winusb, byte pipeId, uint policy, uint length, ref byte value);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_FlushPipe(IntPtr winusb, byte pipeId);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_WritePipe(IntPtr winusb, byte pipeId, byte[] buffer, uint length,
                                                    out uint transferred, IntPtr overlapped);

        [DllImport("winusb.dll", SetLastError = true)]
        private static extern bool WinUsb_ReadPipe(IntPtr winusb, byte pipeId, byte[] buffer, uint length,
                                                   out uint transferred, IntPtr overlapped);
    }
}
