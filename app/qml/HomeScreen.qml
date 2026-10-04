import QtQuick
import QtQuick.Controls

Page {
    id: root
    signal newProject()

    header: ToolBar {
        Label {
            text: "Animator-R-F"
            font.pixelSize: 20
            x: 16
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Label {
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        opacity: 0.7
        text: "Nenhum projeto ainda.\nToque em + para criar."
    }

    RoundButton {
        text: "+"
        font.pixelSize: 28
        highlighted: true
        width: 64
        height: 64
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        onClicked: root.newProject()
    }
}
