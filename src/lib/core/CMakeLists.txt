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

