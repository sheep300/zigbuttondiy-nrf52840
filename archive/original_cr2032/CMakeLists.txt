cmake_minimum_required(VERSION 3.20.0)

# Ajoute le module Zigbee R23 Nordic à notre projet.
set(ZEPHYR_EXTRA_MODULES "C:/ncs/ncs-zigbee")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zigbuttondiy)

target_sources(app PRIVATE src/main.c)