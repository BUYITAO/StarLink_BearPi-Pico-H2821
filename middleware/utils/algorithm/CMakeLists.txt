#===============================================================================
# @brief    cmake file
# Copyright (c) @CompanyNameMagicTag 2022-2022. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "algorithm")

set(LIBS ${CMAKE_CURRENT_SOURCE_DIR}/lib${COMPONENT_NAME}.a)

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/crc
    ${CMAKE_CURRENT_SOURCE_DIR}/sec_random
    ${CMAKE_CURRENT_SOURCE_DIR}/cstl
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

if("CONFIG_DIAG_WITH_SECURE" IN_LIST DEFINES)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/srp/private/srp.c)
    list(APPEND PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/srp/public)
endif()

set(PRIVATE_HEADER
)

set(PRIVATE_DEFINES
)

set(PUBLIC_DEFINES
)

# use this when you want to add ccflags like -include xxx
set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
    -Wno-implicit-function-declaration
)

set(WHOLE_LINK
    true
)

set(MAIN_COMPONENT
    false
)

build_component()

set(COMPONENT_NAME "algorithm_sha256")

set(LIBS ${CMAKE_CURRENT_SOURCE_DIR}/lib${COMPONENT_NAME}.a)

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

set(PRIVATE_HEADER
)

set(PRIVATE_DEFINES
)

set(PUBLIC_DEFINES
)

# use this when you want to add ccflags like -include xxx
set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
)

set(WHOLE_LINK
    true
)

set(MAIN_COMPONENT
    false
)

build_component()