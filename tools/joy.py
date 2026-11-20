#!/usr/bin/env python3
"""Builds a MP_JOY script from tokens: joy.py <start_frame> "U U R A ..." -> "f:mask,f:mask,..."
Tokens: L R U D A B X Y (buttons), S=START, E=SELECT, 1/2=L1/R1, '.'=wait 4 frames, 'wN'=wait N frames."""
import sys
M={'L':1,'R':2,'U':4,'D':8,'A':16,'B':32,'1':64,'2':128,'X':256,'Y':512,'S':1024,'E':2048}
f=int(sys.argv[1]); out=[]
for tok in sys.argv[2].split():
    if tok=='.': f+=4; continue
    if tok[0]=='w': f+=int(tok[1:]); continue
    mask=0
    for c in tok: mask|=M[c]
    out.append(f"{f}:{mask}"); out.append(f"{f+2}:0"); f+=5
print(",".join(out))
