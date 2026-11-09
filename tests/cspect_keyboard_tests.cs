using System;
using Plugin;
using ZXQuestKeyboard;

public static class CSpectKeyboardTests
{
    private static int checks;
    private static void Check(bool condition, string name)
    {
        if (!condition) throw new Exception(name);
        ++checks;
    }
    public static int Main()
    {
        var bridge = new KeyboardBridge();
        var hooks = bridge.Init(null);
        Check(hooks.Count == 2 && hooks[0].Type == eAccess.KeyPress &&
              hooks[0].CMD == "\t" && hooks[0].ID == 1,
              "Tab uses the CSpect text-key registration API");
        Check(hooks[1].Type == eAccess.NextReg_Read && hooks[1].Port == 0xb1,
              "Bridge supplies the extended-key register");
        bool valid;
        Check(bridge.Read(eAccess.NextReg_Read, 0xb1, 0xb1, out valid) == 0 && valid,
              "Idle register is clear");
        Check(!bridge.KeyPressed(99), "Unregistered keys pass through");
        Check(bridge.KeyPressed(1), "Tab event is consumed");
        Check(bridge.Read(eAccess.NextReg_Read, 0xb1, 0xb1, out valid) == 1 && valid,
              "Pending Tab survives until the emulated CPU reads it");
        Check(bridge.Read(eAccess.NextReg_Read, 0xb1, 0xb1, out valid) == 0 && valid,
              "Event is consumed exactly once");
        bridge.KeyPressed(1);
        bridge.Reset();
        Check(bridge.Read(eAccess.NextReg_Read, 0xb1, 0xb1, out valid) == 0,
              "Reset clears pending input");
        bridge.Read(eAccess.Port_Read, 0xb1, 0xb1, out valid);
        Check(!valid, "Unregistered access passes through");
        Console.WriteLine("CSpect keyboard bridge: {0} checks passed", checks);
        return 0;
    }
}
