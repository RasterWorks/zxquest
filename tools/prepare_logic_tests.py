"""Generate authored bytecode fixtures and independent SQ1 message checksums.

Writes build/ only; no game bytes are embedded in test executable sources.
Run from any directory. Uses the checked-in opcode table, not an upstream checkout.
"""
from pathlib import Path
import binascii
import json
import re

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
GAME = ROOT / "games/sq1"
KEY = b"Avis Durgan"


def logic(code, messages=()):
    table, text = bytearray(), bytearray()
    for msg in messages:
        if msg is None:
            table += b"\0\0"
        else:
            table += (2 + 2 * len(messages) + len(text)).to_bytes(2, "little")
            text += msg + b"\0"
    size = 2 + len(table) + len(text)
    encrypted = bytes(b ^ KEY[i % 11] for i, b in enumerate(text))
    return len(code).to_bytes(2, "little") + bytes(code) + bytes([len(messages)]) + size.to_bytes(2, "little") + table + encrypted


class Code:
    def __init__(self):
        self.data, self.labels, self.patches = bytearray(), {}, []

    def emit(self, *values):
        self.data.extend(values)
        return self

    def label(self, name):
        self.labels[name] = len(self.data)
        return self

    def branch(self, label, tests=None):
        self.emit(254 if tests is None else 255)
        if tests is not None:
            self.data.extend(tests)
            self.emit(255)
        self.patches.append((len(self.data), label, tests is None))
        self.emit(0, 0)
        return self

    def finish(self):
        for at, label, signed in self.patches:
            self.data[at:at+2] = (self.labels[label] - at - 2).to_bytes(2, "little", signed=signed)
        return self.data


def fixtures():
    cases = {}
    def add(n, *code):
        cases[n] = logic(code)
    add(0, 3,0,255, 1,0, 3,1,0, 2,1, 5,0,2, 7,1,1,
        3,2,20, 3,3,7, 9,2,3, 10,4,2, 11,2,9, 4,5,20,
        6,4,5, 8,4,3, 3,255,255, 12,255, 14,254, 13,254,
        15,255, 17,255, 16,255, 12,255, 0)
    c=Code().emit(3,0,7,3,1,7)
    c.branch('fail', [2,0,1,3,0,8,4,0,1,5,0,6,6,0,1])  # deliberately false (< equal)
    c.emit(3,2,99).label('fail')
    c.branch('end', [252,1,0,7,13,14,2,255,255,252,253,252,253,1,0,8])
    c.emit(3,2,42).label('end').emit(0)
    cases[1]=logic(c.finish())
    add(2, 22,4, 3,9,3, 23,9, 20,3, 21,9, 0)
    add(3, 1,42, 0)
    add(4, 22,3, 1,43, 0)
    add(5, 1,1, 0x91, 1,2, 0)
    add(6, 1,1, 0x91, 1,2, 0x92, 0)
    c=Code().label('loop').emit(1,0).branch('end',[3,0,10]).branch('loop').label('end').emit(0)
    cases[7]=logic(c.finish())
    add(8,254,253,255)  # goto itself
    add(9,22,9,0)
    add(10,22,11,3,1,99,0)
    add(11,3,2,123,19,2,0)
    add(12,3,0)  # truncated assign
    add(13,183,0)
    add(14,254,0,128)
    add(15,255,1,0)  # truncated condition
    add(16,255,19,255,0,0,0)
    add(17,255,252,7,1,255,0,0,0)  # unclosed OR
    cases[18]=b'\xff\xff\x00'
    add(19,22,255,0)
    add(20,254,255,127)
    c=Code().branch('end',[1,0,1,14,2,255,255,252,253]).emit(3,1,99).label('end').emit(0)
    cases[21]=logic(c.finish())
    c=Code().branch('end',[14,2,255,255,252,253]).emit(3,1,42).label('end').emit(0)
    cases[22]=logic(c.finish())
    c=Code().branch('far')
    c.data.extend([183]*(8189-len(c.data)))
    c.label('far').emit(3,255,41,22,3,1,255,0)
    cases[23]=logic(c.finish())  # assign opcode at resource offset 8191
    add(24,0x65,1,0)  # explicit unsupported print
    add(25,255,13,255,0,0,0)  # unsupported have.key
    add(26,3,0,9,0xa5,0,3,0xa7,0,2,3,1,2,0xa6,0,1,0xa8,0,1,0)
    add(27,0xa7,0,0,0)
    add(28,255,253,255,0,0,0)  # dangling NOT
    add(29,255,14,255,0)  # said count exceeds code
    add(30,18,77,0)
    cases[31]=logic([0], [b'A'*8181,b'Cross-bank message',None,b''])
    add(32,0x90,0,22,45,0x90,0,0)  # callbacks cause cache pressure
    add(33,255,1,255,0)  # operand present, closing token missing
    add(34,3,0,1,0x18,0,0x1e,1,0x1f,0,0x62,1,0)
    add(35,0x18,0,0)  # load missing picture fixture
    cases[36]=logic([0], [b'bad'])[:-1]  # message end exceeds resource
    cases[37]=logic([0],[b'bad'])[:-1]+bytes([ord('X')^KEY[3]])  # unterminated
    add(38,255,1,0,0,255,255,127,0)  # true IF does not take huge skip
    add(39,255,1,0,1,255,255,127,0)  # false IF rejects huge skip
    c=Code().emit(3,0,7,3,1,8,3,2,7,3,3,6,3,4,255,12,255)
    c.branch('end',[1,0,7,2,0,2,3,0,8,4,0,1,5,0,6,6,0,3,7,255,8,4])
    c.emit(3,5,42).label('end').emit(0)
    cases[40]=logic(c.finish())
    c=Code().branch('end',[252,1,0,1,1,0,2,252]).emit(3,1,99).label('end').emit(0)
    cases[41]=logic(c.finish())
    add(42,3,0,255,0xa5,0,255,3,1,255,3,2,255,0xa6,1,2,0)
    bad=bytearray(logic([0],[b'bad']));bad[6:8]=b'\x01\x00';cases[43]=bad
    bad=bytearray(logic([0],[b'bad']));bad[3]=255;cases[44]=bad
    add(45,0x90,0,1,42,0)
    # An opcode occupies the final byte of a 64-byte fetch window. Its
    # operands must refill correctly even when every token yields.
    cases[46]=logic([3,70,0]*21+[3,255,42,1,255,0])
    c=Code().emit(*([3,70,0]*20)).branch('end',[7,3]).emit(3,71,99).label('end').emit(0)
    cases[47]=logic(c.finish())  # closing IF token at byte 65, skip crosses window
    c=Code().emit(*([3,70,0]*19),1,70).branch('end',[14,2,255,255,252,253]).emit(3,72,42).label('end').emit(0)
    cases[48]=logic(c.finish())  # said list crosses window, includes delimiters
    out=BUILD/'logic-fixtures';out.mkdir(parents=True,exist_ok=True)
    directory=bytearray(b'\xff'*768);volume=bytearray()
    for n,payload in sorted(cases.items()):
        directory[n*3:n*3+3]=len(volume).to_bytes(3,'big')
        volume+=b'\x12\x34\x00'+len(payload).to_bytes(2,'little')+payload
    (out/'LOGDIR').write_bytes(directory);(out/'VOL.0').write_bytes(volume)
    for name in ('PICDIR','VIEWDIR','SNDDIR'):
        (out/name).write_bytes(b'\xff'*768)


