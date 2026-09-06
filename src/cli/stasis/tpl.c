#include "delivery.h"
#include "tpl.h"

void tpl_setup_vars(struct Delivery *ctx) {
    // Expose variables for use with the template engine
    // NOTE: These pointers are populated by delivery_init() so please avoid using
    // tpl_render() until then.
    tpl_register(&ctx->tpl_pool, "meta.name", &ctx->meta.name);
    tpl_register(&ctx->tpl_pool, "meta.version", &ctx->meta.version);
    tpl_register(&ctx->tpl_pool, "meta.codename", &ctx->meta.codename);
    tpl_register(&ctx->tpl_pool, "meta.mission", &ctx->meta.mission);
    tpl_register(&ctx->tpl_pool, "meta.python", &ctx->meta.python);
    tpl_register(&ctx->tpl_pool, "meta.python_compact", &ctx->meta.python_compact);
    tpl_register(&ctx->tpl_pool, "info.time_str_epoch", &ctx->info.time_str_epoch);
    tpl_register(&ctx->tpl_pool, "info.release_name", &ctx->info.release_name);
    tpl_register(&ctx->tpl_pool, "info.build_name", &ctx->info.build_name);
    tpl_register(&ctx->tpl_pool, "info.build_number", &ctx->info.build_number);
    tpl_register(&ctx->tpl_pool, "storage.tmpdir", &ctx->storage.tmpdir);
    tpl_register(&ctx->tpl_pool, "storage.output_dir", &ctx->storage.output_dir);
    tpl_register(&ctx->tpl_pool, "storage.delivery_dir", &ctx->storage.delivery_dir);
    tpl_register(&ctx->tpl_pool, "storage.conda_artifact_dir", &ctx->storage.conda_artifact_dir);
    tpl_register(&ctx->tpl_pool, "storage.wheel_artifact_dir", &ctx->storage.wheel_artifact_dir);
    tpl_register(&ctx->tpl_pool, "storage.build_sources_dir", &ctx->storage.build_sources_dir);
    tpl_register(&ctx->tpl_pool, "storage.build_docker_dir", &ctx->storage.build_docker_dir);
    tpl_register(&ctx->tpl_pool, "storage.results_dir", &ctx->storage.results_dir);
    tpl_register(&ctx->tpl_pool, "storage.tools_dir", &ctx->storage.tools_dir);
    tpl_register(&ctx->tpl_pool, "conda.installer_baseurl", &ctx->conda.installer_baseurl);
    tpl_register(&ctx->tpl_pool, "conda.installer_name", &ctx->conda.installer_name);
    tpl_register(&ctx->tpl_pool, "conda.installer_version", &ctx->conda.installer_version);
    tpl_register(&ctx->tpl_pool, "conda.installer_arch", &ctx->conda.installer_arch);
    tpl_register(&ctx->tpl_pool, "conda.installer_platform", &ctx->conda.installer_platform);
    tpl_register(&ctx->tpl_pool, "deploy.jfrog.repo", &globals.jfrog.repo);
    tpl_register(&ctx->tpl_pool, "deploy.jfrog.url", &globals.jfrog.url);
    tpl_register(&ctx->tpl_pool, "deploy.docker.registry", &ctx->deploy.docker.registry);
    tpl_register(&ctx->tpl_pool, "workaround.conda_reactivate", &globals.workaround.conda_reactivate);
    tpl_register(&ctx->tpl_pool, "system.arch", &ctx->system.arch);
    tpl_register(&ctx->tpl_pool, "system.platform", &ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);

}

void tpl_setup_funcs(struct Delivery *ctx) {
    // Expose function(s) to the template engine
    // Prototypes can be found in template_func_proto.h
    tpl_register_func("get_github_release_notes", &get_github_release_notes_tplfunc_entrypoint, 3, NULL);
    tpl_register_func("get_github_release_notes_auto", &get_github_release_notes_auto_tplfunc_entrypoint, 1, ctx);
    tpl_register_func("junitxml_file", &get_junitxml_file_entrypoint, 1, ctx);
    tpl_register_func("basetemp_dir", &get_basetemp_dir_entrypoint, 1, ctx);
    tpl_register_func("tox_run", &tox_run_entrypoint, 2, ctx);
}