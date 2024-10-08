//! @file template_func_proto.h
#ifndef TEMPLATE_FUNC_PROTO_H
#define TEMPLATE_FUNC_PROTO_H

#include "template.h"

int get_github_release_notes_tplfunc_entrypoint(void *frame, void *data_out);
int get_github_release_notes_auto_tplfunc_entrypoint(void *frame, void *data_out);
int get_junitxml_file_entrypoint(void *frame, void *data_out);
int get_basetemp_dir_entrypoint(void *frame, void *data_out);
int tox_run_entrypoint(void *frame, void *data_out);

#endif //TEMPLATE_FUNC_PROTO_H