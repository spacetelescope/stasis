//
// Created by jhunk on 12/17/23.
//

#ifndef OMC_TEMPLATE_H
#define OMC_TEMPLATE_H

#include "omc.h"

void tpl_register(char *key, char *ptr);
void tpl_free();
char *tpl_getval(char *key);
char *tpl_render(char *str);


#endif //OMC_TEMPLATE_H
