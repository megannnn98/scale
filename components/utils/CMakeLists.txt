idf_component_register(INCLUDE_DIRS include SRCS hexstrconv.cpp REQUIRES gsl)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
