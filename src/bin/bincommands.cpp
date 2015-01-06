/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "bincommands.h"
#include "bin.h"

#include <klocalizedstring.h>

AddBinFolderCommand::AddBinFolderCommand(Bin *bin, const QString &id, const QString &name, const QString &parentId, bool remove, QUndoCommand *parent) :
        QUndoCommand(parent),
        m_bin(bin),
        m_id(id),
        m_name(name),
        m_parentId(parentId),
        m_remove(remove)
{
    if (remove) setText(i18n("Remove Folder"));
    else setText(i18n("Add Folder"));
}
// virtual
void AddBinFolderCommand::undo()
{
    if (m_remove)
        m_bin->doAddFolder(m_id, m_name, m_parentId);
    else
        m_bin->doRemoveFolder(m_id);
}
// virtual
void AddBinFolderCommand::redo()
{
    if (m_remove)
        m_bin->doRemoveFolder(m_id);
    else
        m_bin->doAddFolder(m_id, m_name, m_parentId);
}


MoveBinClipCommand::MoveBinClipCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent) :
        QUndoCommand(parent),
        m_bin(bin),
        m_clipId(clipId),
        m_oldParentId(oldParentId),
        m_newParentId(newParentId)
{
    setText(i18n("Move Clip"));
}
// virtual
void MoveBinClipCommand::undo()
{
    m_bin->doMoveClip(m_clipId, m_oldParentId);
}
// virtual
void MoveBinClipCommand::redo()
{
    m_bin->doMoveClip(m_clipId, m_newParentId);
}

