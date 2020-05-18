cmake_minimum_required(VERSION 3.16)

project(psfex)

find_package(PkgConfig REQUIRED)

pkg_check_modules(PLPLOT REQUIRED IMPORTED_TARGET plplot)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3f)
pkg_check_modules(ATLAS REQUIRED IMPORTED_TARGET atlas)

add_definitions(-DPLPLOT_H="plplot.h")
add_definitions(-DPLPLOTP_H="plplotP.h")
add_definitions(-DFFTW_H="fftw3.h")
add_definitions(-DHAVE_GETENV)
add_definitions(-DHAVE_ATLAS)
add_definitions(-DATLAS_LAPACK_H="clapack.h")
add_definitions(-DHAVE_SYS_MMAN_H)
add_definitions(-DHAVE_PLPLOT)

file(GLOB src src/*.c src/fits/*.c src/wcs/*.c)
add_executable(psfex ${src})
target_link_libraries(psfex
        PkgConfig::PLPLOT PkgConfig::FFTW PkgConfig::ATLAS levmar
        m)
