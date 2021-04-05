/*
 * MIT License
 *
 * Copyright (C) 2020 by wangwenx190 (Yuhang Zhao)
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

#include "mdkobject.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTextureProvider>
#include <QScreen>
#include <QStandardPaths>
#include <QTime>
#include <QtMath>

#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif

#if QT_CONFIG(opengl)
#include <QOpenGLFramebufferObject>
#endif

#ifdef Q_OS_WINDOWS
#include <d3d11.h>
#include <wrl/client.h>
#endif

#ifdef Q_OS_MACOS
#include <Metal/Metal.h>
#endif

#include <mdk/Player.h>
#include <mdk/RenderAPI.h>

#if QT_CONFIG(vulkan)
#define VK_RUN_CHECK(x, ...) \
    do { \
        VkResult __vkret__ = x; \
        if (__vkret__ != VK_SUCCESS) { \
            qCDebug(lcMdkVulkanRenderer).noquote() \
                << #x " ERROR: " << __vkret__ << " @" << __LINE__ << __func__; \
            __VA_ARGS__; \
        } \
    } while (false)
#define VK_ENSURE(x, ...) VK_RUN_CHECK(x, return __VA_ARGS__)
#define VK_WARN(x, ...) VK_RUN_CHECK(x)
#endif

Q_LOGGING_CATEGORY(lcMdk, "mdk.general")
Q_LOGGING_CATEGORY(lcMdkLog, "mdk.log.general")
Q_LOGGING_CATEGORY(lcMdkRenderer, "mdk.renderer.general")
#ifdef Q_OS_WINDOWS
Q_LOGGING_CATEGORY(lcMdkD3D12Renderer, "mdk.renderer.d3d12")
Q_LOGGING_CATEGORY(lcMdkD3D11Renderer, "mdk.renderer.d3d11")
#endif
Q_LOGGING_CATEGORY(lcMdkVulkanRenderer, "mdk.renderer.vulkan")
#ifdef Q_OS_MACOS
Q_LOGGING_CATEGORY(lcMdkMetalRenderer, "mdk.renderer.metal")
#endif
Q_LOGGING_CATEGORY(lcMdkOpenGLRenderer, "mdk.renderer.opengl")
Q_LOGGING_CATEGORY(lcMdkPlayback, "mdk.playback.general")
Q_LOGGING_CATEGORY(lcMdkMisc, "mdk.misc.general")

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const MdkObject::Chapters &chapters)
{
    QDebugStateSaver saver(d);
    d.nospace();
    d.noquote();
    QString chaptersStr = {};
    for (auto &&chapter : qAsConst(chapters)) {
        chaptersStr.append(QString::fromUtf8("(title: %1, beginTime: %2, endTime: %3)")
                               .arg(chapter.title,
                                    QString::number(chapter.beginTime),
                                    QString::number(chapter.endTime)));
    }
    d << "QList(" << chaptersStr << ')';
    return d;
}
#endif

static inline QStringList suffixesToMimeTypes(const QStringList &suffixes)
{
    QStringList mimeTypes{};
    const QMimeDatabase db;
    for (auto &&suffix : qAsConst(suffixes)) {
        const QList<QMimeType> typeList = db.mimeTypesForFileName(suffix);
        if (!typeList.isEmpty()) {
            for (auto &&mimeType : qAsConst(typeList)) {
                const QString name = mimeType.name();
                if (!name.isEmpty()) {
                    mimeTypes.append(name);
                }
            }
        }
    }
    if (!mimeTypes.isEmpty()) {
        mimeTypes.removeDuplicates();
    }
    return mimeTypes;
}

static inline QString timeToString(const qint64 ms, const bool isAudio = false)
{
    return QTime(0, 0).addMSecs(ms).toString(isAudio ? QString::fromUtf8("mm:ss")
                                                     : QString::fromUtf8("hh:mm:ss"));
}

static inline std::vector<std::string> qStringListToStdStringVector(const QStringList &stringList)
{
    if (stringList.isEmpty()) {
        return {};
    }
    std::vector<std::string> result{};
    for (auto &&string : qAsConst(stringList)) {
        result.push_back(string.toStdString());
    }
    return result;
}

static inline QString urlToString(const QUrl &value, const bool display = false)
{
    if (!value.isValid()) {
        return {};
    }
    return (value.isLocalFile() ? QDir::toNativeSeparators(value.toLocalFile())
                                : (display ? value.toDisplayString() : value.url()));
}

static inline MDK_NS::LogLevel _MDKObject_MDK_LogLevel()
{
    return static_cast<MDK_NS::LogLevel>(MDK_logLevel());
}

class VideoTextureNode : public QSGTextureProvider, public QSGSimpleTextureNode
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VideoTextureNode)

public:
    explicit VideoTextureNode(MdkObject *item) : m_item(item), m_player(item->m_player)
    {
        Q_ASSERT(m_item && !m_player.isNull());
        m_window = m_item->window();
        connect(m_window, &QQuickWindow::beforeRendering, this, &VideoTextureNode::render);
        connect(m_window, &QQuickWindow::screenChanged, this, [this](QScreen *screen) {
            Q_UNUSED(screen)
            if (m_window->effectiveDevicePixelRatio() != m_dpr) {
                m_item->update();
            }
        });
        m_livePreview = item->m_livePreview;
        if (!m_livePreview) {
            qCDebug(lcMdkRenderer).noquote() << "Renderer created.";
        }
    }

    ~VideoTextureNode() override
    {
        delete texture();
        // Release gfx resources.
#if QT_CONFIG(vulkan)
        freeTexture();
#endif
#if QT_CONFIG(opengl)
        fbo_gl.reset();
#endif
        // when device lost occurs
        const auto player = m_player.lock();
        if (!player) {
            return;
        }
        player->setVideoSurfaceSize(-1, -1);
        if (!m_livePreview) {
            qCDebug(lcMdkRenderer).noquote() << "Renderer destroyed.";
        }
    }

    QSGTexture *texture() const override { return QSGSimpleTextureNode::texture(); }

    void sync()
    {
        m_dpr = m_window->effectiveDevicePixelRatio();
        const QSizeF newSize = m_item->size() * m_dpr;
        bool needsNew = false;
        if (!texture()) {
            needsNew = true;
        }
        if (newSize != m_size) {
            needsNew = true;
            m_size = {qRound(newSize.width()), qRound(newSize.height())};
        }
        if (!needsNew) {
            return;
        }
        delete texture();
        const auto player = m_player.lock();
        if (!player) {
            return;
        }
        QSGRendererInterface *rif = m_window->rendererInterface();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        intmax_t nativeObj = 0;
        int nativeLayout = 0;
#endif
        switch (rif->graphicsApi()) {
        case QSGRendererInterface::Direct3D11Rhi: {
            // Direct3D: Qt RHI's default backend on Windows. Not supported on
            // all other platforms.
#ifdef Q_OS_WINDOWS
            auto dev = static_cast<ID3D11Device *>(
                rif->getResource(m_window, QSGRendererInterface::DeviceResource));
            D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
                                                              m_size.width(),
                                                              m_size.height(),
                                                              1,
                                                              1,
                                                              D3D11_BIND_SHADER_RESOURCE
                                                                  | D3D11_BIND_RENDER_TARGET,
                                                              D3D11_USAGE_DEFAULT,
                                                              0,
                                                              1,
                                                              0,
                                                              0);
            if (FAILED(dev->CreateTexture2D(&desc, nullptr, &m_texture_d3d11))) {
                if (!m_livePreview) {
                    qCCritical(lcMdkD3D11Renderer).noquote() << "Failed to create 2D texture!";
                }
            }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            nativeObj = reinterpret_cast<decltype(nativeObj)>(m_texture_d3d11.Get());
#endif
            MDK_NS::D3D11RenderAPI ra{};
            ra.rtv = m_texture_d3d11.Get();
            player->setRenderAPI(&ra);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            const auto nativeObj = m_texture_d3d11.Get();
            if (nativeObj) {
                QSGTexture *wrapper = QNativeInterface::QSGD3D11Texture::fromNative(nativeObj,
                                                                                    m_window,
                                                                                    m_size);
                setTexture(wrapper);
            } else {
                qCCritical(lcMdkD3D11Renderer).noquote()
                    << "Can't set texture due to null nativeObj. Nothing will be rendered.";
            }
#endif
#else
            qCCritical(lcMdkRenderer).noquote()
                << "Failed to initialize the Direct3D11 renderer: The Direct3D11 renderer is only "
                   "available on Windows platform.";
#endif
        } break;
        case QSGRendererInterface::VulkanRhi: {
            // Vulkan: Qt RHI's default backend on Linux (Android). Supported on
            // Windows and macOS as well.
#if QT_CONFIG(vulkan)
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            nativeLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
            auto inst = reinterpret_cast<QVulkanInstance *>(
                rif->getResource(m_window, QSGRendererInterface::VulkanInstanceResource));
            m_physDev = *static_cast<VkPhysicalDevice *>(
                rif->getResource(m_window, QSGRendererInterface::PhysicalDeviceResource));
            auto newDev = *static_cast<VkDevice *>(
                rif->getResource(m_window, QSGRendererInterface::DeviceResource));
            /*qCDebug(lcMdkVulkanRenderer).noquote()
                << "old device:" << (void *) m_dev << "newDev:" << (void *) newDev;*/
            // TODO: why m_dev is 0 if device lost
            freeTexture();
            m_dev = newDev;
            m_devFuncs = inst->deviceFunctions(m_dev);
            buildTexture(m_size);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            nativeObj = reinterpret_cast<decltype(nativeObj)>(m_texture_vk);
