# Torget — ESP-IDF-projektet för skärmplattformen. Bygg (efter
# `. ~/esp/esp-idf/export.sh`):
#   idf.py set-target esp32s3 && idf.py build
#   idf.py -p /dev/cu.usbmodem* flash monitor
#
# Simulatorn under sim/ är ett HELT eget CMake-projekt och byggs som förr.
# Det som delas mellan dem är platform/, components/app_* (utom net.c) och
# kärnkomponenterna — samma kod på Macen som på hyllan, per design.
cmake_minimum_required(VERSION 3.16)
# third_party/ innehåller vendrade komponenter (cjson) som apparna delar.
# Vibbe (app_buddy) bor i sitt eget repo (~/Buddy) och delas in härifrån —
# EN källa, ingen kopiedrift (inflyttsbeslutet 2026-08-07: "alla tre ska bo
# där"). Utan ~/Buddy (en färsk klon utifrån) byggs plattformen med två
# appar — registry.c grindas på TORGET_HAVE_BUDDY.
set(EXTRA_COMPONENT_DIRS third_party)

# Companion-appar bor i sina EGNA repon och är inte del av det här bygget om
# de inte finns utcheckade. En färsk klon utifrån bygger bara VibePulse —
# ingen ska få Solelkollen eller Vibbe på skärmen utan att ha bett om det.
# Sökvägarna kan pekas om med -DTORGET_SOLELKOLLEN_DIR / -DTORGET_BUDDY_DIR.
set(TORGET_SOLELKOLLEN_DIR "$ENV{HOME}/Solelkollen/components"
    CACHE PATH "Companion-katalog för Solelkollen")
set(TORGET_BUDDY_DIR "$ENV{HOME}/Buddy/components"
    CACHE PATH "Companion-katalog för Vibbe/Buddy")

# main/CMakeLists.txt måste läsa dessa i ESP-IDF:s early-expansion (där
# REQUIRES bestäms). Dit når varken cache- eller normalvariabler härifrån —
# bara miljön överlever. Spegla de UPPLÖSTA värdena (inkl. ev. -D-override) till
# miljön så att main grindar sitt REQUIRES på exakt samma sökväg som grindar
# EXTRA_COMPONENT_DIRS nedan. Utan detta sätts TORGET_HAVE_* men komponentens
# include-katalog dras aldrig in, och registry.c faller på app_solelkollen.h.
set(ENV{TORGET_SOLELKOLLEN_DIR} "${TORGET_SOLELKOLLEN_DIR}")
set(ENV{TORGET_BUDDY_DIR} "${TORGET_BUDDY_DIR}")

if(EXISTS "${TORGET_SOLELKOLLEN_DIR}/app_solelkollen")
  list(APPEND EXTRA_COMPONENT_DIRS "${TORGET_SOLELKOLLEN_DIR}")
else()
  message(STATUS "Torget: Solelkollen saknas — bygger utan den (companion-app)")
endif()
if(EXISTS "${TORGET_BUDDY_DIR}/app_buddy")
  list(APPEND EXTRA_COMPONENT_DIRS "${TORGET_BUDDY_DIR}")
else()
  message(STATUS "Torget: ~/Buddy saknas — bygger utan Vibbe/Buddy (companion-app)")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(torget)
