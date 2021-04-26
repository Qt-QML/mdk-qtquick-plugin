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

#include "videotexturenode.h"
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhigles2_p_p.h>
#ifdef Q_OS_WINDOWS
#include <d3d11.h>
#endif
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#include <QtGui/qvulkaninstance.h>
#include <QtGui/qvulkanfunctions.h>
#endif
#include <mdk/Player.h>
#include <mdk/RenderAPI.h>

MDKPLAYER_BEGIN_NAMESPACE

class VideoTextureNodePrivate final : public VideoTextureNode
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VideoTextureNodePrivate)

public:
    explicit VideoTextureNodePrivate(MDKPlayer *item) : VideoTextureNode(item) {}

    ~VideoTextureNodePrivate() override
    {
        // Release gfx resources
        releaseResources();
    }

private:
    QSGTexture* ensureTexture(MDK_NS_PREPEND(Player) *player, const QSize &size) override;
    void releaseResources();

private:
    QRhiTexture *m_texture = nullptr;
    QRhiTextureRenderTarget *m_rt = nullptr;
    QRhiRenderPassDescriptor *m_rtRp = nullptr;
};

VideoTextureNode* createNodePrivate(MDKPlayer* item)
{
    return new VideoTextureNodePrivate(item);
}

QSGTexture *VideoTextureNodePrivate::ensureTexture(MDK_NS_PREPEND(Player) *player, const QSize &size)
{
    const auto sgrc = QQuickItemPrivate::get(m_item)->sceneGraphRenderContext();
    const auto rhi = sgrc->rhi();
    m_texture = rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (!m_texture->create()) {
#else
    if (!m_texture->build()) {
#endif
        releaseResources();
        return nullptr;
    }
    const QRhiColorAttachment color0(m_texture);
    m_rt = rhi->newTextureRenderTarget({color0});
    if (!m_rt) {
        releaseResources();
        return nullptr;
    }
    m_rtRp = m_rt->newCompatibleRenderPassDescriptor();
    if (!m_rtRp) {
        releaseResources();
        return nullptr;
    }
    m_rt->setRenderPassDescriptor(m_rtRp);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (!m_rt->create()) {
#else
    if (!m_rt->build()) {
#endif
        releaseResources();
        return nullptr;
    }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    intmax_t nativeObj = 0;
    int nativeLayout = 0;
#endif
    const QSGRendererInterface *rif = m_window->rendererInterface();
    switch (rif->graphicsApi()) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    case QSGRendererInterface::OpenGL: // Equal to OpenGLRhi in Qt6
#endif
    case QSGRendererInterface::OpenGLRhi:
    {
#if QT_CONFIG(opengl)
        m_transformMode = TextureCoordinatesTransformFlag::MirrorVertically;
        const auto glrt = static_cast<QGles2TextureRenderTarget *>(m_rt);
        MDK_NS_PREPEND(GLRenderAPI) ra = {};
        ra.fbo = glrt->framebuffer;
        player->setRenderAPI(&ra);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        const auto tex = static_cast<GLuint>(reinterpret_cast<quintptr>(m_texture->nativeTexture().object));
        nativeObj = static_cast<decltype(nativeObj)>(tex);
#if (QT_VERSION <= QT_VERSION_CHECK(5, 14, 0))
        return m_window->createTextureFromId(tex, size);
#endif
#else
        const auto tex = static_cast<GLuint>(m_texture->nativeTexture().object);
        if (tex) {
            return QNativeInterface::QSGOpenGLTexture::fromNative(tex, m_window, size);
        }
#endif
#endif
    } break;
    case QSGRendererInterface::MetalRhi:
    {
#ifdef Q_OS_MACOS
        auto dev = rif->getResource(m_window, QSGRendererInterface::DeviceResource);
        Q_ASSERT(dev);

        MDK_NS_PREPEND(MetalRenderAPI) ra = {};
        ra.texture = reinterpret_cast<const void *>(quintptr(m_texture->nativeTexture().object)); // 5.15+
        ra.device = dev;
        ra.cmdQueue = rif->getResource(m_window, QSGRendererInterface::CommandQueueResource);
        player->setRenderAPI(&ra);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        nativeObj = decltype(nativeObj)(ra.texture);
#else
        if (ra.texture) {
            return QNativeInterface::QSGMetalTexture::fromNative((__bridge id<MTLTexture>)ra.texture, m_window, size);
        }
#endif
#endif
    } break;
    case QSGRendererInterface::Direct3D11Rhi:
    {
#ifdef Q_OS_WINDOWS
        MDK_NS_PREPEND(D3D11RenderAPI) ra = {};
        ra.rtv = reinterpret_cast<ID3D11DeviceChild *>(reinterpret_cast<quintptr>(m_texture->nativeTexture().object));
        player->setRenderAPI(&ra);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        nativeObj = reinterpret_cast<decltype(nativeObj)>(ra.rtv);
#else
        if (ra.rtv) {
            return QNativeInterface::QSGD3D11Texture::fromNative(ra.rtv, m_window, size);
        }
#endif
#endif
    } break;
    case QSGRendererInterface::VulkanRhi:
    {
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
        MDK_NS_PREPEND(VulkanRenderAPI) ra = {};
        ra.device = *static_cast<VkDevice *>(rif->getResource(m_window, QSGRendererInterface::DeviceResource));
        ra.phy_device = *static_cast<VkPhysicalDevice *>(rif->getResource(m_window, QSGRendererInterface::PhysicalDeviceResource));
        ra.opaque = this;
        ra.rt = VkImage(m_texture->nativeTexture().object);
        ra.renderTargetInfo = [](void *opaque, int *w, int *h, VkFormat *fmt, VkImageLayout *layout) {
            const auto node = static_cast<VideoTextureNodePrivate *>(opaque);
            *w = node->m_size.width();
            *h = node->m_size.height();
            *fmt = VK_FORMAT_R8G8B8A8_UNORM;
            *layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return 1;
        };
        ra.currentCommandBuffer = [](void *opaque){
            const auto node = static_cast<VideoTextureNodePrivate *>(opaque);
            const QSGRendererInterface *rif = node->m_window->rendererInterface();
            const auto cmdBuf = *static_cast<VkCommandBuffer *>(rif->getResource(node->m_window, QSGRendererInterface::CommandListResource));
            return cmdBuf;
        };
        player->setRenderAPI(&ra);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        if (ra.rt) {
            return QNativeInterface::QSGVulkanTexture::fromNative(ra.rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_window, size);
        }
#else
        nativeLayout = m_texture->nativeTexture().layout;
        nativeObj = reinterpret_cast<decltype(nativeObj)>(ra.rt);
#endif
#endif
    } break;
    default:
        break;
    }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    if (nativeObj) {
        return m_window->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture, &nativeObj, nativeLayout, size);
    }
#endif
#endif
    return nullptr;
}

void VideoTextureNodePrivate::releaseResources()
{
    if (m_rt) {
        delete m_rt;
        m_rt = nullptr;
    }
    if (m_rtRp) {
        delete m_rtRp;
        m_rtRp = nullptr;
    }
    if (m_texture) {
        delete m_texture;
        m_texture = nullptr;
    }
}

MDKPLAYER_END_NAMESPACE

#include "videotexturenode_private.moc"
