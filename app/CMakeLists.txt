find_package(Qt6 6.6 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.6)

if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(AnimatorRF main.cpp)

# O URI do módulo QML vira uma pasta no build; não pode ter o mesmo nome do executável.
qt_add_qml_module(AnimatorRF
  URI ArfApp
  VERSION 1.0
  SOURCES
    drawing_view.h
    drawing_view.cpp
    brush_engine.h
    brush_engine.cpp
    mypaint_engine.h
    mypaint_engine.cpp
  QML_FILES
    qml/Main.qml
    qml/HomeScreen.qml
    qml/EditorScreen.qml
    qml/Timeline.qml
    qml/ToolStrip.qml
)

target_link_libraries(AnimatorRF PRIVATE Qt6::Quick Qt6::QuickControls2 arf_core)

# Pincéis MyPaint (CC0) embutidos no app + motor libmypaint (ISC).
if(ARF_WITH_MYPAINT)
  target_compile_definitions(AnimatorRF PRIVATE ARF_WITH_MYPAINT)
  target_link_libraries(AnimatorRF PRIVATE arf_mypaint)
  file(GLOB_RECURSE _brush_files
    "${ARF_MYPAINT_BRUSH_DIR}/*.myb"
    "${ARF_MYPAINT_BRUSH_DIR}/*_prev.png")
  qt_add_resources(AnimatorRF "mypaint_brushes"
    PREFIX "/brushes"
    BASE "${ARF_MYPAINT_BRUSH_DIR}"
    FILES ${_brush_files})
endif()

if(ANDROID)
  set_target_properties(AnimatorRF PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.layouter787.animatorrf")
endif()
