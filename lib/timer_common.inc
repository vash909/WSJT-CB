  ! Module storage avoids unsupported TLS COMMON assembly on MinGW GCC 16.
  integer, save :: level = 0, onlevel(0:10) = 0
  !$omp threadprivate(level, onlevel)
