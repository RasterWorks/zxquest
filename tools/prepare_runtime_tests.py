"""Generate runtime fixtures and sound-duration expectations under build/."""
from pathlib import Path
import json
from prepare_logic_tests import Code, logic, ROOT, BUILD, GAME, KEY


def resource(name, number):
    d=(GAME/name).read_bytes(); e=int.from_bytes(d[number*3:number*3+3],'big')
    v=(GAME/f'VOL.{e>>20}').read_bytes();at=e&0xfffff
    assert v[at:at+3]==bytes([18,52,e>>20])
    return v[at+5:at+5+int.from_bytes(v[at+3:at+5],'little')]


def main():
    folder=BUILD/'runtime-fixtures';folder.mkdir(parents=True,exist_ok=True)
    dirs={s:bytearray(b'\xff'*768) for s in ['LOGDIR','PICDIR','VIEWDIR','SNDDIR']};volume=bytearray()
    def add(kind,n,data):
        dirs[kind][3*n:3*n+3]=len(volume).to_bytes(3,'big')
        volume.extend(b'\x12\x34\x00'+len(data).to_bytes(2,'little')+data)
    c=Code().branch('events',[7,5])
    c.emit(0x96,95,3,10, 0x79,97,0,3, 0x79,98,0,3, 0x72,0,1, 0x8f,2,
           0x6f,1,22,0, 0x25,0,50,140, 0x21,0, 0x1e,1, 0x29,0,1,
           0x2b,0,0, 0x2f,0,0, 0x46,0, 0x18,0,0x19,0,0x23,0,0x1a)
    c.label('events').branch('key',[12,3]).emit(1,100)
    c.label('key').branch('end',[13]).emit(12,210).label('end').emit(0)
    add('LOGDIR',0,logic(c.finish(),[b'>',b'TEST']))
    add('LOGDIR',1,logic([0x65,1,3,200,17,0],[b'Hello %s1. Score %v3.']))
    add('LOGDIR',2,logic([22,1,1,201,0]))
    add('LOGDIR',3,logic([0x73,1,1,8,0,8,3,202,99,0],[b'Name: ']))
    c=Code().emit(0x9c,1,0x9d,2,10,0x9d,3,11,0x9d,4,12,0xa0,11,0x9e,0xa1)
    c.branch('end',[12,12]).emit(3,203,98).label('end').emit(0)
    add('LOGDIR',4,logic(c.finish(),
                         [b'File',b'First',b'Disabled',b'Last']))
    add('LOGDIR',5,logic([18,7,0]))
    # Script-level parser tests: success is reported in v205, never hardcoded
    # into the interpreter. Each logic contains one said pattern.
    for number,words in [(6,[2,22]),(7,[2,1]),(8,[2,9999]),(9,[2]),(10,[400])]:
        test=[14,len(words)]
        for word in words:test += [word&255,word>>8]
        c=Code().emit(3,205,0).branch('end',test).emit(3,205,1).label('end').emit(0)
        add('LOGDIR',number,logic(c.finish()))
    add('LOGDIR',11,logic([0],[b'%m2 "%w2"',b'Unknown',b'%m3',
                              b'\n'.join([b'Long dialogue line']*14),
                              b'\n'.join([b'Clipped dialogue']*24),b'W'*500,
                              b'You think you hear footsteps approaching from the passage behind you.',
                              b'123456789012345678901234567890123456\r\nNext line\n\nNew paragraph',
                              b'Prefix '+b'W'*76+b' end',
                              b'Welcome %s1! %m7',
                              b'  Indented text stays indented.\nA  pair   of spaces.\rLast line.',b'',
                              b'Line\n'*10+b'These words must stay visible below the old footer.',
                              b'123456789012345678901234567890123456 next\nend']))
    # Exercise automatic motion through the VM, with script-owned flags.
    add('LOGDIR',12,logic([0x53,1,3,210,0x54,2,3,205,99,0]))
    add('PICDIR',0,resource('PICDIR',1))
    add('VIEWDIR',1,resource('VIEWDIR',1))
    for n,colour in [(2,2),(3,3)]:
        add('VIEWDIR',n,bytes([1,1,1,0,0,7,0,1,3,0,1,1,15,(colour<<4)|1,0]))
    # Geometry changes must be selected before movement/collision.
    add('VIEWDIR',4,bytes([1,1,2,0,0,9,0,17,0,1,3,0,3,1,15,0x23,0,1,3,0,1,1,15,0x21,0]))
    add('VIEWDIR',5,bytes([1,1,1,0,0,7,0,2,5,0,10,0,1,1,15,0x21,0,3,1,15,0x23,0]))
    voices=[[3,5],[6],[],[]]; data=bytearray(8)
    for i,notes in enumerate(voices):
        data[2*i:2*i+2]=len(data).to_bytes(2,'little')
        for duration in notes:data+=duration.to_bytes(2,'little')+b'\x00\x10\x90'
        data+=b'\xff\xff'
    add('SNDDIR',1,data); add('SNDDIR',2,b'\xff'*8)
    for name,data in dirs.items():(folder/name).write_bytes(data)
    (folder/'VOL.0').write_bytes(volume)
    # Encrypted object table with two known room locations, no name lookup needed.
    obj=bytes([6,0,2,6,0,255,8,0,7])+b'a\0b\0'
    (folder/'OBJECT').write_bytes(bytes(b^KEY[i%11] for i,b in enumerate(obj)))
    words=[('a',0),('door',22),('examine',2),('look',2),('look at',2),('on',0),
           ('open',19),('pick',8),('pick up',400),('the',0),('up',0)]
    data=bytearray(52);previous=''
    for word,group in words:
        letter=ord(word[0])-97
        if not data[letter*2:letter*2+2].strip(b'\0'):data[letter*2:letter*2+2]=len(data).to_bytes(2,'big')
        prefix=0
        while prefix<min(len(previous),len(word)) and previous[prefix]==word[prefix]:prefix+=1
        encoded=bytearray(ord(ch)^127 for ch in word[prefix:]);encoded[-1]|=128
        data+=bytes([prefix])+encoded+group.to_bytes(2,'big');previous=word
    data+=b'\0';(folder/'WORDS.TOK').write_bytes(data)
    # Room lifecycle fixture: LOGIC 0 records entry coordinates/state once;
    # LOGIC 1/2 request immediate/variable room changes. No game data changes.
    rooms=BUILD/'room-fixtures';rooms.mkdir(exist_ok=True)
    c=Code().branch('end',[7,5]).emit(0x27,0,200,201,4,202,1,4,203,16,1,204).label('end').emit(0)
    room_volume=bytearray();room_index=bytearray()
    for payload in [logic(c.finish()),logic([0x12,7,0]),logic([0x13,205,0])]:
        room_index+=len(room_volume).to_bytes(3,'big')
        room_volume+=b'\x12\x34\x00'+len(payload).to_bytes(2,'little')+payload
    (rooms/'LOGDIR').write_bytes(room_index);(rooms/'VOL.0').write_bytes(room_volume)
    for name in ['PICDIR','VIEWDIR','SNDDIR']:(rooms/name).write_bytes(b'\xff'*3)
    for name in ['OBJECT','WORDS.TOK']:(rooms/name).write_bytes((folder/name).read_bytes())
    # Independent host expansion of every original dictionary entry. The
    # expected words and IDs remain external test data, not compiled assets.
    raw=(GAME/'WORDS.TOK').read_bytes();at=52;word=b'';records=bytearray();ids=bytearray()
    while at<len(raw):
        prefix=raw[at];at+=1
        if at==len(raw) and prefix==0:break
        assert prefix<=len(word)
        word=word[:prefix]
        while True:
            c=raw[at];at+=1;word+=bytes([(c^127)&127])
            if c&128:break
        group=int.from_bytes(raw[at:at+2],'big');at+=2
        assert len(word)<=40
        records+=bytes([len(word)])+word+group.to_bytes(2,'little');ids+=group.to_bytes(2,'little')
    parser_folder=BUILD/'parser-fixtures';parser_folder.mkdir(exist_ok=True)
    (BUILD/'parser-words.bin').write_bytes(records)
    payload=logic(bytes([0])+ids)
    (parser_folder/'LOGDIR').write_bytes(b'\0\0\0')
    (parser_folder/'VOL.0').write_bytes(b'\x12\x34\x00'+len(payload).to_bytes(2,'little')+payload)
    for name,raw in [('bad-prefix',b'\0\x34'+bytes(50)+b'\x05\xe1\0\x02'),
                     ('truncated',b'\0\x34'+bytes(50)+b'\0\xe1'),
                     ('bad-header',b'\0\x01'+bytes(50))]:
        bad=BUILD/('parser-'+name);bad.mkdir(exist_ok=True);(bad/'WORDS.TOK').write_bytes(raw)
    durations=[]
    index=(GAME/'SNDDIR').read_bytes()
    for n in range(len(index)//3):
        e=int.from_bytes(index[3*n:3*n+3],'big')
        if e&0xfffff==0xfffff or n==0:continue
        data=resource('SNDDIR',n); totals=[]
        starts=[int.from_bytes(data[i:i+2],'little') for i in range(0,8,2)]
        for at in starts:
            total=0
            while True:
                assert at+2<=len(data)
                duration=int.from_bytes(data[at:at+2],'little')
                if duration==65535:break
                assert at+5<=len(data)
                total+=duration;at+=5
            totals.append(total)
        durations.append((n,max(totals)))
    with (BUILD/'runtime_manifest.h').open('w') as out:
        out.write('/* Generated sound timing only, no music data. */\nstatic const struct sound_expected { uint8_t number; uint32_t ticks; } expected_sounds[]={\n')
        for n,t in durations:out.write(f'    {{{n},{t}UL}},\n')
        out.write('};\n')
    print(f'Runtime fixtures written; {len(durations)} sound durations independently checked')


if __name__=='__main__':main()
