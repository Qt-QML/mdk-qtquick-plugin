/*
 * MIT License
 *
 * Copyright (C) 2021 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "mdkplayer_global.h"
#include <QtQuick/qsgtextureprovider.h>
#include <QtQuick/qsgsimpletexturenode.h>
#include <mdk/global.h>

MDK_NS_BEGIN
class Player;
MDK_NS_END

QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(QQuickItem)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)
QT_END_NAMESPACE

MDKPLAYER_BEGIN_NAMESPACE

class MDKPlayer;

class VideoTextureNode : public QSGTextureProvider, public QSGSimpleTextureNode
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VideoTextureNode)

public:
    explicit VideoTextureNode(MDKPlayer *item);
    ~VideoTextureNode() override;

    QSGTexture *texture() const override;

    void sync();

private Q_SLOTS:
    void render();

private:
    virtual QSGTexture *ensureTexture(MDK_NS_PREPEND(Player) *player, const QSize &size) = 0;

protected:
    TextureCoordinatesTransformMode m_transformMode = TextureCoordinatesTransformFlag::NoTransform;
    QQuickWindow *m_window = nullptr;
    QQuickItem *m_item = nullptr;
    QSize m_size = {};

private:
    QWeakPointer<MDK_NS_PREPEND(Player)> m_player;
};

MDKPLAYER_END_NAMESPACE
