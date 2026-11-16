"""Create isolated discovery fixtures; never touch player saves or configs."""
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build'/'launcher-sd'
def game(folder,name):
    folder.mkdir(parents=True,exist_ok=True)
    for p in (ROOT/'games'/name).iterdir():
        if p.name.upper() in ('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK') or p.name.upper().startswith('VOL.'):
            shutil.copyfile(p,folder/p.name)
def main():
    for name in ('empty','invalid','explicit','auto'):(OUT/name).mkdir(parents=True,exist_ok=True)
    game(OUT/'single'/'SQ1','sq1')
    game(OUT/'direct','sq1')
    game(OUT/'stale','sq1');(OUT/'stale'/'ZXQUEST.CFG').write_text('games/sq1\n')
    for g in ('sq1','kq1','pq1'):game(OUT/'multi'/g.upper(),g)
    game(OUT/'containers'/'games'/'SQ1','sq1')
    game(OUT/'legacy'/'OLD'/'games'/'pq1','pq1')
    (OUT/'legacy'/'OLD'/'ZXQUEST.CFG').write_text('games/pq1\n')
    game(OUT/'long'/'Game With A Long Name','sq1')
    (OUT/'explicit'/'ZXQUEST.CFG').write_text('../multi/PQ1\r\n')
    game(OUT/'auto'/'SQ1','sq1');(OUT/'auto'/'ZXQUEST.CFG').write_text('AUTO\n')
    for i in range(33):
        d=OUT/'many'/f'G{i:02}';d.mkdir(parents=True,exist_ok=True)
        for f in ('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK'):(d/f).write_bytes(b'\0\0\0')
        (d/'VOL.0').write_bytes(b'\x12\x34\0\3\0\1\0\0')
    for name in ('incomplete','broken'):
        d=OUT/'invalid'/name;d.mkdir(parents=True,exist_ok=True)
        for f in ('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK'):(d/f).write_bytes(b'\0\0\0' if name=='broken' else b'')
        (d/'VOL.0').write_bytes(b'BAD HEADER')
    print(f'Launcher fixtures: {OUT}')
if __name__=='__main__':main()
