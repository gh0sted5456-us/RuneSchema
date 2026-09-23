"""Render a *.draws capture with stand-in artwork. Not an Unreal screenshot.
Run RenderHelpyV10.cpp in a temporary/output directory first. Requires Pillow and
an installed DejaVu Sans font. No font files are distributed with the patch.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
for source in root.glob('*.draws'):
 im=Image.new('RGBA',(680,762),(28,27,24,255));d=ImageDraw.Draw(im)
 font=ImageFont.truetype(fontpath,12)
 d.text((12,7),'OFFLINE LAYOUT CHECK | Placeholder icons; not a game screenshot',font=font,fill=(220,211,191))
 violations=[]
 for line in source.read_text().splitlines():
  row=line.split('|');kind=int(row[0]);x,y,w,h,size=map(float,row[1:6]);y+=30;center=row[6]=='1';rgba=tuple(round(float(n)*255) for n in row[7:11]);text=bytes.fromhex(row[11]).decode();fallback=bytes.fromhex(row[12]).decode() if len(row)>12 else '?'
  overlay=Image.new('RGBA',im.size,(0,0,0,0));o=ImageDraw.Draw(overlay)
  if kind==0 and w>0 and h>0:o.rectangle((x,y,x+w,y+h),fill=rgba)
  elif kind==1:
   f=ImageFont.truetype(fontpath,max(1,round(size)));length=o.textlength(text,font=f);xx=x-length/2 if center else x
   o.text((xx,y-2),text,font=f,fill=rgba)
   if xx<0 or xx+length>680:violations.append((text,round(xx,1),round(xx+length,1)))
  elif kind in (2,3):
   o.rounded_rectangle((x,y,x+w,y+h),radius=min(w,h)*.12,fill=(59,55,43,rgba[3]),outline=(132,113,74,rgba[3]),width=1)
   label='ITEM' if kind==2 else fallback
   f=ImageFont.truetype(fontpath,min(14,round(w/2)));length=o.textlength(label,font=f);o.text((x+w/2-length/2,y+h/2-8),label,font=f,fill=rgba)
  im=Image.alpha_composite(im,overlay)
 im.convert('RGB').save(source.with_suffix('.png'))
 print(source.name,'horizontal text bounds:',violations)
