"""MTX retail venues and audited ground texture identities (not brightness inference)."""
# Source archive name, public name, event category. Keep cache names separate from names players see.
VENUES = {
 'southwick':('Southwick','McKenna Rock','National'),
 'buddscreek':('BuddsCreek','Paradise Valley','National'),
 'washougal':('Washougal','Devils Gate','National'),
 'broome':('Broome','Pacific Hills','National'),
 'bayou':('Bayou','Boggstown','National'),
 'europe':('Europe','Woodbridge','National'),
 'hawaii':('Hawaii','Hawaii','National'),
 'australia':('Australia','Australia','National'),
 'miami':('Miami','Los Angeles','Supercross'),
 'phoenix':('Phoenix','Phoenix','Supercross'),
 'portland':('Portland','San Francisco','Supercross'),
 'denver':('Denver','Denver','Supercross'),
 'minneapolis':('Minneapolis','Minneapolis','Supercross'),
 'atlanta':('Atlanta','Atlanta','Supercross'),
 'saintlouis':('SaintLouis','St. Louis','Supercross'),
 'daytona':('Daytona','Daytona','Supercross'),
}
# These are ground-only textures inspected from each original world cache. Decals,
# tree bark, sponsor signs, water and stadium seating are deliberately excluded.
# Values: primary soil / loose soil / grass / rock. The original RGB is baked too,
# so grey Phoenix soil, warm stadium dirt and tropical grass keep their own palette.
GROUND = {
 'washougal':('0c409980 6a7452e0 01556a00 85786100 1f52eae0','f634a8dc 987dec28 36340e60','6731a808 23c09158',''),
 'broome':('271cff48 f99ffc70 f8082690 64d3980c','84c312f8 dda1af78 04991aa8 baf62358','e4a74028 7253a014 fd677990 162d3454 082ec4f8','e12427c0'),
 'bayou':('f21f7920 022485ac','e6775a7c ce717f5c','191cb7fc','54c642ac'),
 'europe':('e3d71354 0b897b78','20afd068 83d6cd60','94468764 a11c0914 2683c214','f99a3410'),
 'hawaii':('e3d71354 0b897b78 4a0009cc','2c6e58f4','0a8b8924 51804648 3f34b520','199c9c54 4e4ecc60 843b885c'),
 'australia':('7065969c 08d03e74','629d1190 16c4cb9c 537a56c4','','5c0955c0 923f1ea8'),
 'miami':('2b25939c c9f967e0 73107f14 cc7aef08 ff49c8dc','7c5d9ec0','',''),
 'phoenix':('d34359d8 6e2be9e0 61053eb8 5643d790','c9eca750 31bfd428 cdd07c08','',''),
 'portland':('f74f0bd0 599f2898 41b49dac 9f4cfc28 cdd385c4','7c5d9ec0 f55512d8 e8215b70 6d698b70','',''),
 'denver':('bc718c70 1a0044f8 6cdc6358','bcfe5080','',''),
 'minneapolis':('744c0a10 25bd2aac b1478e98','d74b00dc 15b5b250 832641a4 8a119bd0','b5da2d10',''),
 'atlanta':('2d2df680 ed12e4b4 2e3d3810 71695da8','d1fa4128 9696fb40 3701c9e8 cbfbda64 4ba98ad4 7636e0f8','',''),
 'saintlouis':('a940fa70 8d11c668 2f1072c0 b099ccf0 ddc40298','e16870e8','',''),
 'daytona':('2a759474 2d4d899c 65a4e588 370fc240 c2141fec 7e9bc340 a895885c 347fad04 658087d0 7ff1fc34','a2d46b58 61ceb3cc 412116f0','79142e7c','6e4a1dec'),
}
def surfaces(level):
 return {int(t,16):weight for group,weight in zip(GROUND[level],((0,0,0),(0,1,0),(1,0,0),(0,0,1))) for t in group.split()}
def filename(level):
 return ''.join(c for c in VENUES[level][1] if c.isalnum())+'.rxtrack'
