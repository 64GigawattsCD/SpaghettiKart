# Same audited ENet revision as the earlier networking branch; no native structs on wire.
FetchContent_Declare(kart_enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG 8be2368a8001f28db44e81d5939de5e613025023)
FetchContent_MakeAvailable(kart_enet)
set_target_properties(enet PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  set_property(TARGET enet PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_include_directories(enet PUBLIC ${kart_enet_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/licenses)
configure_file(${kart_enet_SOURCE_DIR}/LICENSE ${CMAKE_BINARY_DIR}/licenses/enet.txt COPYONLY)