#endif
            MDK_NS::VulkanRenderAPI ra{};
            ra.device = m_dev;
            ra.phy_device = m_physDev;
            ra.opaque = this;
            ra.rt = m_texture_vk;
            ra.renderTargetInfo =
                [](void *opaque, int *w, int *h, VkFormat *fmt, VkImageLayout *layout) {
                    auto node = static_cast<VideoTextureNode *>(opaque);
                    *w = node->m_size.width();
                    *h = node->m_size.height();
                    *fmt = VK_FORMAT_R8G8B8A8_UNORM;
                    *layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    return 1;
                };
            ra.currentCommandBuffer = [](void *opaque) {
                auto node = static_cast<VideoTextureNode *>(opaque);
                QSGRendererInterface *rif = node->m_window->rendererInterface();
                auto cmdBuf = *static_cast<VkCommandBuffer *>(
                    rif->getResource(node->m_window, QSGRendererInterface::CommandListResource));
                return cmdBuf;
            };
            player->setRenderAPI(&ra);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            if (m_texture_vk) {
                QSGTexture *wrapper = QNativeInterface::QSGVulkanTexture::fromNative(
                    m_texture_vk, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_window, m_size);
                setTexture(wrapper);
            } else {
                qCCritical(lcMdkVulkanRenderer).noquote()
                    << "Can't set texture due to null nativeObj. Nothing will be rendered.";
            }
#endif
#else
            qCCritical(lcMdkRenderer).noquote()
                << "Failed to initialize the Vulkan renderer: This version of Qt is not configured "
                   "with Vulkan support.";
