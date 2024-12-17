cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX C)

# Поиск libiio
find_library(LIBIIO_LIBRARIES iio)
message(${LIBIIO_LIBRARIES})

set(MAIN_SOURCE_FILES
    src/main.cpp
)

# Добавляем исполняемый файл
add_executable(main ${MAIN_SOURCE_FILES})

# Линковка 
target_link_libraries(main ${LIBIIO_LIBRARIES})

