import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ApplicationWindow {
    visible: true
    width: 412
    height: 892
    title: "Animator-R-F"

    Material.theme: Material.Dark
    Material.accent: "#d98e3f"
    Material.background: "#1e1f22"
    Material.primary: "#2b2d31"

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: HomeScreen {
            onNewProject: stack.push(editorComponent)
        }
    }

    Component {
        id: editorComponent
        EditorScreen {
            onBack: stack.pop()
        }
    }
}
