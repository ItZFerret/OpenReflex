#pragma once
#include "rx_game.h"
/*0082ab20: ESI table,EAX allocator(or0),EDI buckets,stack loadBits/owned,RET8.
 * Native callers MUST use this typed5argument API; generic stack dispatch does
 * not translate the original LTCG register inputs into this C prototype. */
rx_addr rx_guest_string_hash_construct(rx_addr table,rx_addr allocator,
                                      uint32_t buckets,uint32_t load_bits,uint32_t owned);
/*008434f0: EDI table,stack keyPointer/valuePointer,RET8. Returns incidental
 * EAX: valuePointer on update,table+8 on insertion; not the inserted node. */
uint32_t rx_guest_string_hash_insert(rx_addr table,rx_addr key_pointer,rx_addr value_pointer);
