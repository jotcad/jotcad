[[ -d native/cgal ]] || (
  git clone https://github.com/pentacular/cgal native/cgal &&
  CGAL_HOME=${PWD}/native/include/CGAL &&
  cd native/cgal &&
  mkdir build &&
  cd build &&
  rm -f ../CMakeCache.txt &&
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CGAL_HOME} -DCGAL_WITH_GMP=OFF -DCGAL_WITH_MPFR=OFF -DCGAL_USE_GMPXX=OFF -DCGAL_CMAKE_EXACT_NT_BACKEND=BOOST_BACKEND &&
  make &&
  make install &&
  cd ../../..)
