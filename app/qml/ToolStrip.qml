import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Barra de ferramentas enxuta: um ícone de lápis (abre a lista de pincéis) e a borracha.
GridLayout {
    id: root
    property bool vertical: true
    property string brushLabel: ""
    property bool brushActive: true
    property bool eraserActive: false
    signal brushClicked()
    signal eraserClicked()

    columns: vertical ? 1 : 3
    rowSpacing: 4
    columnSpacing: 8

    ToolButton {
        text: "✎"
        font.pixelSize: 26
        highlighted: root.brushActive
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.brushClicked()
    }
    Label {
        text: root.brushLabel
        font.pixelSize: 11
        opacity: 0.8
        elide: Text.ElideRight
        Layout.maximumWidth: 120
        Layout.alignment: Qt.AlignHCenter
        horizontalAlignment: Text.AlignHCenter
    }
    ToolButton {
        text: "⌫"
        font.pixelSize: 26
        highlighted: root.eraserActive
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.eraserClicked()
    }
}
