#--------------------------------------------------------
# The CMakeLists.txt for:                   pGenPathMCTS
# Author(s):                              Evangelos Siatis
#--------------------------------------------------------

SET(SRC
  GenPathMCTS.cpp
  GenPathMCTS_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pGenPathMCTS ${SRC})

TARGET_LINK_LIBRARIES(pGenPathMCTS
   ${MOOS_LIBRARIES}
   geometry
   apputil
   mbutil
   m
   pthread)
