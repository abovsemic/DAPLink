// DapLinkTransportTest.cs - console check of DapLinkTransport on a real board
//
//   build.bat  ->  DapLinkTransportTest.exe [clockHz] [periodMs] [seconds]   (periodMs 0 = touch INT trigger)
//
// 1. touch IC version read (same sequence as the QuickTouch Connect, without PowerSet)
// 2. A96T418 only: channel set CS0..CS9 with the QuickTouch default parameters, start the function
//    test, stream DIFF..KEY (0xC8, 43 bytes) every periodMs

using System;
using System.Diagnostics;
using System.Threading;
using QuickTouch.I2C;

namespace DapLinkTest
{
    internal static class Program
    {
        private const byte SLAVE = 0x20;               // E-OCD 0x40 >> 1

        private class TouchIc
        {
            public string Name;
            public byte IdReg, IdValue, FwReg, StksReg, LibReg;
            public TouchIc(string name, byte idReg, byte idValue, byte fwReg, byte stksReg, byte libReg)
            {
                Name = name; IdReg = idReg; IdValue = idValue; FwReg = fwReg; StksReg = stksReg; LibReg = libReg;
            }
        }

        // QuickTouch A96T4xxRegister.cs
        private static readonly TouchIc[] ICS = new TouchIc[] {
            new TouchIc("A96T418", 0x00, 0xA2, 0x74, 0x76, 0x06),
            new TouchIc("A96T430", 0xF1, 0xA3, 0xF6, 0xF8, 0xFA),
            new TouchIc("A96T448", 0xF1, 0xA4, 0xF6, 0xF8, 0xFA),
        };

        private static string Xyz(byte[] b)
        {
            return string.Format("{0}.{1}.{2}", b[0] & 0x0F, (b[1] & 0xF0) >> 4, b[1] & 0x0F);
        }

        private static int Main(string[] args)
        {
            int clock = args.Length > 0 ? int.Parse(args[0]) : 100000;
            int period = args.Length > 1 ? int.Parse(args[1]) : 10;
            int seconds = args.Length > 2 ? int.Parse(args[2]) : 1;

            DapLinkTransport.Log = Console.WriteLine;
            using (var dap = new DapLinkTransport(SLAVE, clock))
            {
                if (!dap.Open())
                {
                    return 1;
                }
                if (dap.IsI2CInterface)
                {
                    Console.WriteLine("A-Link I2C interface (debugger usable at the same time), INT(PA10) level {0}",
                                      dap.GetIntLevel());
                }
                else
                {
                    Console.WriteLine("CMSIS-DAP {0}, fw {1}, INT(PA10) level {2}",
                                      dap.GetInfoString(0x04), dap.GetInfoString(0x09), dap.GetIntLevel());
                }

                byte[] found = dap.Scan();
                Console.WriteLine("scan: {0}", found == null ? "error" :
                                  found.Length == 0 ? "no device" :
                                  string.Join(" ", Array.ConvertAll(found, a => string.Format("0x{0:X2}", a))));

                TouchIc ic = null;
                foreach (TouchIc t in ICS)
                {
                    byte[] id;
                    if (dap.Read(t.IdReg, 1, out id) && id[0] == t.IdValue)
                    {
                        ic = t;
                        break;
                    }
                }
                if (ic == null)
                {
                    Console.WriteLine("no supported touch IC found");
                    return 2;
                }

                byte[] fw, stks, lib;
                Console.WriteLine("device        : {0}, DEVICE_ID 0x{1:X2}", ic.Name, ic.IdValue);
                if (dap.Read(ic.FwReg, 2, out fw))     Console.WriteLine("QuickTouch FW : {0}", Xyz(fw));
                if (dap.Read(ic.StksReg, 2, out stks)) Console.WriteLine("STKS code     : {0}", Xyz(stks));
                if (dap.Read(ic.LibReg, 1, out lib))   Console.WriteLine("Touch library : {0}", lib[0]);

                if (ic.Name != "A96T418")
                {
                    return 0;                           // DIFF register map known for the A96T418 only
                }
                return StreamTest(dap, period, seconds);
            }
        }

        // A96T418Register.cs (QuickTouch)
        private const byte SET_COMMAND = 0x01;          // 0x00 none/cancel, 0x01 start function test
        private const byte RUN_STATE = 0x02;            // 0x01 = function test running
        private const byte DIFF = 0xC8;                 // DIFF_DATA_CH00, 2 bytes/channel (MSB first)
        private const byte KEY_OFFSET = 0xF0 - DIFF;    // KEY_ID_0..2 at 0xF0..0xF2
        private const int SIZE = 43;                    // DIFF 0xC8 .. KEY_ID 0xF2
        private const int CHANNELS = 16;

        private const int ACTIVE_CHANNELS = 10;         // CS0..CS9

