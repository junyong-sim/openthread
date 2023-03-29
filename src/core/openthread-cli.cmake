#
#  Copyright (c) 2020, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

add_library(openthread-cli SHARED
    ${PROJECT_SOURCE_DIR}/src/posix/ot_instance.c
)

if(OT_FTD)
    target_compile_definitions(openthread-cli PRIVATE
        OPENTHREAD_FTD=1
        OPENTHREAD_MTD=0
    )
elseif(OT_MTD)
    target_compile_definitions(openthread-cli PRIVATE
        OPENTHREAD_FTD=0
        OPENTHREAD_MTD=1
    )
endif()

target_compile_options(openthread-cli PRIVATE
    ${OT_CFLAGS}
)

target_include_directories(openthread-cli PUBLIC ${OT_PUBLIC_INCLUDES} PRIVATE ${COMMON_INCLUDES})

target_sources(openthread-cli PRIVATE ${COMMON_SOURCES})

if(OT_FTD)
    target_link_libraries(openthread-cli
        PRIVATE
            openthread-posix
            openthread-hdlc
            openthread-spinel-rcp
            ${OT_MBEDTLS}
            ot-config-ftd
            ot-config
    )
elseif(OT_MTD)
    target_link_libraries(openthread-cli
        PRIVATE
            openthread-posix
            openthread-hdlc
            openthread-spinel-rcp
            ${OT_MBEDTLS}
            ot-config-mtd
            ot-config
    )
endif()

if(NOT OT_EXCLUDE_TCPLP_LIB)
    if(OT_FTD)
        target_link_libraries(openthread-cli PRIVATE tcplp-ftd)
    elseif(OT_MTD)
        target_link_libraries(openthread-cli PRIVATE tcplp-mtd)
    endif()
endif()

install(TARGETS openthread-cli
    DESTINATION /usr/lib)