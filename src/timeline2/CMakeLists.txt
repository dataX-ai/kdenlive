# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Nicolas Carion <french.ebook.lover@gmail.com>, Jean-Baptiste Mardelle <jb@kdenlive.org>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  timeline2/model/builders/meltBuilder.cpp
  timeline2/model/clipmodel.cpp
  timeline2/model/compositionmodel.cpp
  timeline2/model/groupsmodel.cpp
  timeline2/model/snapmodel.cpp
  timeline2/model/clipsnapmodel.cpp
  timeline2/model/timelinefunctions.cpp
  timeline2/model/timelineitemmodel.cpp
  timeline2/model/timelinemodel.cpp
  timeline2/model/trackmodel.cpp
  timeline2/view/dialogs/clipdurationdialog.cpp
  timeline2/view/dialogs/spacerdialog.cpp
  timeline2/view/dialogs/speeddialog.cpp
  timeline2/view/dialogs/trackdialog.cpp
  timeline2/view/previewmanager.cpp
  timeline2/view/qml/timelineplayhead.cpp
  timeline2/view/qml/timelinerecwaveform.cpp
  timeline2/view/qml/timelinetriangle.cpp
  timeline2/view/qml/timelinewaveform.cpp
  timeline2/view/qmltypes/thumbnailprovider.cpp
  timeline2/view/timelinecontroller.cpp
  timeline2/view/timelinetabs.cpp
  timeline2/view/timelinewidget.cpp
  PARENT_SCOPE)
