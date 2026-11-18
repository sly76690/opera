# OPERA icon, in the family of the O6, O8, NO and OX badges: "OP", the ring O with its satellite, then a condensed P.
# python3 tools/opera-logo.py  -> Source/Assets/orbit-icon.png (1024 px, the app icon and the panel badge)
from PIL import Image, ImageDraw, ImageFilter
import os
S=4; W=1024*S
night=(200,69,95,255); grey=(200,206,212,255); dark=(29,30,31,255); edge=(58,58,60,255) # night = the carmine
def r(v): return int(v*S)
im=Image.new('RGBA',(W,W),(0,0,0,0))
sh=Image.new('RGBA',(W,W),(0,0,0,0)); ImageDraw.Draw(sh).rounded_rectangle([r(102),r(114),r(922),r(934)],radius=r(176),fill=(0,0,0,70))
im=Image.alpha_composite(im,sh.filter(ImageFilter.GaussianBlur(r(6))))
box=[r(102),r(102),r(920),r(920)]
tile=Image.new('RGBA',(W,W),(0,0,0,0)); t=ImageDraw.Draw(tile)
t.rectangle([0,0,W,r(730)],fill=dark); t.rectangle([0,r(730),W,r(826)],fill=grey); t.rectangle([0,r(826),W,W],fill=night)
mask=Image.new('L',(W,W),0); ImageDraw.Draw(mask).rounded_rectangle(box,radius=r(176),fill=255)
im.paste(tile,(0,0),mask); d=ImageDraw.Draw(im); d.rounded_rectangle(box,radius=r(176),outline=edge,width=r(5))
# the O: the ring in light grey, its satellite in carmine with a dark halo (where the 6 and the 8 have theirs)
cx,cy=377,421
d.ellipse([r(cx-167),r(cy-167),r(cx+167),r(cy+167)],fill=grey); d.ellipse([r(cx-110),r(cy-110),r(cx+110),r(cy+110)],fill=dark)
d.ellipse([r(474-44),r(324-44),r(474+44),r(324+44)],fill=dark); d.ellipse([r(474-28),r(324-28),r(474+28),r(324+28)],fill=night)
# the P: a condensed stem and a rounded bowl, same height and stroke as the 6 and the 8, in carmine
x0,x1,top,bot,st=585,748,258,582,54
d.rounded_rectangle([r(x0),r(top),r(x0+st),r(bot)],radius=r(14),fill=night)
d.rounded_rectangle([r(x0),r(top),r(x1),r(top+186)],radius=r(80),fill=night)
d.rounded_rectangle([r(x0+st),r(top+st),r(x1-st),r(top+186-st)],radius=r(30),fill=dark)
root=os.path.join(os.path.dirname(__file__),'..')
im.resize((1024,1024),Image.LANCZOS).save(os.path.join(root,'Source/Assets/orbit-icon.png'))

# the knob index line of the carmine sections
idx=Image.open(os.path.join(root,'Source/Assets/skin/index_amber.png')).convert('RGBA');px=idx.load()
for y in range(idx.height):
    for x in range(idx.width):
        rr,gg,bb,a=px[x,y]
        if a>8: l=(rr+gg+bb)/3/255*1.5; px[x,y]=(min(255,int(235*l)),min(255,int(95*l)),min(255,int(120*l)),a)
idx.save(os.path.join(root,'Source/Assets/skin/index_teal.png'))
