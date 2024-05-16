function(template_filter_)
endfunction()

function(template_filter_set)
  set(IT "${ARGN}" PARENT_SCOPE)
endfunction()

function(template_filter_if_else then otherwise)
  if(IT)
    set(IT "${then}" PARENT_SCOPE)
  else()
    set(IT "${otherwise}" PARENT_SCOPE)
  endif()
endfunction()

function(template_filter_string)
  if(ARGV0 STREQUAL "RAW")
    set(tag "")
    while(IT MATCHES "\\)(${tag}_*)\"")
      set(tag "${CMAKE_MATCH_1}_")
    endwhile()
    set(IT "R\"${tag}(${IT})${tag}\"" PARENT_SCOPE)
    return()
  endif()

  string_escape("${IT}" str)
  set(IT "${str}" PARENT_SCOPE)
endfunction()

function(template_filter_join glue)
  list(JOIN IT "${glue}" joined)
  set(IT "${joined}" PARENT_SCOPE)
endfunction()

