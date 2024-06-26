#include "template_func_proto.h"

int get_github_release_notes_tplfunc_entrypoint(void *frame, void *ptr) {
    int result;
    char **output = (char **) ptr;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GITHUB_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }
    result = get_github_release_notes(api_token ? api_token : "anonymous",
                                      (const char *) f->argv[0].t_char_ptr,
                                      (const char *) f->argv[1].t_char_ptr,
                                      (const char *) f->argv[2].t_char_ptr,
                                      output);
    return result;
}

