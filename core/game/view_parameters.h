#ifndef RX_VIEW_PARAMETERS_H
#define RX_VIEW_PARAMETERS_H
#include "rx_game.h"
/* Full original CPU constructor00794ca0 and nested007b6f20 write set.
 * Deliberately not registered as00794c10: original GPU effect setup is owned
 * by the real host renderer, not implemented by a pretend original callback. */
rx_addr rx_view_parameters_construct(rx_addr parameters);
rx_addr rx_view_parameters_create(void);
#endif