#endif
        } break;
        case QSGRendererInterface::MetalRhi: {
            // Metal: Qt RHI's default backend on macOS. Not supported on all
            // other platforms.
#ifdef Q_OS_MACOS
            auto dev = static_cast<__bridge id<MTLDevice>>(
                rif->getResource(m_window, QSGRendererInterface::DeviceResource));
            Q_ASSERT(dev);
            MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
            desc.textureType = MTLTextureType2D;
            desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
            desc.width = m_size.width();
            desc.height = m_size.height();
            desc.mipmapLevelCount = 1;
            desc.resourceOptions = MTLResourceStorageModePrivate;
            desc.storageMode = MTLStorageModePrivate;
            desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
            m_texture_mtl = [dev newTextureWithDescriptor:desc];
            MDK_NS::MetalRenderAPI ra{};
            ra.texture = (__bridge void *) m_texture_mtl;
            ra.device = static_cast<__bridge void *>(dev);
            ra.cmdQueue = rif->getResource(m_window, QSGRendererInterface::CommandQueueResource);
            player->setRenderAPI(&ra);
            nativeObj = decltype(nativeObj)(ra.texture);
#else
            qCCritical(lcMdkRenderer).noquote()
                << "Failed to initialize the Metal renderer: The Metal renderer is only available "
                   "on macOS platform.";
#endif
        } break;
        case QSGRendererInterface::OpenGLRhi: {
            // OpenGL: The legacy backend, to keep compatibility of Qt 5.
            // Supported on all mainstream platforms.
#if QT_CONFIG(opengl)
            fbo_gl.reset(new QOpenGLFramebufferObject(m_size));
            const auto tex = fbo_gl->texture();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            nativeObj = static_cast<decltype(nativeObj)>(tex);
#endif
            MDK_NS::GLRenderAPI ra{};
            ra.fbo = fbo_gl->handle();
            player->setRenderAPI(&ra);
            // Flip y.
            player->scale(1.0f, -1.0f);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            if (tex) {
                QSGTexture *wrapper = QNativeInterface::QSGOpenGLTexture::fromNative(tex,
                                                                                     m_window,
                                                                                     m_size);
                setTexture(wrapper);
            } else {
                qCCritical(lcMdkOpenGLRenderer).noquote()
                    << "Can't set texture due to null nativeObj. Nothing will be rendered.";
            }
#endif
#else
            qCCritical(lcMdkRenderer).noquote()
                << "Failed to initialize the OpenGL renderer: This version of Qt is not configured "
                   "with OpenGL support.";
#endif
        } break;
        default:
            if (!m_livePreview) {
                qCCritical(lcMdkRenderer).noquote()
                    << "QSGRendererInterface reports unknown graphics API:" << rif->graphicsApi();
            }
            break;
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        if (nativeObj) {
            QSGTexture *wrapper
                = m_window->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture,
                                                          &nativeObj,
                                                          nativeLayout,
                                                          m_size);
            setTexture(wrapper);
        } else {
            qCCritical(lcMdkRenderer).noquote()
                << "Can't set texture due to null nativeObj. Nothing will be rendered.";
        }
#endif
        player->setVideoSurfaceSize(m_size.width(), m_size.height());
    }

private Q_SLOTS:
    // This is hooked up to beforeRendering() so we can start our own render
    // command encoder. If we instead wanted to use the scenegraph's render
    // command encoder (targeting the window), it should be connected to
    // beforeRenderPassRecording() instead.
    void render()
    {
        const auto player = m_player.lock();
        if (!player) {
            return;
        }
        player->renderVideo();
    }

#if QT_CONFIG(vulkan)
    bool buildTexture(const QSize &size)
    {
        VkImageCreateInfo imageInfo;
        memset(&imageInfo, 0, sizeof(imageInfo));
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags = 0;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // QtQuick hardcoded
        imageInfo.extent.width = uint32_t(size.width());
        imageInfo.extent.height = uint32_t(size.height());
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkImage image = VK_NULL_HANDLE;
        VK_ENSURE(m_devFuncs->vkCreateImage(m_dev, &imageInfo, nullptr, &image), false);
        m_texture_vk = image;
        VkMemoryRequirements memReq;
        m_devFuncs->vkGetImageMemoryRequirements(m_dev, image, &memReq);
        quint32 memIndex = 0;
        VkPhysicalDeviceMemoryProperties physDevMemProps;
        m_window->vulkanInstance()
            ->functions()
            ->vkGetPhysicalDeviceMemoryProperties(m_physDev, &physDevMemProps);
        for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
            if (!(memReq.memoryTypeBits & (1 << i))) {
                continue;
            }
            memIndex = i;
        }
        VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          nullptr,
                                          memReq.size,
                                          memIndex};
        VK_ENSURE(m_devFuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, &m_textureMemory), false);
        VK_ENSURE(m_devFuncs->vkBindImageMemory(m_dev, image, m_textureMemory, 0), false);
        return true;
    }

    void freeTexture()
    {
        if (!m_texture_vk) {
            return;
        }
        VK_WARN(m_devFuncs->vkDeviceWaitIdle(m_dev));
        m_devFuncs->vkFreeMemory(m_dev, m_textureMemory, nullptr);
        m_textureMemory = VK_NULL_HANDLE;
        m_devFuncs->vkDestroyImage(m_dev, m_texture_vk, nullptr);
        m_texture_vk = VK_NULL_HANDLE;
    }
#endif

private:
    bool m_livePreview = false;
    QQuickItem *m_item = nullptr;
    QQuickWindow *m_window = nullptr;
    QSize m_size = {};
    qreal m_dpr = 1.0;
#if QT_CONFIG(vulkan)
    VkImage m_texture_vk = VK_NULL_HANDLE;
    VkDeviceMemory m_textureMemory = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDev = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_devFuncs = nullptr;
#endif
#if QT_CONFIG(opengl)
    QScopedPointer<QOpenGLFramebufferObject> fbo_gl;
#endif
    QWeakPointer<MDK_NS::Player> m_player;
#ifdef Q_OS_WINDOWS
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture_d3d11;
#endif
#ifdef Q_OS_MACOS
    id<MTLTexture> m_texture_mtl = nil;
#endif
};

MdkObject::MdkObject(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    qRegisterMetaType<ChapterInfo>();
    qRegisterMetaType<VideoStreamInfo>();
    qRegisterMetaType<AudioStreamInfo>();
    qRegisterMetaType<MediaInfo>();
    m_player.reset(new MDK_NS::Player);
    Q_ASSERT(!m_player.isNull());
    if (!m_livePreview) {
        qCDebug(lcMdk).noquote() << "Player created.";
    }
    m_player->setRenderCallback([this](void *) { QMetaObject::invokeMethod(this, "update"); });
    m_snapshotDirectory = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    connect(this, &MdkObject::urlChanged, this, &MdkObject::fileNameChanged);
    connect(this, &MdkObject::urlChanged, this, &MdkObject::pathChanged);
    connect(this, &MdkObject::positionChanged, this, &MdkObject::positionTextChanged);
    connect(this, &MdkObject::durationChanged, this, &MdkObject::durationTextChanged);
    initMdkHandlers();
    startTimer(50);
}