def main():
    BUILD.mkdir(exist_ok=True)
    fixtures()
    metadata=re.findall(r'(\d+), /\* ([0-9a-f]+) (.*?) \*/',(ROOT/'src/agi/opcodes.c').read_text())
    args=[int(n) for n,_,_ in metadata]; names=[s for _,_,s in metadata]
    assert len(args)==183
    tests=[0,2,2,2,2,2,2,1,1,1,2,5,1,0,0,2,5,5,5]
    entries=(GAME/'LOGDIR').read_bytes();records=[];inventory=[]
    for number in range(len(entries)//3):
        entry=int.from_bytes(entries[number*3:number*3+3],'big')
        if entry&0xfffff==0xfffff:continue
        vol=(GAME/f'VOL.{entry>>20}').read_bytes();at=entry&0xfffff
        assert vol[at:at+3]==bytes([18,52,entry>>20])
        size=int.from_bytes(vol[at+3:at+5],'little');raw=vol[at+5:at+5+size]
        end=int.from_bytes(raw[:2],'little')+2;count=raw[end]
        limit=end+1+int.from_bytes(raw[end+1:end+3],'little');start=end+3+count*2
        assert start<=limit<=size
        plain=bytes(b^KEY[i%11] for i,b in enumerate(raw[start:limit]))
        crc=0xffff; chars=0
        for m in range(count):
            off=int.from_bytes(raw[end+3+2*m:end+5+2*m],'little')
            msg=b'' if not off else plain[end+1+off-start:].split(b'\0',1)[0]
            if off:
                assert start<=end+1+off<limit and b'\0' in plain[end+1+off-start:]
            crc=binascii.crc_hqx(msg+b'\0',crc);chars+=len(msg)+1
        records.append((number,count,chars,crc))
        pc=2; commands={};boundaries=set();branches=[]
        while pc<end:
            boundaries.add(pc);op=raw[pc];pc+=1
            if op==255:
                while raw[pc]!=255:
                    t=raw[pc];pc+=1
                    if t in (252,253):continue
                    assert 0<t<len(tests),(number,pc,t)
                    pc+=1+2*raw[pc] if t==14 else tests[t]
                pc+=1;delta=int.from_bytes(raw[pc:pc+2],'little');pc+=2;branches.append(pc+delta)
            elif op==254:
                delta=int.from_bytes(raw[pc:pc+2],'little',signed=True);pc+=2;branches.append(pc+delta)
            else:
                assert op<len(args),(number,pc,op)
                commands[names[op]]=commands.get(names[op],0)+1;pc+=args[op]
        assert pc==end,(number,pc,end)
        assert all(b in boundaries for b in branches),(number,[b for b in branches if b not in boundaries])
        inventory.append(dict(logic=number,code_bytes=end-2,messages=count,commands=commands))
    with (BUILD/'logic_manifest.h').open('w') as out:
        out.write('/* Generated checksums only; no game text/code. */\nstatic const struct logic_expected { uint8_t number, messages; uint16_t bytes, crc; } expected[] = {\n')
        for n,m,size,crc in records:out.write(f'    {{{n},{m},{size},0x{crc:04x}}},\n')
        out.write('};\n')
    (BUILD/'logic-inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
    print(f'{len(records)} LOGICs structurally checked, {sum(x[1] for x in records)} messages, {sum(x[2] for x in records)} plaintext bytes including terminators')


if __name__=='__main__':main()
