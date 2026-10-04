import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    signal back()

    // Celular: ferramentas embaixo, timeline compacta. Tablet: barra lateral.
    readonly property bool compact: width < 720

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4
            ToolButton { text: "←"; onClicked: root.back() }
            Label { text: "Quadro " + timeline.currentFrame; Layout.fillWidth: true }
            ToolButton { text: "↶" }
            ToolButton { text: "↷" }
            ToolButton { text: "▶" }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ToolStrip {
            visible: !root.compact
            vertical: true
            Layout.preferredWidth: 88
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            DrawingCanvas {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            ToolStrip {
                visible: root.compact
                vertical: false
                Layout.fillWidth: true
                Layout.preferredHeight: 52
            }
            Timeline {
                id: timeline
                Layout.fillWidth: true
                Layout.preferredHeight: root.compact ? 56 : 88
            }
        }
    }
}
