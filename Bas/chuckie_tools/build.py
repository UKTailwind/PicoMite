"""Glue the code, sound table, sprite bitmaps and level maps into chuckie.bas."""
import subprocess, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
PY = r'D:/Dropbox/PicoMite/PicoMite/.venv-1/Scripts/python.exe'


def run(script, args=()):
    r = subprocess.run([PY, os.path.join(HERE, script)] + list(args),
                       capture_output=True, text=True, cwd=HERE)
    if r.returncode:
        sys.exit(script + ' failed:\n' + r.stderr)
    return r.stdout, r.stderr


spr, _ = run('gen_sprites.py')
sprites = [l for l in spr.split('\n') if l.startswith('DATA')]
lev, leverr = run('gen_levels.py')
if 'problems' in leverr and not leverr.strip().endswith('0 level(s) with problems'):
    print(leverr, file=sys.stderr)

code = open(os.path.join(HERE, 'chuckie_code.bas'), encoding='utf-8').read().rstrip('\n')
sfx = open(os.path.join(HERE, 'sfxdata.bas'), encoding='utf-8').read().rstrip('\n')

bar = "' " + '=' * 70
out = [code, sfx, '', bar,
       "'  Sprite bitmaps lifted from the BBC Micro title screen.",
       "'    DATA width, height, colour index, hex bitmap",
       bar, 'sprdata:'] + sprites
out += ['', bar,
        "'  The eight floors.  20 columns x 28 rows, one cell = 16 x 8 pixels.",
        "'    .  empty          #  girder        H  ladder",
        "'    E  egg            S  grain         L  lift shaft",
        "'    P  Harry starts   1-4  a hen starts",
        bar, 'lvldata:', lev.rstrip('\n'), '']

dest = os.path.join(HERE, 'chuckie.bas')
open(dest, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print('chuckie.bas: %d lines, %d bytes' %
      (sum(1 for _ in open(dest, encoding='utf-8')), os.path.getsize(dest)))
