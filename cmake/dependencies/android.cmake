# Android bootstrap adapted from SgtBilko76/SpaghettiKart-VR, 0b5ab4eeaf.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(INSTALL_DOCS OFF)
# SDLActivity loads SDL dynamically even though the other dependencies are static.
set(SDL_SHARED ON CACHE BOOL "Android SDL shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Android SDL static library" FORCE)
FetchContent_Declare(ogg
  GIT_REPOSITORY https://github.com/xiph/ogg.git
  GIT_TAG v1.3.5
  OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(ogg)
FetchContent_Declare(vorbis
  GIT_REPOSITORY https://github.com/xiph/vorbis.git
  GIT_TAG v1.3.7
  OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(vorbis)
foreach(VORBIS_LIB vorbis vorbisenc vorbisfile)
  if(NOT TARGET Vorbis::${VORBIS_LIB})
    add_library(Vorbis::${VORBIS_LIB} ALIAS ${VORBIS_LIB})
  endif()
endforeach()
