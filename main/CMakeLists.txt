idf_component_register(
    SRCS 
    main.cpp
    PRIV_REQUIRES
    driver
    hx711
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
