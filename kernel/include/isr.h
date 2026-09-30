#ifndef ISR_H
#define ISR_H

struct interrupt_frame;

void *isr_get_handler(int n);   /* renvoie le pointeur de fonction isrN */

#endif