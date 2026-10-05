import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ArfApp

Page {
    id: root
    signal back()

    // Celular: ferramentas embaixo. Tablet: barra lateral.
    readonly property bool compact: width < 720
    property string tool: "Pencil"
    property color penColor: "#111111"
    readonly property var swatches: ["#111111", "#c0392b", "#2f6fb0", "#3f8f5b", "#d9a441", "#ffffff"]

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 2
            ToolButton { text: "←"; onClicked: root.back() }
            Label {
                text: "Quadro " + canvas.frame + " / " + canvas.frameCount
                Layout.fillWidth: true
            }
            ToolButton {
                text: Math.round(canvas.zoom * 100) + "%"
                onClicked: canvas.resetView()
            }
            ToolButton { text: "↶"; enabled: canvas.canUndo; onClicked: canvas.undo() }
            ToolButton { text: "↷"; enabled: canvas.canRedo; onClicked: canvas.redo() }
            ToolButton { text: canvas.playing ? "❚❚" : "▶"; onClicked: canvas.togglePlay() }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ToolStrip {
            visible: !root.compact
            vertical: true
            current: root.tool
            onPicked: (t) => root.tool = t
            Layout.preferredWidth: 88
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            DrawingView {
                id: canvas
                tool: root.tool
                color: root.penColor
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            // Cores, tamanho e onion skin
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.preferredHeight: 44
                spacing: 6
                Repeater {
                    model: root.swatches
                    delegate: Rectangle {
                        required property string modelData
                        width: 28; height: 28; radius: 14
                        color: modelData
                        border.width: root.penColor == modelData ? 3 : 1
                        border.color: root.penColor == modelData ? "#d98e3f" : "#555"
                        MouseArea { anchors.fill: parent; onClicked: root.penColor = parent.modelData }
                    }
                }
                Slider {
                    from: 1; to: 40; value: 6
                    Layout.fillWidth: true
                    onMoved: canvas.brushSize = value
                }
                ToolButton {
                    text: "Cebola"
                    checkable: true
                    checked: canvas.onionSkin
                    onToggled: canvas.onionSkin = checked
                }
            }

            ToolStrip {
                visible: root.compact
                vertical: false
                current: root.tool
                onPicked: (t) => root.tool = t
                Layout.fillWidth: true
                Layout.preferredHeight: 48
            }

            // Camadas
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: 0
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: ListView.Horizontal
                    clip: true
                    model: canvas.layerNames
                    delegate: Row {
                        required property int index
                        required property string modelData
                        ToolButton {
                            text: modelData
                            highlighted: index === canvas.activeLayer
                            onClicked: canvas.activeLayer = index
                        }
                        ToolButton {
                            text: { canvas.revision; return canvas.layerVisible(index) ? "●" : "○" }
                            onClicked: canvas.toggleLayerVisible(index)
                        }
                    }
                }
                ToolButton { text: "+ Camada"; onClicked: canvas.addLayer() }
                ToolButton { text: "+ Chave"; onClicked: canvas.insertBlankKey() }
            }

            Timeline {
                Layout.fillWidth: true
                Layout.preferredHeight: root.compact ? 56 : 80
                frameCount: canvas.frameCount
                currentFrame: canvas.frame
                canvas: canvas
                revision: canvas.revision
                onFrameSelected: (f) => canvas.frame = f
            }
        }
    }
}
