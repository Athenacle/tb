INCLUDE(ExternalProject)

ExternalProject_Add(
    rapidjson
    URL https://github.com/Tencent/rapidjson/archive/master.zip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/rapidjson
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

SET(RapidJSONPATH ${CMAKE_CURRENT_BINARY_DIR}/rapidjson/src/rapidjson)

INCLUDE_DIRECTORIES(${RapidJSONPATH}/include)
