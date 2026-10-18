"""Original geometric icon animation frames; no external artwork or GIF decoder."""
from pathlib import Path
from PIL import Image,ImageDraw
import math
ROOT=Path(__file__).resolve().parents[1]
COLORS=[(14,148,130),(24,139,213),(235,120,58),(113,92,213),(23,160,173)]
NAMES=['fan','cool','heat','humid','dry']
size=56;scale=3
frames=[]
for k,name in enumerate(NAMES):
    arrays=[]
    for f in range(12):
        im=Image.new('RGBA',(size*scale,size*scale));d=ImageDraw.Draw(im);c=COLORS[k]
        def line(pts,color=c,w=2):d.line([(int(x*scale),int(y*scale)) for x,y in pts],fill=color,width=w*scale,joint='curve')
        def circle(x,y,r,color=c):d.ellipse(((x-r)*scale,(y-r)*scale,(x+r)*scale,(y+r)*scale),fill=color)
        phase=f/12
        if k==0:
            for blade in range(4):
                a=blade*math.pi/2+phase*math.pi/2
                pts=[]
                for r,t in [(5,0),(16,-.2),(23,.1),(21,.65),(10,.75)]:pts.append((28+r*math.cos(a+t),28+r*math.sin(a+t)))
                d.polygon([(x*scale,y*scale) for x,y in pts],fill=c)
            circle(28,28,5)
        elif k==1:
            for n in range(6):
                a=n*math.pi/3
                for j in range(1,5):
                    intensity=.3+.7*max(0,1-abs(j/4-phase)*2)
                    col=tuple(int(210+(x-210)*intensity) for x in c)
                    line([(28+(j-1)*5*math.cos(a),28+(j-1)*5*math.sin(a)),(28+j*5*math.cos(a),28+j*5*math.sin(a))],col,2)
                for t in [-.65,.65]:line([(28+13*math.cos(a),28+13*math.sin(a)),(28+13*math.cos(a)+8*math.cos(a+t),28+13*math.sin(a)+8*math.sin(a+t))],c,2)
            circle(28,28,2)
        elif k==2:
            for x in [16,28,40]:
                pts=[(x+3*math.sin((y/24-phase)*math.tau),y) for y in range(10,44)]
                line(pts,c,3)
            line([(10,48),(46,48)],c,2)
        elif k==3:
            line([(17,46),(39,46)],c,3)
            for n in range(6):
                p=(phase+n/6)%1;x=28+math.sin(n*2.4)*(7+12*p);y=40-p*30
                col=(*c,int(255*(1-p*.75)));circle(x,y,2+1.6*p,col)
        else:
            for n in range(3):
                p=(phase+n/3)%1;x=16+n*12;y=10+25*p
                d.polygon([(x*scale,(y-5)*scale),((x-3)*scale,(y+1)*scale),((x+3)*scale,(y+1)*scale)],fill=(*c,int(255*(1-p*.4))))
                circle(x,y+1,3,(*c,int(255*(1-p*.4))))
            line([(12,41),(12,47),(44,47),(44,41)],c,2)
        im=im.resize((size,size),Image.Resampling.LANCZOS)
        out=ROOT/'assets/icons'/f'{name}_{f:02}.png';out.parent.mkdir(parents=True,exist_ok=True);im.save(out)
        # LVGL ARGB8888 little-endian bytes, native image source.
        raw=bytes(v for r,g,b,a in im.getdata() for v in (b,g,r,a))
        arrays.append(raw)
    frames.append(arrays)
out=['#include "ui/animations.h"']
for k,name in enumerate(NAMES):
 for f,raw in enumerate(frames[k]):
  out.append(f'static const uint8_t data_{name}_{f}[] = {{'+','.join(str(b) for b in raw)+'};')
  out.append(f'static const lv_image_dsc_t img_{name}_{f} = {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=56,.h=56,.stride=224}},.data_size=12544,.data=data_{name}_{f}}};')
out.append('const lv_image_dsc_t *const monitor_frames[5][12] = {'+','.join('{'+','.join(f'&img_{name}_{f}' for f in range(12))+'}' for name in NAMES)+'};')
(ROOT/'src/ui/animations.c').write_text('\n'.join(out))
(ROOT/'include/ui/animations.h').write_text('#pragma once\n#include "lvgl.h"\nextern const lv_image_dsc_t *const monitor_frames[5][12];\n')
print('60 original animation frames generated')
