#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "delivery.h"

int callback_sort_deliveries_cmpfn(const void *a, const void *b);
int callback_sort_deliveries_dynamic_cmpfn(const void *a, const void *b);

#endif //CALLBACKS_H
