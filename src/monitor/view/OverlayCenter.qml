/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2018 Willian Pessoa
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15

Item {
    id: overlay
    property color color
    Rectangle {
        color: overlay.color
        width: parent.width
        height: 1
        anchors.centerIn: parent
    }
    Rectangle {
        color: overlay.color
        height: parent.height
        width: 1
        anchors.centerIn: parent
    }
}
