#!/bin/bash
##---------------------------------------------------------------------------##
## CONFIGURE DTK
##---------------------------------------------------------------------------##

rm -rf CMakeCache.txt
rm -rf CMakeFiles

##---------------------------------------------------------------------------##

#PATH_TO_TRILINOS=/home/hobbes/hobbes/enclave/trilinos
PATH_TO_INSTALL_DIR=/opt/simple_busybox/install
PATH_TO_BLAS_LIB=/usr
PATH_TO_LAPACK_LIB=/usr

cmake \
    -D CMAKE_INSTALL_PREFIX:PATH=${PATH_TO_INSTALL_DIR} \
    -D CMAKE_BUILD_TYPE:STRING=RELEASE \
    -D CMAKE_VERBOSE_MAKEFILE:BOOL=OFF \
    -D CMAKE_CXX_FLAGS:STRING="-Wno-unused-parameter -Wno-unknown-pragmas" \
    -D BUILD_SHARED_LIBS:BOOL=OFF \
    -D TPL_FIND_SHARED_LIBS:BOOL=OFF \
    -D Trilinos_LINK_SEARCH_START_STATIC:BOOL=ON \
    -D TPL_ENABLE_MPI:BOOL=ON \
    -D TPL_ENABLE_Boost:BOOL=ON \
    -D TPL_ENABLE_BoostLib:BOOL=ON \
    -D BoostLib_LIBRARY_DIRS:PATH=/usr/lib64 \
    -D BoostLib_INCLUDE_DIRS:PATH=/usr/include \
    -D TPL_BLAS_LIBRARIES:STRING="${PATH_TO_BLAS_LIB}" \
    -D TPL_LAPACK_LIBRARIES:STRING="${PATH_TO_LAPACK_LIB}" \
    -D TPL_ENABLE_Netcdf:BOOL=ON \
    -D Netcdf_LIBRARY_DIRS:PATH="${PATH_TO_INSTALL_DIR}/lib" \
    -D Netcdf_INCLUDE_DIRS:PATH="${PATH_TO_INSTALL_DIR}/include" \
    -D TPL_ENABLE_BinUtils:BOOL=ON \
    -D Trilinos_ENABLE_ALL_OPTIONAL_PACKAGES:BOOL=OFF \
    -D Trilinos_EXTRA_LINK_FLAGS:STRING="-static -L${PATH_TO_INSTALL_DIR}/lib -lnetcdf -L${PATH_TO_INSTALL_DIR}/lib -lhdf5_hl -lhdf5 -lm -lz -lcurl -lblas -llapack -lblas -llapack -ldl" \
    -D Trilinos_EXTRA_REPOSITORIES="DataTransferKit" \
    -D Trilinos_ASSERT_MISSING_PACKAGES=OFF \
    -D Trilinos_ENABLE_Epetra:BOOL=ON \
    -D Trilinos_ENABLE_AztecOO:BOOL=ON \
    -D Trilinos_ENABLE_CXX11:BOOL=ON \
    -D Trilinos_ENABLE_SEACASExodus:BOOL=ON \
    -D Trilinos_ENABLE_SEACASIoss:BOOL=ON \
    -D Trilinos_ENABLE_DataTransferKit:BOOL=ON \
    -D Trilinos_ENABLE_DataTransferKitSTKMeshAdapters:BOOL=ON \
    -D Trilinos_ENABLE_DataTransferKitMoabAdapters:BOOL=OFF \
    -D Trilinos_ENABLE_DataTransferKitLibmeshAdapters:BOOL=OFF \
    -D Trilinos_ENABLE_DataTransferKitClassicDTKAdapters:BOOL=OFF \
    -D Tpetra_INST_COMPLEX_DOUBLE:BOOL=OFF \
    -D Tpetra_INST_COMPLEX_FLOAT:BOOL=OFF \
    -D DataTransferKit_ENABLE_DBC:BOOL=OFF \
    -D DataTransferKit_ENABLE_TESTS:BOOL=ON \
    -D DataTransferKit_ENABLE_EXAMPLES:BOOL=ON \
    $EXTRA_ARGS \
    $PATH_TO_TRILINOS

