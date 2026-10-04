import QtQuick
import QtQuick.Controls

ListView {
    id: root
    property string current: "Pencil"
    property bool vertical: true
    signal picked(string tool)

    orientation: vertical ? ListView.Vertical : ListView.Horizontal
    clip: true
    spacing: 4
    model: ["Pencil", "Brush", "Eraser"]

    delegate: ToolButton {
        required property string modelData
        text: modelData
        highlighted: modelData === root.current
        onClicked: root.picked(modelData)
    }
}
