// world_node_geom.cpp -- oracle adapters for core/game/world_node_geom.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_node.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x0081e0b0, FUN_0081e0b0, "rx_world", 0) {
    rx_world_line_cylinder_x(m.r[7], m.r[6], m.arg(0), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), f32(m.arg(4)), m.arg(5),
                             m.arg(6), m.arg(7));
    return m.ret(0x20);
}
RX_PORT(0x0081e380, FUN_0081e380, "rx_world", 0) {
    rx_world_line_cylinder_y(m.r[6], m.r[7], m.arg(0), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), f32(m.arg(4)), m.arg(5),
                             m.arg(6), m.arg(7));
    return m.ret(0x20);
}
RX_PORT(0x0081e630, FUN_0081e630, "rx_world", 0) {
    rx_world_line_cylinder_z(m.r[6], m.r[7], m.arg(0), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), f32(m.arg(4)), m.arg(5),
                             m.arg(6), m.arg(7));
    return m.ret(0x20);
}
RX_PORT(0x0081ecb0, FUN_0081ecb0, "rx_world", 0) {
    rx_world_line_box_edges(m.r[0], m.arg(0), m.arg(1), f32(m.arg(2)), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
RX_PORT(0x0081d480, FUN_0081d480, "rx_world", 0) {
    rx_world_segment_cone(m.r[1], m.r[0], m.r[6], m.r[7], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)));
    return m.ret(0xc);
}
RX_PORT(0x0081beb0, FUN_0081beb0, "rx_world", 0) {
    rx_world_segment_cylinder(m.r[7], m.r[0], f32(m.arg(0)), f32(m.arg(1)), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.r[5], m.r[3], m.r[6]);
    return m.ret(0x18);
}
RX_PORT(0x0081b6b0, FUN_0081b6b0, "rx_world", 0) {
    rx_world_segment_box_faces(m.r[1], m.r[6], m.r[0], m.r[7], m.arg(0), m.r[5]);
    return m.ret(4);
}
RX_PORT(0x0081c590, FUN_0081c590, "rx_world", 0) {
    rx_world_disc_cylinder(m.r[1], m.r[0], m.r[6], m.r[7], m.arg(0), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), m.r[3], m.r[5]);
    return m.ret(0x10);
}
