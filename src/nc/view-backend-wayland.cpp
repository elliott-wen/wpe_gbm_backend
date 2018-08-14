/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "nested-compositor.h"

#define WL_HIDE_DEPRECATED 1

#include "display.h"
#include "nc-renderer-host.h"
#include "nc-view-display.h"
#include "ivi-application-client-protocol.h"
#include "wayland-drm-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <unordered_map>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-server-core.h>
#include "wayland-drm-server-protocol.h"
#include <wayland-server-protocol.h>
#include <wpe/wpe.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace NC {
namespace Wayland {

class ViewBackend: public NC::ViewDisplay::Client {
public:
    ViewBackend(struct wpe_view_backend*);
    virtual ~ViewBackend();

    virtual void OnSurfaceAttach(NC::ViewDisplay::Surface const&, NC::ViewDisplay::Buffer const*) override;

    virtual void OnSurfaceCommit(NC::ViewDisplay::Surface const&, NC::ViewDisplay::Buffer const*,
            NC::ViewDisplay::FrameCallback const*, NC::ViewDisplay::Damage const*) override;

    void initialize();

    struct wpe_view_backend* backend() { return m_backend; }

    struct ResizingData {
        struct wpe_view_backend* backend;
        uint32_t width;
        uint32_t height;
    };

private:
    ::Wayland::Display& m_display;
    struct wpe_view_backend* m_backend;

    struct wl_surface* m_surface;
    struct xdg_surface* m_xdgSurface;
    struct ivi_surface* m_iviSurface;
    struct wl_cursor_theme* m_cursorTheme {nullptr};

    ResizingData m_resizingData { nullptr, 0, 0 };

    struct {
        struct wl_global* drm;
    } m_server;

    NC::ViewDisplay m_viewDisplay;

    static const struct wl_drm_interface g_drmImplementation;
    static const struct wl_drm_listener g_drmListener;
    static const struct wl_buffer_interface g_drmBufferImplementation;
    static const struct wl_buffer_listener g_drmBufferListener;

    static void bindDrm(struct wl_client*, void*, uint32_t, uint32_t);

