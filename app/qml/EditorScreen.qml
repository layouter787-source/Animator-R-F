import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import ArfApp

Page {
    id: root
    property string projectId
    signal back()

    // Celular: ferramentas embaixo. Tablet: barra lateral.
    readonly property bool compact: width < 720
    readonly property bool smoke: Qt.application.arguments.indexOf("--smoke") >= 0
    property string tool: "Pencil"
    property color penColor: "#111111"
    property var smokeQueue: ["png", "gif", "zip", "mp4"]
    readonly property var swatches: ["#111111", "#c0392b", "#2f6fb0", "#3f8f5b", "#d9a441", "#ffffff"]
    readonly property string brushLabel: {
        if (tool === "Preset") return canvas.preset.split("/").pop()
        return ({ "Pencil": "Lápis", "Ink": "Tinta", "Brush": "Macio", "Eraser": "Borracha" })[tool]
    }

    function leave() {
        canvas.saveNow()
        root.back()
    }

    function startExport(kind) {
        const filters = { "png": ["PNG (*.png)"], "gif": ["GIF (*.gif)"], "zip": ["ZIP (*.zip)"], "mp4": ["MP4 (*.mp4)"] }
        exportDialog.kind = kind
        exportDialog.nameFilters = filters[kind]
        exportDialog.defaultSuffix = kind
        exportDialog.open()
    }

    function smokeNext() {
        if (smokeQueue.length === 0) {
            Qt.exit(0)
            return
        }
        const k = smokeQueue.shift()
        canvas.exportAs(k, "file:///tmp/arf_smoke." + k)
    }

    Component.onCompleted: {
        canvas.openProject(root.projectId)
        if (smoke) {
            canvas.saveNow()
            smokeNext()
        }
    }

    // Salva quando o app vai para segundo plano.
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive) canvas.saveNow()
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 2
            ToolButton { text: "←"; onClicked: root.leave() }
            Label {
                text: canvas.projectName + " · " + canvas.frame + "/" + canvas.frameCount
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                text: Math.round(canvas.zoom * 100) + "%"
                onClicked: canvas.resetView()
            }
            ToolButton { text: "↶"; enabled: canvas.canUndo; onClicked: canvas.undo() }
            ToolButton { text: "↷"; enabled: canvas.canRedo; onClicked: canvas.redo() }
            ToolButton { text: canvas.playing ? "❚❚" : "▶"; onClicked: canvas.togglePlay() }
            ToolButton { text: "⋮"; onClicked: exportMenu.popup() }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ToolStrip {
            visible: !root.compact
            vertical: true
            brushLabel: root.brushLabel
            brushActive: root.tool !== "Eraser"
            eraserActive: root.tool === "Eraser"
            onBrushClicked: brushPopup.open()
            onEraserClicked: root.tool = "Eraser"
            Layout.preferredWidth: 88
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 12
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
                onExportFinished: (ok, message) => {
                    if (root.smoke) {
                        if (!ok) { console.log("ERRO smoke export: " + message); Qt.exit(2) }
                        else root.smokeNext()
                        return
                    }
                    toastLabel.text = message
                    toast.open()
                }
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
                ToolButton { text: "⚙"; onClicked: settingsPopup.open() }
            }

            ToolStrip {
                visible: root.compact
                vertical: false
                brushLabel: root.brushLabel
                brushActive: root.tool !== "Eraser"
                eraserActive: root.tool === "Eraser"
                onBrushClicked: brushPopup.open()
                onEraserClicked: root.tool = "Eraser"
                Layout.alignment: Qt.AlignHCenter
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

    // ---- exportação ----
    Menu {
        id: exportMenu
        MenuItem { text: "Exportar vídeo MP4"; onTriggered: root.startExport("mp4") }
        MenuItem { text: "Exportar GIF animado"; onTriggered: root.startExport("gif") }
        MenuItem { text: "Exportar imagem (PNG)"; onTriggered: root.startExport("png") }
        MenuItem { text: "Sequência de PNG (.zip)"; onTriggered: root.startExport("zip") }
    }

    FileDialog {
        id: exportDialog
        property string kind: "gif"
        fileMode: FileDialog.SaveFile
        onAccepted: canvas.exportAs(kind, selectedFile)
    }

    Popup {
        id: toast
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: parent.height - height - 120
        padding: 12
        closePolicy: Popup.NoAutoClose
        onOpened: toastTimer.restart()
        Timer { id: toastTimer; interval: 2500; onTriggered: toast.close() }
        contentItem: Label { id: toastLabel }
    }

    Rectangle {
        anchors.fill: parent
        visible: canvas.busy
        color: "#aa000000"
        z: 10
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            spacing: 12
            BusyIndicator { running: canvas.busy; anchors.horizontalCenter: parent.horizontalCenter }
            Label { text: "Exportando…" }
        }
    }

    // ---- ajustes do pincel ----
    Popup {
        id: settingsPopup
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

    // ---- lista de pincéis (abre ao tocar no ícone do lápis) ----
    Popup {
        id: brushPopup
        parent: Overlay.overlay
        width: Math.min(root.width - 24, 560)
        height: Math.min(root.height * 0.75, 560)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 12
        modal: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 8
            Label { text: "Escolha o pincel"; font.bold: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: [
                        { tool: "Pencil", name: "Lápis" },
                        { tool: "Ink", name: "Tinta" },
                        { tool: "Brush", name: "Macio" }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.name
                        Layout.fillWidth: true
                        highlighted: root.tool === modelData.tool
                        onClicked: {
                            root.tool = modelData.tool
                            brushPopup.close()
                        }
                    }
                }
            }

            Label {
                visible: canvas.presets.length > 0
                text: "Pincéis de pintura (" + canvas.presets.length + ")"
                opacity: 0.8
            }
            GridView {
                id: grid
                visible: canvas.presets.length > 0
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
                            brushPopup.close()
                        }
                    }
                }
            }
        }
    }
}
