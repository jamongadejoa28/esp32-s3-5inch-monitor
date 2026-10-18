"""Use the live Windows handshake; never persist or print a bridge token."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
script=root/'tools'/(sys.argv[1] if len(sys.argv)>1 and sys.argv[1].endswith('.mjs') else 'eez.mjs')
args=sys.argv[2:] if script.name!='eez.mjs' else sys.argv[1:]
win=subprocess.check_output(['wslpath','-w',str(script)],text=True).strip()
raise SystemExit(subprocess.call(['/mnt/c/Program Files/nodejs/node.exe',win,*args],cwd=root))