MdkObject::~MdkObject()
{
    if (!m_livePreview) {
        qCDebug(lcMdk).noquote() << "Player destroyed.";
    }
}

// The beauty of using a true QSGNode: no need for complicated cleanup
// arrangements, unlike in other examples like metalunderqml, because the
// scenegraph will handle destroying the node at the appropriate time.
// Called on the render thread when the scenegraph is invalidated.
void MdkObject::invalidateSceneGraph()
{
    m_node = nullptr;
    if (!m_livePreview) {
        qCDebug(lcMdkRenderer).noquote() << "Scenegraph invalidated.";
    }
}

// Called on the gui thread if the item is removed from scene.
void MdkObject::releaseResources()
{
    m_node = nullptr;
    if (!m_livePreview) {
        qCDebug(lcMdkRenderer).noquote() << "Resources released.";
    }
}

QSGNode *MdkObject::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    Q_UNUSED(data)
    auto n = static_cast<VideoTextureNode *>(node);
    if (!n && (width() <= 0 || height() <= 0)) {
        return nullptr;
    }
    if (!n) {
        m_node = new VideoTextureNode(this);
        n = m_node;
    }
    m_node->sync();
    n->setTextureCoordinatesTransform(QSGSimpleTextureNode::NoTransform);
    n->setFiltering(QSGTexture::Linear);
    n->setRect(0, 0, width(), height());
    // Ensure getting to beforeRendering() at some point.
    window()->update();
    return n;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void MdkObject::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
#else
void MdkObject::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
#endif
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QQuickItem::geometryChange(newGeometry, oldGeometry);
#else
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
#endif
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QUrl MdkObject::url() const
{
    // ### TODO: isStopped() ?
    if (!m_player->url()) {
        return {};
    }
    return QUrl::fromUserInput(QString::fromUtf8(m_player->url()),
                               QCoreApplication::applicationDirPath(),
                               QUrl::AssumeLocalFile);
}

void MdkObject::setUrl(const QUrl &value)
{
    const QUrl now = url();
    if (now.isValid() && (value != now)) {
        Q_EMIT newHistory(now, position());
    }
    const auto realStop = [this]() -> void {
        m_player->setNextMedia(nullptr);
        m_player->setState(MDK_NS::PlaybackState::Stopped);
        m_player->waitFor(MDK_NS::PlaybackState::Stopped);
    };
    if (value.isEmpty()) {
        realStop();
        return;
    }
    if (!value.isValid() || (value == url()) || !isMedia(value)) {
        return;
    }
    realStop();
    //advance(value);
    // The first url may be the same as current url.
    m_player->setMedia(nullptr);
    m_player->setMedia(qUtf8Printable(urlToString(value)));
    Q_EMIT urlChanged();
    m_player->prepare();
    if (autoStart() && !livePreview()) {
        m_player->setState(MDK_NS::PlaybackState::Playing);
    }
}

void MdkObject::setUrls(const QList<QUrl> &value)
{
    m_player->setNextMedia(nullptr);
    if (value.isEmpty()) {
        m_urls.clear();
        Q_EMIT urlsChanged();
        m_next_it = nullptr;
        stop();
        return;
    }
    const QUrl now = url();
    const QUrl first = value.constFirst();
    if (m_urls == value) {
        if (!isPlaying()) {
            if (now.isValid()) {
                play();
            } else {
                play(first);
            }
        }
    } else {
        m_urls = value;
        Q_EMIT urlsChanged();
        if (!now.isValid()) {
            play(first);
            return;
        }
        m_next_it = std::find(m_urls.cbegin(), m_urls.cend(), now);
        if (m_next_it != m_urls.cbegin()) {
            play(first);
            return;
        }
        //advance(now);
    }
}

QList<QUrl> MdkObject::urls() const
{
    return m_urls;
}

bool MdkObject::loop() const
{
    return m_loop;
}

void MdkObject::setLoop(const bool value)
{
    if (m_loop != value) {
        m_loop = value;
        Q_EMIT loopChanged();
    }
}

QString MdkObject::fileName() const
{
    const QUrl source = url();
    return source.isValid() ? (source.isLocalFile() ? source.fileName() : source.toDisplayString())
                            : QString();
}

QString MdkObject::path() const
{
    const QUrl source = url();
    return source.isValid() ? urlToString(source, true) : QString();
}

qint64 MdkObject::position() const
{
    return isStopped() ? 0 : m_player->position();
}

void MdkObject::setPosition(const qint64 value)
{
    if (isStopped() || (value == position())) {
        return;
    }
    seek(value);
}

qint64 MdkObject::duration() const
{
    return m_mediaInfo.duration;
}

QSize MdkObject::videoSize() const
{
    const auto &vs = m_mediaInfo.videoStreams;
    if (vs.isEmpty()) {
        return {};
    }
    const auto &vsf = vs.constFirst();
    return QSize(vsf.width, vsf.height);
}

qreal MdkObject::volume() const
{
    return m_volume;
}

void MdkObject::setVolume(const qreal value)
{
    if (qFuzzyCompare(value, m_volume)) {
        return;
    }
    m_volume = value;
    m_player->setVolume(m_volume);
    Q_EMIT volumeChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkPlayback).noquote() << "Volume -->" << m_volume;
    }
}

bool MdkObject::mute() const
{
    return m_mute;
}

void MdkObject::setMute(const bool value)
{
    if (value == m_mute) {
        return;
    }
    m_mute = value;
    m_player->setMute(m_mute);
    Q_EMIT muteChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkPlayback).noquote() << "Mute -->" << m_mute;
    }
}

bool MdkObject::seekable() const
{
    // Local files are always seekable, in theory.
    return (isLoaded() && url().isLocalFile());
}

