using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Text.RegularExpressions;
using Plugin;
[assembly: System.Reflection.AssemblyTitle("ZXQuestKeyboard")]
[assembly: System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.5.2")]

// CSpect BASIC mode provides useful arrow/Escape mappings, but its synthetic
// Extend matrix pulse can disappear between AGI frames. Latch the host Tab
// event until the emulated CPU reads the real Next extended-key register.
namespace ZXQuestKeyboard {
public sealed class KeyboardBridge : iPlugin
{
    private int pending;
    private string log;
    private iCSpect host;
    private ushort keyAddress;
    private int lastKey = -1;
    private ushort queueAddress, headAddress;
    private int observedHead;
    public List<sIO> Init(iCSpect cspect)
    {
        host = cspect;
        log = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "zxquest-keyboard.log");
        File.WriteAllText(log, "ZXQuest keyboard bridge loaded\n");
        string map = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", "runtime.map");
        if (host != null && File.Exists(map)) {
            Match match = Regex.Match(File.ReadAllText(map), @"(?m)^_next_key_previous\s+= \$([0-9A-Fa-f]+)");
            if (match.Success) keyAddress = Convert.ToUInt16(match.Groups[1].Value, 16);
            string symbols = File.ReadAllText(map);
            Match queue = Regex.Match(symbols, @"(?m)^_keys\s+= \$([0-9A-Fa-f]+)");
            Match head = Regex.Match(symbols, @"(?m)^_next_key_head\s+= \$([0-9A-Fa-f]+)");
            if (queue.Success && head.Success) {
                queueAddress = Convert.ToUInt16(queue.Groups[1].Value, 16);
                headAddress = Convert.ToUInt16(head.Groups[1].Value, 16);
            }
        }
        return new List<sIO> {
            // KeyPress hooks use text commands; the integer overload registers
            // ports, not OpenTK key enum values. The callback receives our ID.
            new sIO("\t", eAccess.KeyPress, 1, 0),
            new sIO(0xb1, eAccess.NextReg_Read, 0xb1, 0)
        };
    }
    public bool KeyPressed(int key)
    {
        if (key != 1) return false;
        Interlocked.Exchange(ref pending, 1);
        File.AppendAllText(log, "Extend queued\n");
        return true;
    }
    public byte Read(eAccess access, int address, int id, out bool valid)
    {
        valid = false;
        if (access != eAccess.NextReg_Read) return 0;
        valid = true;
        if (Interlocked.Exchange(ref pending, 0) == 0) return 0;
        File.AppendAllText(log, "Extend read\n");
        return 1;
    }
    public bool Write(eAccess access, int address, int id, byte value) { return false; }
    public void Tick() {
        if (host == null || keyAddress == 0) return;
        if (headAddress != 0) {
            int head = host.Peek(headAddress) & 31;
            while (observedHead != head) {
                File.AppendAllText(log, "Queued key=" + host.Peek((ushort)(queueAddress + observedHead)) + "\n");
                observedHead = (observedHead + 1) & 31;
            }
        }
        int key = host.Peek(keyAddress);
        if (key != lastKey) {
            lastKey = key;
            File.AppendAllText(log, "Decoded key=" + key + "\n");
        }
    }
    public void OSTick() { }
    public void Reset() { Interlocked.Exchange(ref pending, 0); }
    public void Quit() { }
}
}



