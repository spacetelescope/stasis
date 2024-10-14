include_directories(${PROJECT_BINARY_DIR})

add_library(stasis_core STATIC
        globals.c
        str.c
        strlist.c
        ini.c
        conda.c
        environment.c
        utils.c
        system.c
        download.c
        delivery_postprocess.c
        delivery_conda.c
        delivery_docker.c
        delivery_install.c
        delivery_artifactory.c
        delivery_test.c
        delivery_build.c
        delivery_show.c
        delivery_populate.c
        delivery_init.c
        delivery.c
        recipe.c
        relocation.c
        wheel.c
        copy.c
        artifactory.c
        template.c
        rules.c
        docker.c
        junitxml.c
        github.c
        template_func_proto.c
        envctl.c
        multiprocessing.c
)