MdkObject::PlaybackState MdkObject::playbackState() const
{
    switch (m_player->state()) {
    case MDK_NS::PlaybackState::Playing:
        return PlaybackState::Playing;
    case MDK_NS::PlaybackState::Paused:
        return PlaybackState::Paused;
    case MDK_NS::PlaybackState::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

void MdkObject::setPlaybackState(const MdkObject::PlaybackState value)
{
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::Playing:
        m_player->setState(MDK_NS::PlaybackState::Playing);
        break;
    case PlaybackState::Paused:
        m_player->setState(MDK_NS::PlaybackState::Paused);
        break;
    case PlaybackState::Stopped:
        m_player->setState(MDK_NS::PlaybackState::Stopped);
        break;
    }
}

MdkObject::MediaStatus MdkObject::mediaStatus() const
{
    const auto ms = m_player->mediaStatus();
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::NoMedia)) {
        return MediaStatus::NoMedia;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Unloaded)) {
        return MediaStatus::Unloaded;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Loading)) {
        return MediaStatus::Loading;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Loaded)) {
        return MediaStatus::Loaded;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Prepared)) {
        return MediaStatus::Prepared;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Stalled)) {
        return MediaStatus::Stalled;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Buffering)) {
        return MediaStatus::Buffering;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Buffered)) {
        return MediaStatus::Buffered;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::End)) {
        return MediaStatus::End;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Seeking)) {
        return MediaStatus::Seeking;
    }
    if (MDK_NS::test_flag(ms & MDK_NS::MediaStatus::Invalid)) {
        return MediaStatus::Invalid;
    }
    return MediaStatus::Unknown;
}

MdkObject::LogLevel MdkObject::logLevel() const
{
    switch (_MDKObject_MDK_LogLevel()) {
    case MDK_NS::LogLevel::Off:
        return LogLevel::Off;
    case MDK_NS::LogLevel::Debug:
        return LogLevel::Debug;
    case MDK_NS::LogLevel::Warning:
        return LogLevel::Warning;
    case MDK_NS::LogLevel::Error:
        return LogLevel::Critical;
    case MDK_NS::LogLevel::Info:
        return LogLevel::Info;
    default:
        return LogLevel::Debug;
    }
}

void MdkObject::setLogLevel(const MdkObject::LogLevel value)
{
    if (value == logLevel()) {
        return;
    }
    MDK_NS::LogLevel logLv = MDK_NS::LogLevel::Debug;
    switch (value) {
    case LogLevel::Off:
        logLv = MDK_NS::LogLevel::Off;
        break;
    case LogLevel::Debug:
        logLv = MDK_NS::LogLevel::Debug;
        break;
    case LogLevel::Warning:
        logLv = MDK_NS::LogLevel::Warning;
        break;
    case LogLevel::Critical:
    case LogLevel::Fatal:
        logLv = MDK_NS::LogLevel::Error;
        break;
    case LogLevel::Info:
        logLv = MDK_NS::LogLevel::Info;
        break;
    }
    MDK_NS::SetGlobalOption("logLevel", logLv);
    Q_EMIT logLevelChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Log level -->" << value;
    }
}

qreal MdkObject::playbackRate() const
{
    return static_cast<qreal>(m_player->playbackRate());
}

void MdkObject::setPlaybackRate(const qreal value)
{
    if (isStopped() || (value == playbackRate())) {
        return;
    }
    m_player->setPlaybackRate(value);
    Q_EMIT playbackRateChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkPlayback).noquote() << "Playback rate -->" << value;
    }
}

qreal MdkObject::aspectRatio() const
{
    const QSize vs = videoSize();
    return (static_cast<qreal>(vs.width()) / static_cast<qreal>(vs.height()));
}

void MdkObject::setAspectRatio(const qreal value)
{
    if (isStopped() || (value == aspectRatio())) {
        return;
    }
    m_player->setAspectRatio(value);
    Q_EMIT aspectRatioChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkPlayback).noquote() << "Aspect ratio -->" << value;
    }
}

QString MdkObject::snapshotDirectory() const
{
    return QDir::toNativeSeparators(m_snapshotDirectory);
}

void MdkObject::setSnapshotDirectory(const QString &value)
{
    if (value.isEmpty() || (value == snapshotDirectory())) {
        return;
    }
    const QString val = QDir::toNativeSeparators(value);
    if (val == snapshotDirectory()) {
        return;
    }
    m_snapshotDirectory = val;
    Q_EMIT snapshotDirectoryChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Snapshot directory -->" << m_snapshotDirectory;
    }
}

QString MdkObject::snapshotFormat() const
{
    return m_snapshotFormat;
}

void MdkObject::setSnapshotFormat(const QString &value)
{
    if (value.isEmpty() || (value == m_snapshotFormat)) {
        return;
    }
    m_snapshotFormat = value;
    Q_EMIT snapshotFormatChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Snapshot format -->" << m_snapshotFormat;
    }
}

QString MdkObject::snapshotTemplate() const
{
    return m_snapshotTemplate;
}

void MdkObject::setSnapshotTemplate(const QString &value)
{
    if (value.isEmpty() || (value == m_snapshotTemplate)) {
        return;
    }
    m_snapshotTemplate = value;
    Q_EMIT snapshotTemplateChanged();
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Snapshot template -->" << m_snapshotTemplate;
    }
}

QStringList MdkObject::videoMimeTypes()
{
    return suffixesToMimeTypes(videoSuffixes());
}

QStringList MdkObject::audioMimeTypes()
{
    return suffixesToMimeTypes(audioSuffixes());
}

QString MdkObject::positionText() const
{
    return isStopped() ? QString() : timeToString(position(), currentIsAudio());
}

QString MdkObject::durationText() const
{
    return isStopped() ? QString() : timeToString(duration(), currentIsAudio());
}

bool MdkObject::hardwareDecoding() const
{
    return m_hardwareDecoding;
}

void MdkObject::setHardwareDecoding(const bool value)
{
    if (m_hardwareDecoding != value) {
        m_hardwareDecoding = value;
        if (m_hardwareDecoding) {
            setVideoDecoders(defaultVideoDecoders());
        } else {
            setVideoDecoders({QString::fromUtf8("FFmpeg")});
        }
        Q_EMIT hardwareDecodingChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Hardware decoding -->" << m_hardwareDecoding;
        }
    }
}

QStringList MdkObject::videoDecoders() const
{
    return m_videoDecoders;
}

