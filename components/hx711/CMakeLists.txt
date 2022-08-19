idf_component_register(
    INCLUDE_DIRS 
    HX711_IDF/main
    SRCS
    HX711_IDF/main/HX711.c    
    )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
