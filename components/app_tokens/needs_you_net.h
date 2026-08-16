#ifndef NEEDS_YOU_NET_H
#define NEEDS_YOU_NET_H

/* The device's answer channel. Registers the takeover's verdict callback and
 * sends signed POSTs to the bridge on a worker task, so a tap never blocks the
 * UI on the network. Compiled to no-ops without TK_VIBEPULSE_DEVICE_KEY, which
 * leaves the screens display-only (LEAVE IT and the private tap still dismiss
 * locally, they just tell no one). */
void tokens_needs_you_net_start(void);

/* KEY3 panic: deny everything parked, from the platform's button handler. */
void tk_needs_you_send_panic(void);

#endif
