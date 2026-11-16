# Compatible hardening plus a separate ASan/UBSan diagnostic configuration.
include(CheckCXXCompilerFlag)
option(SIOYEK_HARDENING "Enable compiler/linker hardening on Linux" ON)
option(SIOYEK_SANITIZERS "Instrument application and vendored libraries with ASan/UBSan" OFF)
option(SIOYEK_WARNINGS_AS_ERRORS "Reject application C++ warnings" ON)

set(SIOYEK_HARDENING_FLAGS)
set(SIOYEK_SANITIZER_FLAGS)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(SIOYEK_HARDENING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(flag -fstack-protector-strong -fstack-clash-protection
                 -fcf-protection=full -ftrivial-auto-var-init=zero -fstrict-flex-arrays=3)
      string(MAKE_C_IDENTIFIER "SIOYEK_HAS_${flag}" supported)
      check_cxx_compiler_flag("${flag}" ${supported})
      if(${supported})
        list(APPEND SIOYEK_HARDENING_FLAGS "${flag}")
      endif()
    endforeach()
  endif()
  if(SIOYEK_SANITIZERS)
    set(SIOYEK_SANITIZER_FLAGS -fsanitize=address,undefined
      -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-optimize-sibling-calls)
  endif()
elseif(SIOYEK_SANITIZERS)
  message(FATAL_ERROR "SIOYEK_SANITIZERS currently requires GCC or Clang")
endif()

function(sioyek_enable_safety target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Focus on correctness/security diagnostics; do not hide existing warnings.
    target_compile_options(${target} PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:-Wformat=2;-Wformat-security;-Wreturn-type;-Wuninitialized;-Wnull-dereference;-Warray-bounds;-Wnon-virtual-dtor;-Werror=deprecated-declarations>")
    if(SIOYEK_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Werror>")
    endif()
  endif()
  if(SIOYEK_HARDENING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_compile_options(${target} PRIVATE ${SIOYEK_HARDENING_FLAGS})
    target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS)
    # Fortify requires optimization. makepkg supplies its own optimized flags
    # with build type None; apply the same definitions to that configuration.
    target_compile_options(${target} PRIVATE
      "$<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE;-D_FORTIFY_SOURCE=3>")
    target_link_options(${target} PRIVATE -pie -Wl,-z,relro,-z,now,-z,noexecstack)
  endif()
  if(SIOYEK_SANITIZERS)
    target_compile_options(${target} PRIVATE ${SIOYEK_SANITIZER_FLAGS})
    target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
  endif()
endfunction()

# Make does not track changed flags: give each compiler/flag combination its own
# output directory so a hardening or sanitizer toggle cannot reuse stale objects.
set(SIOYEK_MUPDF_SAFETY_FLAGS ${SIOYEK_HARDENING_FLAGS} ${SIOYEK_SANITIZER_FLAGS})
# MuPDF's bundled C libraries use over-allocated trailing [1] arrays. Do not
# change their layout assumptions with the stricter C++ flexible-array policy.
list(REMOVE_ITEM SIOYEK_MUPDF_SAFETY_FLAGS -fstrict-flex-arrays=3)
if(SIOYEK_HARDENING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND SIOYEK_MUPDF_SAFETY_FLAGS -fPIC -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
endif()
string(JOIN " " SIOYEK_MUPDF_SAFETY_FLAGS ${SIOYEK_MUPDF_SAFETY_FLAGS})
string(SHA256 SIOYEK_MUPDF_FLAGS_HASH
  "${CMAKE_C_COMPILER};${CMAKE_CXX_COMPILER};${SIOYEK_MUPDF_SAFETY_FLAGS}")
string(SUBSTRING "${SIOYEK_MUPDF_FLAGS_HASH}" 0 12 SIOYEK_MUPDF_FLAGS_HASH)
