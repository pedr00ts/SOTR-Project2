# Specify the minimum required CMake version
cmake_minimum_required(VERSION 3.20.0)

# Find and include the Zephyr build system
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Define the project name (SOTR_Project2 in your case)
project(SOTR_Project2)

# Add your source file(s)
target_sources(app PRIVATE stbs.c)