        /// <summary>
        /// QuickTouch "Channel Set" (PerformSendCommonParameter) with the default parameters of
        /// A96T418RegisterManager.InitializeTouchCommonParameter: register, size, value.
        /// </summary>
        private static readonly int[,] COMMON_PARAMETERS = new int[,] {
            { 0x0A, 1, 0x02 },      // TOUCH_CLK_VAL            4 MHz
            { 0x0B, 1, 0x04 },      // TOUCH_CLK_OFFSET_VAL
            { 0x0C, 1, 0x70 },      // TOUCH_LPF_C_VAL          10.5 pF
            { 0x0D, 1, 0x04 },      // TOUCH_LPF_R_VAL          100 kOhm
            { 0x0E, 1, 0x00 },      // TOUCH_MODE_VAL           normal
            { 0x0F, 1, 0x01 },      // TOUCH_PORT_SEL_VAL       out low
            { 0x10, 2, 4000 },      // TOUCH_VHS_VAL
            { 0x12, 2, 3700 },      // TOUCH_COMP_REFV_VAL
            { 0x14, 1, 0 },         // DETECT_DEBOUNCE_CNT_VAL
            { 0x15, 1, 0 },         // RELEASE_DEBOUNCE_CNT_VAL
            { 0x16, 1, 0 },         // FILTER_IIR_GAIN_VAL
            { 0x17, 2, 300 },       // TIMER_START_UP_VAL
            { 0x19, 2, 500 },       // TIMER_BASELINE_SAMPLE_VAL
            { 0x1B, 1, 3 },         // BASE_RELEASE_HOLD_CNT_VAL
            { 0x1C, 2, 100 },       // TIMER_REVERSE_BASE_SAMPLE_VAL
            { 0x1E, 1, 70 },        // THD_RELEASE_RATE_VAL
            { 0x1F, 2, 50 },        // THD_REVERSE_RATE_VAL
        };

        private static bool SendCommonParameter(DapLinkTransport dap, int channels)
        {
            // ACT_CH_BIT_0..2 (0x07, 3 bytes): bit n = CSn, last byte = CS0..CS7
            int mask = (1 << channels) - 1;
            byte[] act = new byte[] { (byte)(mask >> 16), (byte)(mask >> 8), (byte)mask };
            if (!dap.Write(0x07, act.Length, act))
            {
                return false;
            }
            for (int i = 0; i < COMMON_PARAMETERS.GetLength(0); i++)
            {
                if (!dap.Write((byte)COMMON_PARAMETERS[i, 0], COMMON_PARAMETERS[i, 1], COMMON_PARAMETERS[i, 2]))
                {
                    return false;
                }
            }
            Console.WriteLine("channel set   : CS0..CS{0} (ACT_CH {1:X2}{2:X2}{3:X2}), default parameters",
                              channels - 1, act[0], act[1], act[2]);
            return true;
        }

        /// <summary>QuickTouch PerformFunctionTest steps 0..2: SET_COMMAND 0, 1, wait for RUN_STATE 1</summary>
        private static bool StartFunctionTest(DapLinkTransport dap)
        {
            if (!dap.Write(SET_COMMAND, 1, 0x00) || !dap.Write(SET_COMMAND, 1, 0x01))
            {
                return false;
            }
            var sw = Stopwatch.StartNew();
            int last = -1;
            while (sw.ElapsedMilliseconds < 10000)                 // QuickTouch: 10 s, 10 ms polling
            {
                byte[] state;
                if (!dap.Read(RUN_STATE, 1, out state))
                {
                    Thread.Sleep(10);
                    continue;
                }
                if (state[0] != last)
                {
                    Console.WriteLine("  RUN_STATE 0x{0:X2} at {1} ms", state[0], sw.ElapsedMilliseconds);
                    last = state[0];
                }
                if (state[0] == 0x01)
                {
                    Console.WriteLine("function test : started ({0} ms)", sw.ElapsedMilliseconds);
                    return true;
                }
                Thread.Sleep(10);
            }
            Console.WriteLine("function test : RUN_STATE did not become 0x01");
            return false;
        }

        private static string FormatDiffKey(byte[] d)
        {
            var sb = new System.Text.StringBuilder("DIFF");
            for (int ch = 0; ch < CHANNELS; ch++)
            {
                sb.AppendFormat(" {0}", (short)((d[ch * 2] << 8) | d[ch * 2 + 1]));
            }
            sb.AppendFormat("  KEY {0:X2}{1:X2}{2:X2}", d[KEY_OFFSET], d[KEY_OFFSET + 1], d[KEY_OFFSET + 2]);
            return sb.ToString();
        }

        private static bool SameData(byte[] a, byte[] b)
        {
            if (a.Length != b.Length)
            {
                return false;
            }
            for (int i = 0; i < a.Length; i++)
            {
                if (a[i] != b[i])
                {
                    return false;
                }
            }
            return true;
        }

