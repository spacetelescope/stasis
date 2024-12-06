//
// Created by jhunk on 11/15/24.
//

#include "core.h"
#include "callbacks.h"

// qsort callback to sort delivery contexts by compact python version
int callback_sort_deliveries_cmpfn(const void *a, const void *b) {
    const struct Delivery *delivery1 = (struct Delivery *) a;
    const size_t delivery1_python = strtoul(delivery1->meta.python_compact, NULL, 10);
    const struct Delivery *delivery2 = (struct Delivery *) b;
    const size_t delivery2_python = strtoul(delivery2->meta.python_compact, NULL, 10);

    if (delivery2_python > delivery1_python) {
        return 1;
    }
    if (delivery2_python < delivery1_python) {
        return -1;
    }
    return 0;
}

// qsort callback to sort dynamically allocated delivery contexts by compact python version
int callback_sort_deliveries_dynamic_cmpfn(const void *a, const void *b) {
    const struct Delivery *delivery1 = a;
    const size_t delivery1_python = strtoul(delivery1->meta.python_compact, NULL, 10);
    const struct Delivery *delivery2 = b;
    const size_t delivery2_python = strtoul(delivery2->meta.python_compact, NULL, 10);

    if (delivery2_python > delivery1_python) {
        return 1;
    }
    if (delivery2_python < delivery1_python) {
        return -1;
    }
    return 0;
}

