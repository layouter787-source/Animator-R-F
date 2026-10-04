find_package(Qt6 6.6 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.6)

qt_add_executable(AnimatorRF main.cpp)

qt_add_qml_module(AnimatorRF
  URI AnimatorRF
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/HomeScreen.qml
    qml/EditorScreen.qml
    qml/DrawingCanvas.qml
    qml/Timeline.qml
    qml/ToolStrip.qml
)

target_link_libraries(AnimatorRF PRIVATE Qt6::Quick Qt6::QuickControls2 arf_core)

if(ANDROID)
  set_target_properties(AnimatorRF PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.layouter787.animatorrf")
endif()
