#include <linux/ppp-ioctl.h>

/*
 * Packet sizes
 */

#define	PPP_MTU		1500	/* Default MTU (size of Info field) */
#define PPP_MAXMRU	65000	/* Largest MRU we allow */
#define PROTO_IPX	0x002b	/* protocol numbers */
#define PROTO_DNA_RT    0x0027  /* DNA Routing */
