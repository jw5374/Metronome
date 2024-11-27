cmake_minimum_required(VERSION 3.1...3.27)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON) # JW: generates the compile_commands.json used with clangd LSP

# This is your project statement. You should always list languages;
# Listing the version is nice here since it sets lots of useful variables

# JW: Project name potentially needs to match the directory of the project?
project(Metronome
		VERSION 1.0
		LANGUAGES CXX)

link_directories("C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22621.0\\um\\x64")

# JW: This adds the binary targets you want the source to build to
add_executable(metronome metronome.cpp)
# add_executable(working maintestworking.cpp)

# target_link_libraries(metronome Winmm.lib)
