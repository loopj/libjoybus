# Fetch the Unity test framework
if(NOT unity_FOUND)
  include(FetchContent)
  FetchContent_Declare(Unity GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git)
  FetchContent_MakeAvailable(Unity)
endif()

# Little helper function to create a test
function(add_libjoybus_test TEST_NAME)
  # Create the test executable
  add_executable(${TEST_NAME} ${ARGN})

  # Link against the joybus library and Unity
  target_link_libraries(${TEST_NAME} joybus unity::framework)

  # Add the test to CTest
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# GameCube controller target tests
add_libjoybus_test(test_gc_controller target/test_gc_controller.c)
add_libjoybus_test(test_wavebird target/test_wavebird.c)
