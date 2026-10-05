import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property int frameCount: 48
    property int currentFrame: 1
    property var canvas: null
    property int revision: 0
    signal frameSelected(int frame)

    color: "#2b2d31"

    onCurrentFrameChanged: list.positionViewAtIndex(currentFrame - 1, ListView.Contain)

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        clip: true
        model: root.frameCount
        delegate: Rectangle {
            id: cell
            required property int index
            readonly property bool keyed: { root.revision; return root.canvas ? root.canvas.hasKey(index + 1) : false }
            width: 40
            height: ListView.view.height
            color: (index + 1) === root.currentFrame ? "#d98e3f" : "transparent"
            border.color: "#3a3d42"
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 6
                text: cell.index + 1
                font.pixelSize: 12
            }
            Rectangle {
                visible: cell.keyed
                width: 10; height: 10; radius: 5
                color: "#f2f2f2"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.frameSelected(cell.index + 1)
            }
        }
    }
}
