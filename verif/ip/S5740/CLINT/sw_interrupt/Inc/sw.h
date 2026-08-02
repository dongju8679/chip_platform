#ifndef __SW__
#define __SW__

void init_swint_handler(void);
int register_swint_handler(int (*)(void));

#endif