void MdkObject::setVideoDecoders(const QStringList &value)
{
    if (m_videoDecoders != value) {
        m_videoDecoders = value.isEmpty() ? QStringList{QString::fromUtf8("FFmpeg")} : value;
        m_player->setDecoders(MDK_NS::MediaType::Video, qStringListToStdStringVector(m_videoDecoders));
        Q_EMIT videoDecodersChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Video decoders -->" << m_videoDecoders;
        }
    }
}

QStringList MdkObject::audioDecoders() const
{
    return m_audioDecoders;
}

void MdkObject::setAudioDecoders(const QStringList &value)
{
    if (m_audioDecoders != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioDecoders = value;
        m_player->setDecoders(MDK_NS::MediaType::Audio, qStringListToStdStringVector(m_audioDecoders));
        Q_EMIT audioDecodersChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Audio decoders -->" << m_audioDecoders;
        }
    }
}

QStringList MdkObject::audioBackends() const
{
    return m_audioBackends;
}

void MdkObject::setAudioBackends(const QStringList &value)
{
    if (m_audioBackends != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioBackends = value;
        m_player->setAudioBackends(qStringListToStdStringVector(m_audioBackends));
        Q_EMIT audioBackendsChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Audio backends -->" << m_audioBackends;
        }
    }
}

bool MdkObject::autoStart() const
{
    return m_autoStart;
}

void MdkObject::setAutoStart(const bool value)
{
    if (m_autoStart != value) {
        m_autoStart = value;
        Q_EMIT autoStartChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Auto start -->" << m_autoStart;
        }
    }
}

bool MdkObject::livePreview() const
{
    return m_livePreview;
}

void MdkObject::setLivePreview(const bool value)
{
    if (m_livePreview != value) {
        m_livePreview = value;
        if (m_livePreview) {
            // Disable log output, otherwise they'll mix up with the real
            // player.
            MDK_NS::SetGlobalOption("logLevel", MDK_NS::LogLevel::Off);
            // We only need static images.
            m_player->setState(MDK_NS::PlaybackState::Paused);
            // We don't want the preview window play sound.
            m_player->setMute(true);
            // Decode as soon as possible when media data received. It also
            // ensures the maximum delay of rendered video is one second and no
            // accumulated delay.
            m_player->setBufferRange(0, 1000, true);
            // Prevent player stop playing after EOF is reached.
            m_player->setProperty("continue_at_end", "1");
            // And don't forget to use accurate seek.
        } else {
            // Restore everything to default.
            m_player->setBufferRange(1000, 2000, false);
            m_player->setMute(m_mute);
            m_player->setProperty("continue_at_end", "0");
            //MDK_NS::setLogLevel(MDK_NS::LogLevel::Debug);
        }
        Q_EMIT livePreviewChanged();
    }
}

MdkObject::VideoBackend MdkObject::videoBackend() const
{
    const QQuickWindow *win = window();
    if (win) {
        const QString sgbe = win->sceneGraphBackend();
        if (sgbe.startsWith(QString::fromUtf8("d3d"), Qt::CaseInsensitive)) {
            return VideoBackend::D3D11;
        }
        if (sgbe.startsWith(QString::fromUtf8("vulkan"), Qt::CaseInsensitive)) {
            return VideoBackend::Vulkan;
        }
        if (sgbe.startsWith(QString::fromUtf8("metal"), Qt::CaseInsensitive)) {
            return VideoBackend::Metal;
        }
        if (sgbe.startsWith(QString::fromUtf8("opengl"), Qt::CaseInsensitive)
            || sgbe.startsWith(QString::fromUtf8("gl"), Qt::CaseInsensitive)) {
            return VideoBackend::OpenGL;
        }
    }
    return VideoBackend::Auto;
}

MdkObject::FillMode MdkObject::fillMode() const
{
    return m_fillMode;
}

void MdkObject::setFillMode(const MdkObject::FillMode value)
{
    if (m_fillMode != value) {
        m_fillMode = value;
        switch (m_fillMode) {
        case FillMode::PreserveAspectFit:
            m_player->setAspectRatio(MDK_NS::KeepAspectRatio);
            break;
        case FillMode::PreserveAspectCrop:
            m_player->setAspectRatio(MDK_NS::KeepAspectRatioCrop);
            break;
        case FillMode::Stretch:
            m_player->setAspectRatio(MDK_NS::IgnoreAspectRatio);
            break;
        }
        Q_EMIT fillModeChanged();
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Fill mode -->" << m_fillMode;
        }
    }
}

MdkObject::MediaInfo MdkObject::mediaInfo() const
{
    return m_mediaInfo;
}

void MdkObject::open(const QUrl &value)
{
    if (!value.isValid()) {
        return;
    }
    if ((value != url()) && isMedia(value)) {
        setUrl(value);
    }
    if (!isPlaying()) {
        play();
    }
}

void MdkObject::play()
{
    if (!isPaused() || !url().isValid()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Playing);
}

void MdkObject::play(const QUrl &value)
{
    if (!value.isValid()) {
        return;
    }
    const QUrl source = url();
    if ((value == source) && !isPlaying()) {
        play();
    }
    if ((value != source) && isMedia(value)) {
        open(value);
    }
}

void MdkObject::pause()
{
    if (!isPlaying()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Paused);
}

void MdkObject::stop()
{
    if (isStopped()) {
        return;
    }
    m_player->setNextMedia(nullptr);
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_player->waitFor(MDK_NS::PlaybackState::Stopped);
}

void MdkObject::seek(const qint64 value, const bool keyFrame)
{
    if (isStopped() || (value == position())) {
        return;
    }
    // We have to seek accurately when we are in live preview mode.
    m_player->seek(qBound(qint64(0), value, duration()),
                   (!keyFrame || m_livePreview) ? MDK_NS::SeekFlag::FromStart
                                                : MDK_NS::SeekFlag::Default);
    if (!m_livePreview) {
        qCDebug(lcMdkPlayback).noquote()
            << "Seek -->" << value << '='
            << qRound((static_cast<qreal>(value) / static_cast<qreal>(duration())) * 100) << '%';
    }
}

void MdkObject::rotateImage(const int value)
{
    if (isStopped()) {
        return;
    }
    m_player->rotate(value);
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Rotate -->" << value;
    }
}

