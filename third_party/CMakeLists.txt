# Componentes de código aberto:
#  - libmypaint 1.6.1 (licença ISC): o motor de pincéis do MyPaint
#  - mypaint-brushes 1.3.1 (CC0 / domínio público): os pincéis prontos
#  - json-c 0.17 (licença MIT): leitura dos arquivos .myb
#  - minih264 e minimp4 (CC0 / domínio público): exportação de vídeo MP4 (H.264)
# Tudo é baixado na configuração e compilado junto com o app. Veja THIRD_PARTY_NOTICES.md.
include(FetchContent)
enable_language(C)

# json-c antigo pede CMake < 3.5; libera para CMake novos.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
set(DISABLE_EXTRA_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(json_c
  GIT_REPOSITORY https://github.com/json-c/json-c.git
  GIT_TAG json-c-0.17-20230812
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(json_c)

if(TARGET json-c-static)
  set(ARF_JSONC_TARGET json-c-static)
elseif(TARGET json-c)
  set(ARF_JSONC_TARGET json-c)
else()
  message(FATAL_ERROR "json-c: nenhum alvo encontrado")
endif()

# Cabeçalhos do json-c acessíveis como <json.h> e <json-c/json.h>.
set(_shim "${CMAKE_CURRENT_BINARY_DIR}/jsonc_shim")
file(GLOB _jc_headers "${json_c_SOURCE_DIR}/*.h")
file(COPY ${_jc_headers} DESTINATION "${_shim}/json-c")
file(COPY "${json_c_BINARY_DIR}/json_config.h" DESTINATION "${_shim}/json-c")

# libmypaint e pincéis: sem CMake próprio, então só baixamos os fontes.
FetchContent_Declare(libmypaint
  GIT_REPOSITORY https://github.com/mypaint/libmypaint.git
  GIT_TAG v1.6.1
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR arf_sem_cmake)
FetchContent_MakeAvailable(libmypaint)

FetchContent_Declare(mypaint_brushes
  GIT_REPOSITORY https://github.com/mypaint/mypaint-brushes.git
  GIT_TAG v1.3.1
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR arf_sem_cmake)
FetchContent_MakeAvailable(mypaint_brushes)

# Gera os cabeçalhos de configurações do pincel (feito pelo autotools no projeto original).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
  COMMAND ${Python3_EXECUTABLE} generate.py mypaint-brush-settings-gen.h brushsettings-gen.h
  WORKING_DIRECTORY "${libmypaint_SOURCE_DIR}"
  RESULT_VARIABLE _gen_result)
if(NOT _gen_result EQUAL 0)
  message(FATAL_ERROR "libmypaint: generate.py falhou (${_gen_result})")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mp_config/config.h"
  "#define GETTEXT_PACKAGE \"libmypaint\"\n#define HAVE_JSON_C 1\n")

set(_mp "${libmypaint_SOURCE_DIR}")
add_library(arf_mypaint STATIC
  ${_mp}/brushmodes.c
  ${_mp}/fifo.c
  ${_mp}/helpers.c
  ${_mp}/mypaint-mapping.c
  ${_mp}/mypaint.c
  ${_mp}/mypaint-brush.c
  ${_mp}/mypaint-brush-settings.c
  ${_mp}/mypaint-fixed-tiled-surface.c
  ${_mp}/mypaint-matrix.c
  ${_mp}/mypaint-symmetry.c
  ${_mp}/mypaint-rectangle.c
  ${_mp}/mypaint-surface.c
  ${_mp}/mypaint-tiled-surface.c
  ${_mp}/operationqueue.c
  ${_mp}/rng-double.c
  ${_mp}/tilemap.c)
target_include_directories(arf_mypaint
  PUBLIC ${_mp}
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/mp_config" "${_shim}" "${_shim}/json-c")
set_target_properties(arf_mypaint PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
target_link_libraries(arf_mypaint PUBLIC ${ARF_JSONC_TARGET})
if(UNIX AND NOT ANDROID)
  target_link_libraries(arf_mypaint PUBLIC m)
endif()

# Pasta dos pincéis, usada pelo app para embutir os arquivos.
set(ARF_MYPAINT_BRUSH_DIR "${mypaint_brushes_SOURCE_DIR}/brushes" PARENT_SCOPE)

# Vídeo MP4: codificador H.264 (minih264) + gravador de MP4 (minimp4), só cabeçalhos.
FetchContent_Declare(minih264
  GIT_REPOSITORY https://github.com/lieff/minih264.git
  GIT_TAG b0baea7a80ef9d12da97301dd1099b8791b5ba43
  SOURCE_SUBDIR arf_sem_cmake)
FetchContent_Declare(minimp4
  GIT_REPOSITORY https://github.com/lieff/minimp4.git
  GIT_TAG 2a3014a7753cfcba4442adeb469431967238fe6a
  SOURCE_SUBDIR arf_sem_cmake)
FetchContent_MakeAvailable(minih264 minimp4)

add_library(arf_mp4 STATIC "${CMAKE_CURRENT_SOURCE_DIR}/mp4_impl/arf_mp4.c")
target_include_directories(arf_mp4
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mp4_impl"
  PRIVATE "${minih264_SOURCE_DIR}" "${minimp4_SOURCE_DIR}")
set_target_properties(arf_mp4 PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
