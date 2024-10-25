#ifndef STASIS_SYSTEM_REQUIREMENTS_H
#define STASIS_SYSTEM_REQUIREMENTS_H

#include "delivery.h"
#include "callbacks.h"
#include "envctl.h"

void check_system_env_requirements();
void check_system_requirements(struct Delivery *ctx);
void check_requirements(struct Delivery *ctx);
void check_pathvar(struct Delivery *ctx);

#endif //STASIS_SYSTEM_REQUIREMENTS_H
