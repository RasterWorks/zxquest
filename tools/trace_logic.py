"""Read-only AGI v2 LOGIC disassembly for interpreter regression work."""
from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
OPS = [(int(n), name) for n, name in re.findall(r'(\d+), /\* [0-9a-f]{2} ([^*]+) \*/', (ROOT/'src/agi/opcodes.c').read_text())]
TESTS = [0,2,2,2,2,2,2,1,1,1,2,5,1,0,0,2,5,5,5]

def resource(number, game='sq1'):
    root=ROOT/'games'/game
    entry=int.from_bytes((root/'LOGDIR').read_bytes()[number*3:number*3+3], 'big')
    if entry & 0xfffff == 0xfffff: return None
    vol=(root/f'VOL.{entry>>20}').read_bytes(); at=(entry&0xfffff)+5
    return vol[at:at+int.from_bytes(vol[at-2:at], 'little')]

def messages(data):
    at=2+int.from_bytes(data[:2],'little'); count=data[at]; start=at+3+count*2
    text=bytes(c^b'Avis Durgan'[i%11] for i,c in enumerate(data[start:]))
    for i in range(count):
        off=int.from_bytes(data[at+3+i*2:at+5+i*2], 'little')
        yield i+1, text[at+1+off-start:].split(b'\0')[0].decode('ascii',errors='replace') if off else ''

def commands(data):
    code=data[2:2+int.from_bytes(data[:2],'little')]; at=0
    while at<len(code):
        pos=at; op=code[at];at+=1
        if op==255:
            start=at
            while code[at]!=255:
                test=code[at];at+=1
                if test in (252,253):continue
                if test==14: n=1+code[at]*2
                else:n=TESTS[test]
                at+=n
            tests=code[start:at];at+=1
            jump=int.from_bytes(code[at:at+2],'little');at+=2
            yield pos,'if',f'{tests.hex(" ")} else -> {at+jump}'
        elif op==254:
            jump=int.from_bytes(code[at:at+2],'little',signed=True);at+=2
            yield pos,'goto',str(at+jump)
        else:
            count,name=OPS[op];args=code[at:at+count];at+=count
            yield pos,name,','.join(map(str,args))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('logic',type=int,nargs='+');args=parser.parse_args()
    for number in args.logic:
        data=resource(number);print(f'LOGIC {number}')
        for pos,name,params in commands(data):print(f'{pos:4} {name}({params})')
        for i,msg in messages(data):print(f'M{i}: {msg}')
