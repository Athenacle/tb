
INCLUDE(ExternalProject)

ExternalProject_Add(
  RapidJSON
  URL https://github.com/Tencent/rapidjson/archive/master.zip
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/RapidJSON
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

SET(RapidJSONPATH ${CMAKE_CURRENT_BINARY_DIR}/RapidJSON/src/RapidJSON)

INCLUDE_DIRECTORIES(${RapidJSONPATH}/include)
