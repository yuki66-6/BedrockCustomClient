cmake_minimum_required(VERSION 3.24)

# Universal Minecraft Client — composants natifs (Windows).
#   bedrock/  : overlay Minecraft Bedrock (C++20, MSVC)
#   java/     : mod Fabric (Gradle, compilé à part : voir java/fabric/README.md)
#   shared/   : schémas, définitions de modules, profils communs aux deux éditions
project(UniversalMinecraftClient VERSION 1.1.0 LANGUAGES NONE)

enable_testing()
add_subdirectory(bedrock)
