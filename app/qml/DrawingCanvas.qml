import QtQuick

// Placeholder: será substituído pelo renderizador GPU por tiles.
Rectangle {
    id: root
    color: "#ffffff"
    property var strokes: []
    property var current: []

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = 3
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.strokeStyle = "#111111"
            var all = root.strokes.concat([root.current])
            for (var i = 0; i < all.length; i++) {
                var s = all[i]
                if (s.length < 2) continue
                ctx.beginPath()
                ctx.moveTo(s[0].x, s[0].y)
                for (var j = 1; j < s.length; j++) ctx.lineTo(s[j].x, s[j].y)
                ctx.stroke()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: (m) => { root.current = [{ x: m.x, y: m.y }] }
        onPositionChanged: (m) => { root.current.push({ x: m.x, y: m.y }); cv.requestPaint() }
        onReleased: { root.strokes.push(root.current); root.current = []; cv.requestPaint() }
    }
}
