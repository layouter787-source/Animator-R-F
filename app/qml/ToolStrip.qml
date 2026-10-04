import QtQuick
import QtQuick.Controls

ListView {
    id: root
    property string current: "Pencil"
    property bool vertical: true

    orientation: vertical ? ListView.Vertical : ListView.Horizontal
    clip: true
    spacing: 4
    model: ["Pencil", "Brush", "Eraser", "Fill", "Select", "Bone"]

    delegate: ToolButton {
        required property string modelData
        text: modelData
        highlighted: modelData === root.current
        onClicked: root.current = modelData
    }
}
