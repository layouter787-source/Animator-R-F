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

            // Cores, tamanho e ajustes do pincel
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
                ToolButton { text: "⚙"; onClicked: brushPopup.open() }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                spacing: 0
                ToolStrip {
                    visible: root.compact
                    vertical: false
                    current: root.tool
                    onPicked: (t) => root.tool = t
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Item { visible: !root.compact; Layout.fillWidth: true }
                ToolButton {
                    text: "Pincéis"
                    highlighted: root.tool === "Preset"
                    onClicked: libraryPopup.open()
                }
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

    // Ajustes do pincel
    Popup {
        id: brushPopup
        parent: Overlay.overlay
        width: Math.min(root.width - 32, 380)
        x: (parent.width - width) / 2
        y: parent.height - height - 150
        padding: 16
        modal: false
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 8
            Label { text: "Ajustes do pincel"; font.bold: true }
            RowLayout {
                Label { text: "Opacidade"; Layout.preferredWidth: 100 }
                Slider {
                    from: 0.05; to: 1.0; value: canvas.brushOpacity
                    Layout.fillWidth: true
                    onMoved: canvas.brushOpacity = value
                }
            }
            RowLayout {
                Label { text: "Estabilizador"; Layout.preferredWidth: 100 }
                Slider {
                    from: 0.0; to: 1.0; value: canvas.stabilizer
                    Layout.fillWidth: true
                    onMoved: canvas.stabilizer = value
                }
            }
            Switch {
                text: "Suavizar traço"
                checked: canvas.smoothStrokes
                onToggled: canvas.smoothStrokes = checked
            }
            Switch {
                text: "Onion skin"
                checked: canvas.onionSkin
                onToggled: canvas.onionSkin = checked
            }
        }
    }

    // Biblioteca de pincéis (MyPaint, domínio público)
    Popup {
        id: libraryPopup
        parent: Overlay.overlay
        width: Math.min(root.width - 24, 560)
        height: Math.min(root.height * 0.7, 520)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 12
        modal: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                text: canvas.presets.length > 0
                      ? "Pincéis (" + canvas.presets.length + ")"
                      : "Pincéis indisponíveis nesta versão"
                font.bold: true
            }
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: Math.floor(width / Math.max(2, Math.floor(width / 120)))
                cellHeight: 96
                model: canvas.presets
                delegate: Rectangle {
                    required property var modelData
                    width: grid.cellWidth - 6
                    height: grid.cellHeight - 6
                    radius: 6
                    color: canvas.preset === modelData.id && root.tool === "Preset" ? "#d98e3f" : "#2b2d31"
                    Image {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 4
                        height: parent.height - 26
                        fillMode: Image.PreserveAspectFit
                        source: modelData.preview
                        asynchronous: true
                    }
                    Label {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 4
                        width: parent.width - 8
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: modelData.name
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            canvas.preset = modelData.id
                            root.tool = "Preset"
                            libraryPopup.close()
                        }
                    }
                }
            }
        }
    }
}
