// Line-oriented test driver: reads vectors on stdin, writes one result line per input line.
// Token format for F80 values: z<s> zero, i<s> inf, q<s> nan, n<s>:<mant hex16>:<exp dec>.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "f80.h"
using namespace reflex_f80;

static F80 parse(const std::string& t) {
  F80 r;
  r.kind = 0; r.sign = 0; r.mant = 0; r.exp = 0;
  r.sign = (uint8_t)(t[1] - '0');
  if (t[0] == 'z') return r;
  if (t[0] == 'i') { r.kind = 1; return r; }
  if (t[0] == 'q') { r.kind = 2; return r; }
  size_t c1 = t.find(':', 0), c2 = t.find(':', c1 + 1);
  r.mant = strtoull(t.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr, 16);
  r.exp = (int32_t)atol(t.substr(c2 + 1).c_str());
  return r;
}
static std::string fmt(const F80& a) {
  char buf[64];
  if (a.kind == 2) { snprintf(buf, sizeof buf, "q%d", a.sign); return buf; }
  if (a.kind == 1) { snprintf(buf, sizeof buf, "i%d", a.sign); return buf; }
  if (a.mant == 0) { snprintf(buf, sizeof buf, "z%d", a.sign); return buf; }
  snprintf(buf, sizeof buf, "n%d:%016llx:%d", a.sign, (unsigned long long)a.mant, a.exp);
  return buf;
}
static std::string hexbytes(const uint8_t* p, int n) {
  std::string s;
  char b[4];
  for (int i = 0; i < n; i++) { snprintf(b, sizeof b, "%02x", p[i]); s += b; }
  return s;
}

int main() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::string o;
    if (op == "add" || op == "sub") {
      int bits, rc, neg; std::string a, b;
      in >> bits >> rc >> neg >> a >> b;
      o = fmt(add(parse(a), parse(b), bits, rc, neg != 0));
    } else if (op == "mul" || op == "div") {
      int bits, rc; std::string a, b;
      in >> bits >> rc >> a >> b;
      o = fmt(op == "mul" ? mul(parse(a), parse(b), bits, rc) : div(parse(a), parse(b), bits, rc));
    } else if (op == "sqrt") {
      int bits, rc; std::string a;
      in >> bits >> rc >> a;
      o = fmt(sqrt(parse(a), bits, rc));
    } else if (op == "cmp") {
      std::string a, b;
      in >> a >> b;
      o = std::to_string(compare(parse(a), parse(b)));
    } else if (op == "rint") {
      int rc; std::string a;
      in >> rc >> a;
      o = fmt(round_to_integer(parse(a), rc));
    } else if (op == "scale") {
      std::string a; long long n;
      in >> a >> n;
      o = fmt(scale(parse(a), n));
    } else if (op == "rem") {
      int nearest; std::string a, b;
      in >> nearest >> a >> b;
      int q = 0;
      F80 r = remainder(parse(a), parse(b), nearest != 0, &q);
      o = fmt(r) + " " + std::to_string(q);
    } else if (op == "to_f32") {
      int rc; std::string a;
      in >> rc >> a;
      char b[32]; snprintf(b, sizeof b, "%08x", to_f32(parse(a), rc));
      o = b;
    } else if (op == "to_f64") {
      int rc; std::string a;
      in >> rc >> a;
      char b[32]; snprintf(b, sizeof b, "%016llx", (unsigned long long)to_f64(parse(a), rc));
      o = b;
    } else if (op == "to_f80") {
      std::string a;
      in >> a;
      uint8_t out[10];
      to_f80(parse(a), out);
      o = hexbytes(out, 10);
    } else if (op == "from_f32") {
      std::string h; in >> h;
      o = fmt(from_f32((uint32_t)strtoul(h.c_str(), nullptr, 16)));
    } else if (op == "from_f64") {
      std::string h; in >> h;
      o = fmt(from_f64(strtoull(h.c_str(), nullptr, 16)));
    } else if (op == "from_f80") {
      std::string h; in >> h;
      uint8_t raw[10];
      for (int i = 0; i < 10; i++) raw[i] = (uint8_t)strtoul(h.substr(2 * i, 2).c_str(), nullptr, 16);
      o = fmt(from_f80(raw));
    } else if (op == "from_int") {
      long long v; in >> v;
      o = fmt(from_int(v));
    } else if (op == "tr") {
      std::string name, a, b; int rc;
      in >> name >> rc >> a >> b;
      F80 fa, fb;
      const F80 *pa = nullptr, *pb = nullptr;
      if (a != "-") { fa = parse(a); pa = &fa; }
      if (b != "-") { fb = parse(b); pb = &fb; }
      int err = 0;
      F80 r = transcendental_ex(name.c_str(), pa, pb, rc, &err);
      o = err ? "ERR" : fmt(r);
    } else {
      o = "BADOP";
    }
    std::cout << o << "\n";
  }
  return 0;
}
