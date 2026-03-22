set(srcs
    "cli.c"
    "cli_sys.c"
    )

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS "./"
    REQUIRES console driver
    )
