import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ArfApp

Page {
    id: root
    signal newProject()
    signal openProject(string id)

    StackView.onActivated: ProjectStore.refresh()

    header: ToolBar {
        Label {
            text: "Animator-R-F"
            font.pixelSize: 20
            x: 16
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Label {
        visible: ProjectStore.projects.length === 0
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        opacity: 0.7
        text: "Nenhum projeto ainda.\nToque em + para criar."
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        cellWidth: Math.floor(width / Math.max(2, Math.floor(width / 170)))
        cellHeight: cellWidth * 0.75 + 52
        model: ProjectStore.projects

        delegate: Item {
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 8
                color: "#2b2d31"

                Rectangle {
                    id: thumbBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    height: parent.width * 0.75 - 12
                    radius: 4
                    color: "#ffffff"
                    Image {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: modelData.thumb
                        cache: false
                        asynchronous: true
                    }
                }
                Label {
                    anchors.top: thumbBox.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 8
                    anchors.topMargin: 4
                    elide: Text.ElideRight
                    font.bold: true
                    text: modelData.name
                }
                Label {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 8
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    opacity: 0.7
                    text: modelData.width + "×" + modelData.height + " · " + modelData.fps + " fps · " + modelData.modified
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.openProject(modelData.id)
                    onPressAndHold: {
                        deleteDialog.projectId = modelData.id
                        deleteDialog.projectName = modelData.name
                        deleteDialog.open()
                    }
                }
            }
        }
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

    Dialog {
        id: deleteDialog
        property string projectId
        property string projectName
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Apagar projeto?"
        standardButtons: Dialog.Yes | Dialog.No
        Label { text: "“" + deleteDialog.projectName + "” será apagado para sempre." }
        onAccepted: ProjectStore.remove(projectId)
    }
}
