// D3DCAPS9 (offsets per d3d9caps.h): a shader model 3 GeForce 8-class adapter, HAL device.
put(0, 1); put(4, 0);                                   // DeviceType HAL, AdapterOrdinal
put(8, 0x00020000); put(12, 0xe0020000); put(16, 0x000003a0); put(20, 0x8000000f);   // Caps, Caps2, Caps3, PresentationIntervals
put(24, 1); put(28, 0x001bbef0); put(32, 0x000ef2f2); put(36, 0x07732191);           // CursorCaps, DevCaps, PrimitiveMiscCaps, RasterCaps
put(40, 0xff); put(44, 0x3fff); put(48, 0x3fff); put(52, 0xff); put(56, 0x00084208);  // ZCmp, SrcBlend, DestBlend, AlphaCmp, Shade
put(60, 0x0001ecc5); put(64, 0x03030700); put(68, 0x03030300); put(72, 0x03030300);  // TextureCaps, filter caps
put(76, 0x3f); put(80, 0x3f); put(84, 0x1f);                                         // address caps, LineCaps
put(88, 8192); put(92, 8192); put(96, 2048); put(100, 8192); put(104, 8192); put(108, 16);
putf(112, 1e10f); putf(116, -1e8f); putf(120, -1e8f); putf(124, 1e8f); putf(128, 1e8f); putf(132, 0.0f);
put(136, 0x1ff); put(140, 0x00180008); put(144, 0x03feffff); put(148, 8); put(152, 8); put(156, 0x17b);
put(160, 8); put(164, 8); put(168, 4); put(172, 0); putf(176, 8192.0f); put(180, 0x00ffffff); put(184, 0x00ffffff);
put(188, 16); put(192, 255); put(196, 0xfffe0300); put(200, 256); put(204, 0xffff0300); putf(208, 65504.0f);
put(212, 0x51); putf(216, 0.0f); put(220, 0); put(224, 0); put(228, 0); put(232, 1); put(236, 0x30f); put(240, 4); put(244, 0x03000300);
put(248, 1); put(252, 24); put(256, 32); put(260, 4);                                  // VS20Caps
put(264, 0x1f); put(268, 24); put(272, 32); put(276, 4); put(280, 512);               // PS20Caps
put(284, 0x03000300); put(288, 65535); put(292, 65535); put(296, 4096); put(300, 4096);
