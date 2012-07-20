CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (playerplugin_examples)

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "/home/hayesst/local/player-3.0.2/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)

PLAYER_ADD_PLUGIN_DRIVER (RFIDdriver SOURCES RFIDdriver.cpp)
