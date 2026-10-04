import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property int frameCount: 48
    property int currentFrame: 1
    color: "#2b2d31"

    ListView {
        anchors.fill: parent
        orientation: ListView.Horizontal
        clip: true
        model: root.frameCount
        delegate: Rectangle {
            required property int index
            width: 40
            height: ListView.view.height
            color: (index + 1) === root.currentFrame ? "#d98e3f" : "transparent"
            border.color: "#3a3d42"
            Label {
                anchors.centerIn: parent
                text: index + 1
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.currentFrame = index + 1
            }
        }
    }
}
