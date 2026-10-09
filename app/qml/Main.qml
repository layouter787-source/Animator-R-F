import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import ArfApp

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
        initialItem: homeComponent
    }

    Component {
        id: homeComponent
        HomeScreen {
            onNewProject: stack.push(newProjectComponent)
            onOpenProject: (id) => stack.push(editorComponent, { projectId: id })
        }
    }

    Component {
        id: newProjectComponent
        NewProjectScreen {
            onBack: stack.pop()
            onCreated: (id) => stack.replace(editorComponent, { projectId: id })
        }
    }

    Component {
        id: editorComponent
        EditorScreen {
            onBack: stack.pop()
        }
    }

    // Teste de fumaça do CI: cria um projeto de exemplo (com traços), abre o editor,
    // salva e exporta PNG, GIF, ZIP e MP4.
    Component.onCompleted: {
        if (Qt.application.arguments.indexOf("--smoke") >= 0) {
            const id = ProjectStore.createSample()
            stack.push(editorComponent, { projectId: id })
        }
    }
}
