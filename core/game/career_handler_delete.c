/*00402080 EventHandler::vf5, complete original8-byte body. A listener removed
 * during dispatch remains alive until the dispatcher observes this flag.*/
#include "rx_game.h"
void rx_career_handler_mark_deleted(rx_addr handler) {
    SU32(handler,4,1);
}
RX_FUNCTION(0x00402080,rx_career_handler_mark_deleted)
