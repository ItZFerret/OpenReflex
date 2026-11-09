"""Semantic checks: authored palette/UV placement and surface identities survive baking."""
import struct
import numpy as np
from surfaces import SurfaceBake

# Triangle covers exactly four known terrain pixels. Vertex lighting is neutral.
raw=bytearray(3*48)
for i,(pos,uv) in enumerate(zip(((-2047,0,2047),(-2043,0,2047),(-2047,0,2043)),((.25,.25),(1.25,.25),(.25,1.25)))):
 struct.pack_into('<3f',raw,i*48,*pos)
 for layer in range(4):struct.pack_into('<2f',raw,i*48+12+layer*8,*uv)
 raw[i*48+44:i*48+48]=bytes([128,128,128,64])
mat=bytearray(34);struct.pack_into('<I',mat,0,1);struct.pack_into('<3f',mat,12,1,1,1)
# Unequal colors catch palette substitution and horizontal/vertical inversion.
tex=np.array([[[100,20,10,255],[20,100,10,255]],[[10,20,100,255],[90,80,70,255]]],np.uint8)
a=SurfaceBake(np.zeros(3),{1:(0,0,0),2:(1,0,0)})
a.draw((mat,[]),raw,np.array([[0,1,2]]),{1:(2,2,tex.tobytes())})
assert np.allclose(a.colour[0,0,:3],[100,20,10],atol=1)
assert np.allclose(a.colour[0,1,:3],[20,100,10],atol=1)
assert np.allclose(a.colour[1,0,:3],[10,20,100],atol=1)
assert np.allclose(a.colour[0,2,:3],[100,20,10],atol=1) # repeated U
assert a.pixels[0,0,3]==255 and a.pixels[0,0,0]==0
mat[10]=1
clamped=SurfaceBake(np.zeros(3),{1:(0,0,0)})
clamped.draw((mat,[]),raw,np.array([[0,1,2]]),{1:(2,2,tex.tobytes())})
assert np.allclose(clamped.colour[0,2,:3],[20,100,10],atol=1)
mat[10]=0
layer=bytearray(31);struct.pack_into('<I',layer,0,2);layer[4]=6;layer[5]=64;layer[16]=1;struct.pack_into('<3f',layer,19,1,1,1)
a=SurfaceBake(np.zeros(3),{1:(0,0,0),2:(1,0,0)})
a.draw((mat,[layer]),raw,np.array([[0,1,2]]),{1:(2,2,tex.tobytes()),2:(1,1,bytes([0,80,0,255]))})
assert np.allclose(a.colour[0,0,:3],[71,58,5],atol=1)
assert a.pixels[0,0,0]==127 and a.pixels[0,0,1]==0
for i in range(3):raw[i*48+44:i*48+47]=bytes([64,64,64])
shaded=SurfaceBake(np.zeros(3),{1:(0,0,0)})
shaded.draw((mat,[]),raw,np.array([[0,1,2]]),{1:(2,2,tex.tobytes())})
assert np.allclose(shaded.colour[0,0,:3],[71,11,5],atol=2),shaded.colour[0,0]
print('PASS: source colors, UV orientation, repeat wrapping, half-blended grass and neutral vertex lighting')