void MdkObject::scaleImage(const qreal x, const qreal y)
{
    if (isStopped()) {
        return;
    }
    m_player->scale(x, y);
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Scale -->" << QSizeF{x, y};
    }
}

void MdkObject::snapshot()
{
    if (isStopped()) {
        return;
    }
    MDK_NS::Player::SnapshotRequest snapshotRequest = {};
    m_player->snapshot(&snapshotRequest,
                       [this](MDK_NS::Player::SnapshotRequest *ret, qreal frameTime) {
                           Q_UNUSED(ret)
                           const QString path = QString::fromUtf8("%1%2%3_%4.%5")
                                                    .arg(snapshotDirectory(),
                                                         QDir::separator(),
                                                         fileName(),
                                                         QString::number(frameTime),
                                                         snapshotFormat());
                           if (!m_livePreview) {
                               qCDebug(lcMdkMisc).noquote() << "Taking snapshot -->" << path;
                           }
                           return qUtf8Printable(path);
                       });
}

bool MdkObject::isVideo(const QUrl &value)
{
    if (value.isValid()) {
        return videoSuffixes().contains(QString::fromUtf8("*.")
                                            + QFileInfo(value.fileName()).suffix(),
                                        Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isAudio(const QUrl &value)
{
    if (value.isValid()) {
        return audioSuffixes().contains(QString::fromUtf8("*.")
                                            + QFileInfo(value.fileName()).suffix(),
                                        Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isMedia(const QUrl &value)
{
    return (isVideo(value) || isAudio(value));
}

bool MdkObject::currentIsVideo() const
{
    if (isStopped()) {
        return false;
    }
    return isVideo(url());
}

bool MdkObject::currentIsAudio() const
{
    if (isStopped()) {
        return false;
    }
    return isAudio(url());
}

bool MdkObject::currentIsMedia() const
{
    if (isStopped()) {
        return false;
    }
    return (currentIsVideo() || currentIsAudio());
}

void MdkObject::seekBackward(const int value)
{
    if (isStopped()) {
        return;
    }
    seek(position() - qAbs(value), false);
}

void MdkObject::seekForward(const int value)
{
    if (isStopped()) {
        return;
    }
    seek(position() + qAbs(value), false);
}

void MdkObject::playPrevious()
{
    if (isStopped() || (m_urls.count() < 2)) {
        return;
    }
    auto it = std::find(m_urls.cbegin(), m_urls.cend(), url());
    if (it == m_urls.cbegin()) {
        it = m_urls.cend();
    }
    --it;
    play(*it);
}

void MdkObject::playNext()
{
    if (isStopped() || (m_urls.count() < 2)) {
        return;
    }
    auto it = std::find(m_urls.cbegin(), m_urls.cend(), url());
    if (it != m_urls.cend()) {
        ++it;
    }
    if (it == m_urls.cend()) {
        it = m_urls.cbegin();
    }
    play(*it);
}

void MdkObject::startRecording(const QUrl &value, const QString &format)
{
    if (value.isValid() && value.isLocalFile()) {
        // If media is not loaded, recorder will start when playback starts.
        const QString path = urlToString(value);
        m_player->record(qUtf8Printable(path), format.isEmpty() ? nullptr : qUtf8Printable(format));
        if (!m_livePreview) {
            qCDebug(lcMdkMisc).noquote() << "Start recording -->" << path;
        }
    }
}

void MdkObject::stopRecording()
{
    m_player->record();
    if (!m_livePreview) {
        qCDebug(lcMdkMisc).noquote() << "Recording stopped.";
    }
}

void MdkObject::timerEvent(QTimerEvent *event)
{
    QQuickItem::timerEvent(event);
    if (!isStopped()) {
        Q_EMIT positionChanged();
    }
}

void MdkObject::initMdkHandlers()
{
    MDK_NS::setLogHandler([this](MDK_NS::LogLevel level, const char *msg) {
        if (m_livePreview) {
            return;
        }
        switch (level) {
        case MDK_NS::LogLevel::Info:
            qCInfo(lcMdkLog).noquote() << msg;
            break;
        case MDK_NS::LogLevel::All:
        case MDK_NS::LogLevel::Debug:
            qCDebug(lcMdkLog).noquote() << msg;
            break;
        case MDK_NS::LogLevel::Warning:
            qCWarning(lcMdkLog).noquote() << msg;
            break;
        case MDK_NS::LogLevel::Error:
            qCCritical(lcMdkLog).noquote() << msg;
            break;
        default:
            break;
        }
    });
    m_player->currentMediaChanged([this] {
        const QUrl now = url();
        if (!now.isValid()) {
            return;
        }
        advance(now);
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "Current media -->" << urlToString(now, true);
        }
        Q_EMIT urlChanged();
    });
    m_player->onMediaStatusChanged([this](MDK_NS::MediaStatus ms) {
        if (MDK_NS::flags_added(m_mediaStatus, ms, MDK_NS::MediaStatus::Loaded)) {
            const auto &info = m_player->mediaInfo();
            m_mediaInfo.startTime = info.start_time;
            m_mediaInfo.duration = info.duration;
            m_mediaInfo.bitRate = info.bit_rate;
            m_mediaInfo.fileSize = info.size;
            m_mediaInfo.format = QString::fromUtf8(info.format);
            m_mediaInfo.streamCount = info.streams;
            m_hasVideo = !info.video.empty();
            if (m_hasVideo) {
                m_mediaInfo.videoStreams.clear();
                for (auto &&vsi : qAsConst(info.video)) {
                    VideoStreamInfo vsinfo{};
                    vsinfo.index = vsi.index;
                    vsinfo.startTime = vsi.start_time;
                    vsinfo.duration = vsi.duration;
                    const auto &codec = vsi.codec;
                    vsinfo.codec = QString::fromUtf8(codec.codec);
                    vsinfo.bitRate = codec.bit_rate;
                    vsinfo.frameRate = codec.frame_rate;
                    vsinfo.format = QString::fromUtf8(codec.format_name);
                    vsinfo.width = codec.width;
                    vsinfo.height = codec.height;
                    const auto &metaData = vsi.metadata;
                    if (!metaData.empty()) {
                        for (auto &&data : qAsConst(metaData)) {
                            vsinfo.metaData.insert(QString::fromStdString(data.first),
                                                   QString::fromStdString(data.second));
                        }
                    }
                    m_mediaInfo.videoStreams.append(vsinfo);
                }
                Q_EMIT videoSizeChanged();
            }
            m_hasAudio = !info.audio.empty();
            if (m_hasAudio) {
                m_mediaInfo.audioStreams.clear();
                for (auto &&asi : qAsConst(info.audio)) {
                    AudioStreamInfo asinfo{};
                    asinfo.index = asi.index;
                    asinfo.startTime = asi.start_time;
                    asinfo.duration = asi.duration;
                    const auto &codec = asi.codec;
                    asinfo.codec = QString::fromUtf8(codec.codec);
                    asinfo.bitRate = codec.bit_rate;
                    asinfo.frameRate = codec.frame_rate;
                    asinfo.channels = codec.channels;
                    asinfo.sampleRate = codec.sample_rate;
                    const auto &metaData = asi.metadata;
                    if (!metaData.empty()) {
                        for (auto &&data : qAsConst(metaData)) {
                            asinfo.metaData.insert(QString::fromStdString(data.first),
                                                   QString::fromStdString(data.second));
                        }
                    }
                    m_mediaInfo.audioStreams.append(asinfo);
                }
            }
            // ### TODO: m_hasSubtitle = ...
            // ### TODO: if (m_hasSubtitle) { ... }
            m_hasChapters = !info.chapters.empty();
            if (m_hasChapters) {
                m_mediaInfo.chapters.clear();
                for (auto &&chapter : qAsConst(info.chapters)) {
                    ChapterInfo cpinfo{};
                    cpinfo.beginTime = chapter.start_time;
                    cpinfo.endTime = chapter.end_time;
                    cpinfo.title = QString::fromStdString(chapter.title);
                    m_mediaInfo.chapters.append(cpinfo);
                }
            }
            if (!info.metadata.empty()) {
                m_mediaInfo.metaData.clear();
                for (auto &&data : qAsConst(info.metadata)) {
                    m_mediaInfo.metaData.insert(QString::fromStdString(data.first),
                                                QString::fromStdString(data.second));
                }
            }
            Q_EMIT positionChanged();
            Q_EMIT durationChanged();
            Q_EMIT seekableChanged();
            Q_EMIT mediaInfoChanged();
            Q_EMIT loaded();
            if (!m_livePreview) {
                qCDebug(lcMdkPlayback).noquote() << "Media loaded.";
            }
        }
        m_mediaStatus = ms;
        Q_EMIT mediaStatusChanged();
        return true;
    });
    m_player->onEvent([this](const MDK_NS::MediaEvent &me) {
        if (!m_livePreview) {
            qCDebug(lcMdk).noquote() << "MDK event:" << me.category.data() << me.detail.data();
        }
        return false;
    });
    m_player->onLoop([this](int count) {
        if (!m_livePreview) {
            qCDebug(lcMdkPlayback).noquote() << "loop:" << count;
        }
        return false;
    });
    m_player->onStateChanged([this](MDK_NS::PlaybackState pbs) {
        Q_EMIT playbackStateChanged();
        if (pbs == MDK_NS::PlaybackState::Playing) {
            Q_EMIT playing();
            if (!m_livePreview) {
                qCDebug(lcMdkPlayback).noquote() << "Start playing.";
            }
        }
        if (pbs == MDK_NS::PlaybackState::Paused) {
            Q_EMIT paused();
            if (!m_livePreview) {
                qCDebug(lcMdkPlayback).noquote() << "Paused.";
            }
        }
        if (pbs == MDK_NS::PlaybackState::Stopped) {
            resetInternalData();
            Q_EMIT stopped();
            if (!m_livePreview) {
                qCDebug(lcMdkPlayback).noquote() << "Stopped.";
            }
        }
    });
}

void MdkObject::resetInternalData()
{
    // Make sure MdkObject::url() returns empty.
    m_player->setMedia(nullptr);
#if 0
    advance();
    if (m_next_it == nullptr) {
        m_urls.clear();
        Q_EMIT urlsChanged();
    } else {
        if (m_next_it == m_urls.cbegin()) {
            m_next_it = m_urls.cend();
        }
        --m_next_it;
    }
#endif
    // ----------------
    m_hasVideo = false;
    m_hasAudio = false;
    m_hasSubtitle = false;
    m_hasChapters = false;
    //m_loop = false;
    m_mediaInfo = {};
    m_mediaStatus = MDK_NS::MediaStatus::NoMedia;
    Q_EMIT urlChanged();
    Q_EMIT positionChanged();
    Q_EMIT durationChanged();
    Q_EMIT seekableChanged();
    Q_EMIT mediaInfoChanged();
    //Q_EMIT loopChanged();
    Q_EMIT mediaStatusChanged();
}

void MdkObject::advance()
{
    if (m_next_it == nullptr) {
        return;
    }
    if (m_next_it != m_urls.cend()) {
        ++m_next_it;
    }
    if (m_next_it == m_urls.cend()) {
        if (m_loop) {
            m_next_it = m_urls.cbegin();
        } else {
            m_next_it = nullptr;
        }
    }
}

void MdkObject::advance(const QUrl &value)
{
    if (value.isValid()) {
        m_next_it = std::find(m_urls.cbegin(), m_urls.cend(), value);
        advance();
    }
    m_player->setNextMedia(nullptr);
    if (m_next_it == nullptr) {
        return;
    }
    const QUrl nextUrl = *m_next_it;
    if (nextUrl.isValid()) {
        m_player->setNextMedia(qUtf8Printable(urlToString(nextUrl)));
    }
    advance();
}

bool MdkObject::isLoaded() const
{
    return !isStopped();
}

bool MdkObject::isPlaying() const
{
    return (m_player->state() == MDK_NS::PlaybackState::Playing);
}

bool MdkObject::isPaused() const
{
    return (m_player->state() == MDK_NS::PlaybackState::Paused);
}

bool MdkObject::isStopped() const
{
    return (m_player->state() == MDK_NS::PlaybackState::Stopped);
}

#include "mdkobject.moc"
