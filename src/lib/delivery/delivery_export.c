#include "delivery.h"

static void delivery_export_configuration(const struct Delivery *ctx) {
    msg(STASIS_MSG_L2, "Exporting delivery configuration\n");
    if (!pushd(ctx->storage.cfgdump_dir)) {
        char filename[PATH_MAX] = {0};
        sprintf(filename, "%s.ini", ctx->info.release_name);
        FILE *spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx->_stasis_ini_fp.delivery, &spec, INI_WRITE_RAW);
        fclose(spec);

        memset(filename, 0, sizeof(filename));
        sprintf(filename, "%s-rendered.ini", ctx->info.release_name);
        spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx->_stasis_ini_fp.delivery, &spec, INI_WRITE_PRESERVE);
        fclose(spec);
        popd();
    } else {
        SYSERROR("Failed to enter directory: %s", ctx->storage.delivery_dir);
        exit(1);
    }
}

void delivery_export(const struct Delivery *ctx, char *envs[]) {
    delivery_export_configuration(ctx);

    for (size_t i = 0; envs[i] != NULL; i++) {
        char *name = envs[i];
        msg(STASIS_MSG_L2, "Exporting %s\n", name);
        if (conda_env_export(name, ctx->storage.delivery_dir, name)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", name);
            exit(1);
        }
    }
}

void delivery_rewrite_stage1(struct Delivery *ctx, char *specfile) {
    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 1): %s\n", path_basename(specfile));
    delivery_rewrite_spec(ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_1);

    msg(STASIS_MSG_L1, "Rendering mission templates\n");
    delivery_mission_render_files(ctx);
}

void delivery_rewrite_stage2(struct Delivery *ctx, char *specfile) {
    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 2): %s\n", path_basename(specfile));
    delivery_rewrite_spec(ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_2);
}