        private static int StreamTest(DapLinkTransport dap, int periodMs, int seconds)
        {
            if (!SendCommonParameter(dap, ACTIVE_CHANNELS) || !StartFunctionTest(dap))
            {
                return 3;
            }

            uint tsClock;
            // periodMs 0 = touch INT (PA10) falling edge trigger
            byte mode = periodMs > 0 ? DapLinkTransport.STREAM_PERIODIC : DapLinkTransport.STREAM_INT_FALLING;
            if (!dap.StreamStart(mode, periodMs, DIFF, SIZE, out tsClock))
            {
                dap.Write(SET_COMMAND, 1, 0x00);
                return 3;
            }
            Console.WriteLine("streaming {0} s ({1}), touch the keys...", seconds,
                              periodMs > 0 ? periodMs + " ms period" : "INT falling edge");

            int records = 0, errors = 0, overrunSum = 0, missedSum = 0, reads = 0, longGaps = 0, nonZero = 0, changes = 0;
            var errorCount = new System.Collections.Generic.SortedDictionary<byte, int>();
            uint firstTs = 0, lastTs = 0;
            double minGap = double.MaxValue, maxGap = 0;
            byte[] lastData = null;
            string lastKey = null;
            var sw = Stopwatch.StartNew();
            long nextPrint = 0;

            while (sw.ElapsedMilliseconds < seconds * 1000)
            {
                int overrun, missed;
                DapLinkTransport.StreamRecord[] recs = dap.StreamRead(out overrun, out missed);
                reads++;
                if (recs == null)
                {
                    break;
                }
                overrunSum += overrun;
                missedSum += missed;
                foreach (DapLinkTransport.StreamRecord r in recs)
                {
                    if (r.Status != 0)
                    {
                        int n;
                        errorCount.TryGetValue(r.Status, out n);
                        errorCount[r.Status] = n + 1;
                        errors++;
                        if (r.Data.Length >= 7)     // TIMEOUT debug data
                        {
                            Console.WriteLine("  {0}: state {1} ST 0x{2:X2} CR 0x{3:X2} widx {4} ridx {5} SCL {6} SDA {7}",
                                              DapLinkTransport.StatusText(r.Status), r.Data[0], r.Data[1], r.Data[2],
                                              r.Data[3], r.Data[4], r.Data[5], r.Data[6]);
                        }
                        continue;
                    }
                    if (records == 0)
                    {
                        firstTs = r.Timestamp;
                    }
                    else
                    {
                        double gap = unchecked(r.Timestamp - lastTs) * 1000.0 / tsClock;    // ms, 32-bit wrap safe
                        minGap = Math.Min(minGap, gap);
                        maxGap = Math.Max(maxGap, gap);
                        if (periodMs > 0 && gap > periodMs * 1.5)
                        {
                            longGaps++;
                        }
                    }
                    if (Array.Exists(r.Data, b => b != 0))
                    {
                        nonZero++;
                    }
                    string key = string.Format("{0:X2}{1:X2}{2:X2}",
                                               r.Data[KEY_OFFSET], r.Data[KEY_OFFSET + 1], r.Data[KEY_OFFSET + 2]);
                    if (key != lastKey)
                    {
                        Console.WriteLine("  KEY {0}", key);     // key change
                        lastKey = key;
                    }
                    lastTs = r.Timestamp;
                    if (lastData != null && !SameData(lastData, r.Data))
                    {
                        changes++;                       // touch IC updated its data
                    }
                    lastData = r.Data;
                    records++;
                }
                if (lastData != null && sw.ElapsedMilliseconds >= nextPrint)
                {
                    Console.WriteLine("  {0}", FormatDiffKey(lastData));     // every 500 ms
                    nextPrint = sw.ElapsedMilliseconds + 500;
                }
                Thread.Sleep(5);
            }
            dap.StreamStop();
            dap.Write(SET_COMMAND, 1, 0x00);                        // QuickTouch: NONE_CANCEL at the end

            double span = records > 1 ? unchecked(lastTs - firstTs) * 1000.0 / tsClock : 0;
            Console.WriteLine();
            Console.WriteLine("stream {0} ms x {1} s: {2} records ({3} StreamRead), errors {4}, overrun {5}, missed {6}",
                              periodMs, seconds, records, reads, errors, overrunSum, missedSum);
            foreach (var kv in errorCount)
            {
                Console.WriteLine("error         : {0} x {1}", DapLinkTransport.StatusText(kv.Key), kv.Value);
            }
            if (records > 1)
            {
                Console.WriteLine("interval      : avg {0:F2} ms, min {1:F2}, max {2:F2} (timestamp clock {3} Hz)",
                                  span / (records - 1), minGap, maxGap, tsClock);
                Console.WriteLine("long gaps     : {0} (> {1:F1} ms)", longGaps, periodMs * 1.5);
            }
            Console.WriteLine("non-zero data : {0} / {1} records", nonZero, records);
            if (records > 1 && changes > 0)
            {
                Console.WriteLine("data changes  : {0} = touch IC update about every {1:F1} ms", changes, span / changes);
            }
            return errors == 0 && records > 0 ? 0 : 4;
        }
    }
}
