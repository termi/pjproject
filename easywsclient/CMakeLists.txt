cmake_minimum_required(VERSION 3.18)

project(easywsclient_library VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(easywsclient STATIC easywsclient.cpp easywsclient_cpp.cpp)

# Указываем директорию с заголовочными файлами для этой библиотеки
target_include_directories(easywsclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#target_compile_features(easywsclient PUBLIC cxx_std_11)
target_link_options(easywsclient PRIVATE -static-libstdc++ -static-libgcc)

# Если нужна поддержка для специфичных платформ (например, Windows)
if(WIN32)
    target_compile_definitions(easywsclient PRIVATE -D_WIN32_WINNT=0x0601)
endif()

# Установка (для экспорта библиотеки, если нужно)
install(TARGETS easywsclient
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES easywsclient.hpp DESTINATION include)