    static void destroyBuffer(struct wl_resource*);
    static void destroyDrm(struct wl_resource*);
};

static const struct xdg_surface_listener g_xdgSurfaceListener = {
    // configure
    [](void* data, struct xdg_surface* surface, int32_t width, int32_t height, struct wl_array*, uint32_t serial)
    {
        if( width != 0 || height != 0 ) {
            auto* resizeData = static_cast<ViewBackend::ResizingData*>(data);
            wpe_view_backend_dispatch_set_size(resizeData->backend, std::max(0, width), std::max(0, height));
            resizeData->width = width;
            resizeData->height = height;
        }
        xdg_surface_ack_configure(surface, serial);
    },
    // delete
    [](void*, struct xdg_surface*) { },
};

static const struct ivi_surface_listener g_iviSurfaceListener = {
    // configure
    [](void* data, struct ivi_surface*, int32_t width, int32_t height)
    {
        auto* resizeData = static_cast<ViewBackend::ResizingData*>(data);
        wpe_view_backend_dispatch_set_size(resizeData->backend, std::max(0, width), std::max(0, height));
        resizeData->width = width;
        resizeData->height = height;
    },
};

const struct wl_buffer_interface ViewBackend::g_drmBufferImplementation = {
    // destroy
    [](struct wl_client* client, struct wl_resource* resource)
    {
        auto* buffer = static_cast<struct wl_buffer*>(wl_resource_get_user_data(resource));
        wl_buffer_destroy(buffer);
        wl_resource_set_user_data(resource, nullptr);
    },
};

const struct wl_buffer_listener ViewBackend::g_drmBufferListener = {
    // release
    [](void* data, struct wl_buffer* buffer)
    {
        auto* resource = static_cast<struct wl_resource*>(data);
        wl_buffer_send_release(resource);
    },
};

void ViewBackend::destroyBuffer(struct wl_resource* resource)
{
    auto* buffer = static_cast<struct wl_buffer*>(wl_resource_get_user_data(resource));
    if (buffer)
        wl_buffer_destroy(buffer);
}

struct drm {
    struct wl_drm* drm;
    struct wl_display* display;
};

const struct wl_drm_interface ViewBackend::g_drmImplementation = {
    // authenticate
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        auto* drm = static_cast<struct drm*>(wl_resource_get_user_data(resource));
        wl_drm_authenticate(drm->drm, id);
        wl_display_roundtrip(drm->display);
    },
    // create_buffer
	[](struct wl_client *client, struct wl_resource *resource, uint32_t id, uint32_t name,
            int32_t width, int32_t height, uint32_t stride, uint32_t format)
    {
        auto* drm = static_cast<struct drm*>(wl_resource_get_user_data(resource));

        struct wl_resource* buffer_resource = wl_resource_create(client, &wl_buffer_interface,
                1, id);

        if (!buffer_resource) {
            wl_resource_post_no_memory(resource);
            return;
        }

        struct wl_buffer* buffer = wl_drm_create_buffer(drm->drm, name, width,
                height, stride, format);

        wl_resource_set_implementation(buffer_resource, &g_drmBufferImplementation, buffer,
                destroyBuffer);

        wl_buffer_add_listener(buffer, &g_drmBufferListener, buffer_resource);
    },
    // create_planar_buffer
    [](struct wl_client *client, struct wl_resource *resource, uint32_t id, uint32_t name,
            int32_t width, int32_t height, uint32_t format, int32_t offset0, int32_t stride0,
            int32_t offset1, int32_t stride1, int32_t offset2, int32_t stride2)
    {
        auto* drm = static_cast<struct drm*>(wl_resource_get_user_data(resource));

        struct wl_resource* buffer_resource = wl_resource_create(client, &wl_buffer_interface,
                1, id);

        if (!buffer_resource) {
            wl_resource_post_no_memory(resource);
            return;
        }

        struct wl_buffer* buffer = wl_drm_create_planar_buffer(drm->drm, name, width, height, format,
                offset0, stride0, offset1, stride1, offset2, stride2);

        wl_resource_set_implementation(buffer_resource, &g_drmBufferImplementation, buffer,
                destroyBuffer);

        wl_buffer_add_listener(buffer, &g_drmBufferListener, buffer_resource);
    },
	// create_prime_buffer
    [](struct wl_client *client, struct wl_resource *resource, uint32_t id, int32_t name,
            int32_t width, int32_t height, uint32_t format, int32_t offset0, int32_t stride0,
            int32_t offset1, int32_t stride1, int32_t offset2, int32_t stride2)
    {
        auto* drm = static_cast<struct drm*>(wl_resource_get_user_data(resource));

        struct wl_resource* buffer_resource = wl_resource_create(client, &wl_buffer_interface,
                1, id);

        if (!buffer_resource) {
            wl_resource_post_no_memory(resource);
            return;
        }

        struct wl_buffer* buffer = wl_drm_create_prime_buffer(drm->drm, name, width, height, format,
                offset0, stride0, offset1, stride1, offset2, stride2);

        wl_resource_set_implementation(buffer_resource, &g_drmBufferImplementation, buffer,
                destroyBuffer);

        wl_buffer_add_listener(buffer, &g_drmBufferListener, buffer_resource);
    },
};

void ViewBackend::destroyDrm(struct wl_resource* resource)
{
    auto* drm = static_cast<struct drm*>(wl_resource_get_user_data(resource));
    wl_drm_destroy(drm->drm);
    delete drm;
}

void ViewBackend::bindDrm(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    auto& backend = *static_cast<ViewBackend*>(data);
    struct wl_resource* resource = wl_resource_create(client, &wl_drm_interface,
        version, id);

    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    struct wl_drm* drm = static_cast<struct wl_drm*>(wl_registry_bind(backend.m_display.registry(),
                backend.m_display.interfaces().drm_name, &wl_drm_interface, version));

    wl_drm_add_listener(drm, &g_drmListener, resource);

    struct drm* drm_data = new struct drm;
    drm_data->drm = drm;
    drm_data->display = backend.m_display.display();

    wl_resource_set_implementation(resource, &g_drmImplementation, drm_data, destroyDrm);

    wl_display_roundtrip(backend.m_display.display());
}

