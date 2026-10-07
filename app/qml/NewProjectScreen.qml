import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ArfApp

Page {
    id: root
    signal back()
    signal created(string id)

    readonly property var sizes: [[1280, 720], [1920, 1080], [2560, 1440], [3840, 2160]]
    readonly property var sizeNames: ["HD 720p", "Full HD 1080p", "2K 1440p", "4K 2160p"]
    readonly property var fpsValues: [12, 15, 24, 25, 30, 60]

    // Orientação: 0 horizontal, 1 vertical, 2 quadrado
    property int orientation: 0

    readonly property int outWidth: {
        const s = sizes[sizeBox.currentIndex]
        return orientation === 1 ? s[1] : (orientation === 2 ? s[1] : s[0])
    }
    readonly property int outHeight: {
        const s = sizes[sizeBox.currentIndex]
        return orientation === 1 ? s[0] : s[1]
    }

    function applyPreset(sizeIndex, orient, fpsValue) {
        sizeBox.currentIndex = sizeIndex
        orientation = orient
        fpsBox.currentIndex = fpsValues.indexOf(fpsValue)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; onClicked: root.back() }
            Label { text: "Novo projeto"; font.pixelSize: 18; Layout.fillWidth: true }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 32
        clip: true

        ColumnLayout {
            id: column
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 14

            Label { text: "Nome"; font.bold: true }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "Minha animação"
            }

            Label { text: "Predefinições"; font.bold: true }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Button { text: "YouTube"; onClicked: root.applyPreset(1, 0, 30) }
                Button { text: "Shorts / TikTok"; onClicked: root.applyPreset(1, 1, 30) }
                Button { text: "Anime"; onClicked: root.applyPreset(1, 0, 24) }
                Button { text: "Quadrado"; onClicked: root.applyPreset(1, 2, 30) }
                Button { text: "HD leve"; onClicked: root.applyPreset(0, 0, 24) }
            }

            Label { text: "Resolução"; font.bold: true }
            ComboBox {
                id: sizeBox
                Layout.fillWidth: true
                model: root.sizeNames
                currentIndex: 0
            }

            Label { text: "Orientação"; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Horizontal"; Layout.fillWidth: true
                    highlighted: root.orientation === 0
                    onClicked: root.orientation = 0
                }
                Button {
                    text: "Vertical"; Layout.fillWidth: true
                    highlighted: root.orientation === 1
                    onClicked: root.orientation = 1
                }
                Button {
                    text: "Quadrado"; Layout.fillWidth: true
                    highlighted: root.orientation === 2
                    onClicked: root.orientation = 2
                }
            }

            Label { text: "Quadros por segundo"; font.bold: true }
            ComboBox {
                id: fpsBox
                Layout.fillWidth: true
                model: root.fpsValues
                currentIndex: 2
            }

            Label { text: "Duração (quadros)"; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                SpinBox {
                    id: framesBox
                    from: 1; to: 1200; value: 48
                    editable: true
                    Layout.fillWidth: true
                }
                Label {
                    opacity: 0.7
                    text: "≈ " + (framesBox.value / root.fpsValues[fpsBox.currentIndex]).toFixed(1) + " s"
                }
            }

            Label {
                opacity: 0.7
                text: "Tamanho final: " + root.outWidth + " × " + root.outHeight
            }

            Button {
                text: "Criar projeto"
                highlighted: true
                Layout.fillWidth: true
                Layout.topMargin: 8
                onClicked: {
                    const id = ProjectStore.create(nameField.text, root.outWidth, root.outHeight,
                                                   root.fpsValues[fpsBox.currentIndex], framesBox.value)
                    if (id !== "") root.created(id)
                }
            }
        }
    }
}
