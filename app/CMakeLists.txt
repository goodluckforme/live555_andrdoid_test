 cmake_minimum_required(VERSION 3.4.1)
 set(libname rtspclient)

 add_definitions("-D__STDC_CONSTANT_MACROS")

 set(CXX_Home ${CMAKE_SOURCE_DIR}/src/main/cpp)

 include_directories(${CXX_Home}/jni/include/BasicUsageEnvironment)
 include_directories(${CXX_Home}/jni/include/groupsock)
 include_directories(${CXX_Home}/jni/include/liveMedia)
 include_directories(${CXX_Home}/jni/include/UsageEnvironment)

 # 设置 STL 库
 set(ANDROID_STL c++_shared)

 # 设置库路径
 set(CXX_LIB_PATH ${CXX_Home}/libs/${ANDROID_ABI})  # 根据实际路径设置

 message(STATUS "CXX_LIB_PATH: ${CXX_LIB_PATH}")

 message(STATUS "CXX_LIB_PATH:88888888888888888888888888888888")
 add_library(BasicUsageEnvironment
         SHARED
         IMPORTED)
 set_target_properties(BasicUsageEnvironment
         PROPERTIES IMPORTED_LOCATION
         ${CXX_LIB_PATH}/libBasicUsageEnvironment.so)

 add_library(groupsock
         SHARED
         IMPORTED)
 set_target_properties(groupsock
         PROPERTIES IMPORTED_LOCATION
         ${CXX_LIB_PATH}/libgroupsock.so)

 add_library(liveMedia
         SHARED
         IMPORTED)
 set_target_properties(liveMedia
         PROPERTIES IMPORTED_LOCATION
         ${CXX_LIB_PATH}/libliveMedia.so)


 add_library(UsageEnvironment
         SHARED
         IMPORTED)

 set_target_properties(UsageEnvironment
         PROPERTIES IMPORTED_LOCATION
         ${CXX_LIB_PATH}/libUsageEnvironment.so)


 file(GLOB SRC_FILES
         ${CXX_Home}/*.cpp
 )

 add_library( # Sets the name of the library.
         ${libname}
         SHARED
         ${SRC_FILES}
 )
 message(STATUS "CXX_LIB_PATH:88888888888888888888888888s888888")

 find_library(log-lib log)

 # 链接 OpenCV 的 .so 文件
 target_link_libraries(
         ${libname}
         android
         ${log-lib}
         BasicUsageEnvironment
         groupsock
         liveMedia
         UsageEnvironment
 )


