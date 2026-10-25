include(FetchContent)
# libultraship already pins this same commit; standalone tests fetch it below.
if(TARGET monocypher)
  FetchContent_GetProperties(monocypher)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/licenses)
  if(EXISTS "${monocypher_SOURCE_DIR}/LICENCE.md")
    configure_file(${monocypher_SOURCE_DIR}/LICENCE.md
                   ${CMAKE_BINARY_DIR}/licenses/monocypher.txt COPYONLY)
  endif()
  return()
endif()
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Official 4.0.2 tag: git ls-remote https://github.com/LoupVaillant/Monocypher.git
# refs/tags/4.0.2 -> 0d85f98c9d9b0227e42cf795cb527dff372b40a4
# Commit-addressed archive plus SHA-256; no floating branch or tag at configure.
# APIs: https://monocypher.org/manual/{aead,argon2,x25519,blake2b}
FetchContent_Declare(kart_monocypher
  URL https://codeload.github.com/LoupVaillant/Monocypher/tar.gz/0d85f98c9d9b0227e42cf795cb527dff372b40a4
  URL_HASH SHA256=7339f8a4729d5339ad6df61ba11937f8d5d163ba5a49edf5fdf3d8717b73d1a9)
FetchContent_MakeAvailable(kart_monocypher)

if(NOT TARGET monocypher)
  add_library(monocypher STATIC ${kart_monocypher_SOURCE_DIR}/src/monocypher.c)
  target_include_directories(monocypher PUBLIC ${kart_monocypher_SOURCE_DIR}/src)
  set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    set_property(TARGET monocypher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/licenses)
  configure_file(${kart_monocypher_SOURCE_DIR}/LICENCE.md
                 ${CMAKE_BINARY_DIR}/licenses/monocypher.txt COPYONLY)
endif()