const struct wl_drm_listener ViewBackend::g_drmListener = {
    // device
    [](void* data, struct wl_drm*, char const* name)
    {
        auto* resource = static_cast<struct wl_resource*>(data);
        wl_drm_send_device(resource, name);
    },
    // format
    [](void* data, struct wl_drm*, uint32_t format)
    {
        auto* resource = static_cast<struct wl_resource*>(data);
        wl_drm_send_format(resource, format);
    },
    // authenticated
    [](void* data, struct wl_drm*)
    {
        auto* resource = static_cast<struct wl_resource*>(data);
        wl_drm_send_authenticated(resource);
    },
    // capabilities
    [](void* data, struct wl_drm*, uint32_t capabilities)
    {
        auto* resource = static_cast<struct wl_resource*>(data);
        wl_drm_send_capabilities(resource, capabilities);
    },
};


ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : m_display(::Wayland::Display::singleton())
    , m_backend(backend)
    , m_viewDisplay(this)
{
    m_surface = wl_compositor_create_surface(m_display.interfaces().compositor);

    if (m_display.interfaces().xdg) {
        m_xdgSurface = xdg_shell_get_xdg_surface(m_display.interfaces().xdg, m_surface);
        xdg_surface_add_listener(m_xdgSurface, &g_xdgSurfaceListener, &m_resizingData);
        xdg_surface_set_title(m_xdgSurface, "WPE");
    }

    if (m_display.interfaces().ivi_application) {
        m_iviSurface = ivi_application_surface_create(m_display.interfaces().ivi_application,
            4200 + getpid(), // a unique identifier
            m_surface);
        ivi_surface_add_listener(m_iviSurface, &g_iviSurfaceListener, &m_resizingData);
    }

    // Ensure the Pasteboard singleton is constructed early.
    // FIXME:
    // Pasteboard::Pasteboard::singleton();

    m_resizingData.backend = m_backend;

    auto& server = NC::RendererHost::singleton();
    server.initialize();

    m_viewDisplay.initialize(server.display());

    m_server.drm = wl_global_create(server.display(), &wl_drm_interface, m_display.interfaces().drm_version,
            this, bindDrm);


    if (m_display.interfaces().shm)
        m_cursorTheme = wl_cursor_theme_load(NULL, 32, m_display.interfaces().shm);

    if (m_cursorTheme)
        m_display.setCursor(wl_cursor_theme_get_cursor(m_cursorTheme, "left_ptr"));
}

ViewBackend::~ViewBackend()
{
    m_backend = nullptr;

    m_display.unregisterInputClient(m_surface);
    m_display.setCursor(nullptr);

    m_resizingData = { nullptr, 0, 0 };

    if (m_iviSurface)
        ivi_surface_destroy(m_iviSurface);
    m_iviSurface = nullptr;
    if (m_xdgSurface)
        xdg_surface_destroy(m_xdgSurface);
    m_xdgSurface = nullptr;
    if (m_surface)
        wl_surface_destroy(m_surface);
    if (m_cursorTheme)
        wl_cursor_theme_destroy(m_cursorTheme);

    m_surface = nullptr;
}

void ViewBackend::OnSurfaceAttach(NC::ViewDisplay::Surface const&, NC::ViewDisplay::Buffer const* buffer)
{
    struct wl_buffer* b = nullptr;
    int32_t x = 0;
    int32_t y = 0;

    if (buffer) {
        b = static_cast<struct wl_buffer*>(wl_resource_get_user_data(buffer->resource()));
        x = buffer->X();
        y = buffer->Y();
    }

    wl_surface_attach(m_surface, b, x, y);
}

void ViewBackend::OnSurfaceCommit(NC::ViewDisplay::Surface const&, NC::ViewDisplay::Buffer const*,
        NC::ViewDisplay::FrameCallback const* frameCallback, NC::ViewDisplay::Damage const* damage)
{
    if (damage)
        wl_surface_damage(m_surface, damage->X(), damage->Y(), damage->width(),
                damage->height());

    if (frameCallback) {
        static const struct wl_callback_listener frameCallbackListener = {
            // done
            [](void* data, struct wl_callback*, uint32_t callback_data)
            {
                auto* callback_resource = static_cast<struct wl_resource*>(data);
                wl_callback_send_done(callback_resource, callback_data);
            },
        };

        struct wl_callback* callback = wl_surface_frame(m_surface);

        wl_resource_set_implementation(frameCallback->resource(), nullptr, callback,
                [](struct wl_resource* resource)
                {
                    auto* callback = static_cast<struct wl_callback*>(wl_resource_get_user_data(resource));
                    wl_callback_destroy(callback);
                });

        wl_callback_add_listener(callback, &frameCallbackListener, frameCallback->resource());
    }

    wl_surface_commit(m_surface);
}

void ViewBackend::initialize()
{
    m_display.registerInputClient(m_surface, m_backend);
}
} // namespace Wayland
} // namespace NC

extern "C" {

struct wpe_view_backend_interface nc_view_backend_wayland_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new NC::Wayland::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<NC::Wayland::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data) {
        auto* backend = static_cast<NC::Wayland::ViewBackend*>(data);
        backend->initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        return -1;
    },
};

}
