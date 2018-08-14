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

#include "view-backend-drm.h"

#include "ipc.h"
#include "ipc-gbm.h"
#include "inputserver.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <glib.h>
#include <unordered_map>
#include <utility>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>


#define EWIDTH 1664
#define EHEIGHT 2304
#define REMOTE_X11_ENDPOINT "/tmp/webkit_opengl"
namespace DRM {



class ViewBackend;

struct PageFlipHandlerData {
    ViewBackend* backend;
    //char *internal_fb_mem;
    std::pair<bool, uint32_t> nextFB;
    std::pair<bool, uint32_t> lockedFB;
};

class ViewBackend : public IPC::Host::Handler, WPE::Client {
public:
    ViewBackend(struct wpe_view_backend*);
    virtual ~ViewBackend();

    // IPC::Host::Handler
    void handleFd(int) override;
    void handleMessage(char*, size_t) override;

    void handleKeyboardEvent(struct wpe_input_keyboard_event* event);


    void handlePointerEvent(struct wpe_input_pointer_event* event);


    void handleAxisEvent(struct wpe_input_axis_event* event);


    void handleTouchEvent(struct wpe_input_touch_event* event);



    struct wpe_view_backend* backend;

    
    struct {
        int fd { -1 };
        struct gbm_device* device;
    } m_gbm;

    struct {
        std::unordered_map<uint32_t, std::pair<struct gbm_bo*, uint32_t>> fbMap;
        PageFlipHandlerData pageFlipData;
        
    } m_display;

    struct {
        IPC::Host ipcHost;
        int pendingBufferFd { -1 };
    } m_renderer;


};

static int _send_fd(int s, int fd)
{
    char buf[1];
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    unsigned int n;
    char cms[CMSG_SPACE(sizeof(int))];
    
    buf[0] = 0;
    iov.iov_base = buf;
    iov.iov_len = 1;

    memset(&msg, 0, sizeof msg);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = (caddr_t)cms;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    memmove(CMSG_DATA(cmsg), &fd, sizeof(int));

    if((n=sendmsg(s, &msg, 0)) != iov.iov_len)
    {
        printf("Err %d\n", errno);
        return -1;
    }
    return 0;
}
static int gbm_deliver_frame_to_x11(int postfd, char *result)
{
    
    char reply = 0;
    struct sockaddr_un addr;
    int unixfd =  socket(PF_UNIX, SOCK_STREAM, 0);
    if(unixfd < 0)
    {
        printf("failed to create unix socket\n");
        return -1;
    }
    

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, REMOTE_X11_ENDPOINT);
    if (connect(unixfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        printf("failed to connect unix socket\n");
        close(unixfd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(unixfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    if(_send_fd(unixfd, postfd) == -1)
    {
        printf("failed to send unix socket \n");
        close(unixfd);
        return -1;
    }
    //printf("Okay\n");

    if(recv(unixfd, &reply, sizeof(reply), 0) != sizeof(reply))
    {
        printf("failed to recv unix socket\n");
        close(unixfd);
        return -1;
    }
    close(unixfd);
    if(result != NULL)
    {
        *result = reply;
    }
    return 0;
}



static void* pageFlipHandler(void* data)
{
    // /printf("Flip\n");
    usleep(1000000/30); //FPS

   
    


    auto& handlerData = *static_cast<PageFlipHandlerData*>(data);
    if (!handlerData.backend)
        return 0;

    {
        IPC::Message message;
        IPC::GBM::FrameComplete::construct(message);
        handlerData.backend->m_renderer.ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }

    auto bufferToRelease = handlerData.lockedFB;
    handlerData.lockedFB = handlerData.nextFB;
    handlerData.nextFB = { false, 0 };

    if (bufferToRelease.first) {
        IPC::Message message;
        IPC::GBM::ReleaseBuffer::construct(message, bufferToRelease.second);
        handlerData.backend->m_renderer.ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
    return 0;
}

ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : backend(backend)
{
    // m_display.pageFlipData.internal_fb_mem = (char*) malloc(EWIDTH * EHEIGHT* 4);

    // if(!m_display.pageFlipData.internal_fb_mem)
    //     return;

    const char* renderCard = "/dev/dri/renderD128";
    m_gbm.fd = open(renderCard, O_RDWR | O_CLOEXEC);
    if (m_gbm.fd < 0) {
        fprintf(stderr, "ViewBackend: couldn't connect DRM to card %s\n", renderCard);
        return;
    }


    m_gbm.device = gbm_create_device(m_gbm.fd);
    if (!m_gbm.device)
    {
        fprintf(stderr, "ViewBackend: couldn't open gbm\n");
        close(m_gbm.fd);
        return;
    }
   

    fprintf(stderr, "ViewBackend: successfully initialized DRM.\n");

    m_display.pageFlipData.backend = this;

    m_renderer.ipcHost.initialize(*this);

    WPE::Client* inputClient = this;

    WPE::LibinputServer::singleton().setClient(inputClient);


}

ViewBackend::~ViewBackend()
{
    m_renderer.ipcHost.deinitialize();

    m_display.fbMap = { };

    // if(m_display.pageFlipData.internal_fb_mem)
    //     free(m_display.pageFlipData.internal_fb_mem);
    m_display.pageFlipData = { nullptr, { }, { } };

   

    if (m_gbm.device)
        gbm_device_destroy(m_gbm.device);
    m_gbm = { };

   

    
}

void ViewBackend::handleFd(int fd)
{
    if (m_renderer.pendingBufferFd != -1)
        close(m_renderer.pendingBufferFd);
    m_renderer.pendingBufferFd = fd;
}

void ViewBackend::handleKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    wpe_view_backend_dispatch_keyboard_event(backend, event);
}

void ViewBackend::handlePointerEvent(struct wpe_input_pointer_event* event)
{
    wpe_view_backend_dispatch_pointer_event(backend, event);
}

void ViewBackend::handleAxisEvent(struct wpe_input_axis_event* event)
{
    wpe_view_backend_dispatch_axis_event(backend, event);
}

void ViewBackend::handleTouchEvent(struct wpe_input_touch_event* event)
{
    wpe_view_backend_dispatch_touch_event(backend, event);
}


void ViewBackend::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    if (message.messageCode != IPC::GBM::BufferCommit::code)
        return;

    auto& bufferCommit = IPC::GBM::BufferCommit::cast(message);
    uint32_t fbID = 0;
    struct gbm_bo* our_bo = 0; 
    if (m_renderer.pendingBufferFd >= 0) {
        int fd = m_renderer.pendingBufferFd;
        m_renderer.pendingBufferFd = -1;

        assert(m_display.fbMap.find(bufferCommit.handle) == m_display.fbMap.end());

        struct gbm_import_fd_data fdData = { fd, bufferCommit.width, bufferCommit.height, bufferCommit.stride, bufferCommit.format };
        struct gbm_bo* bo = gbm_bo_import(m_gbm.device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_SCANOUT);
        //uint32_t primeHandle = gbm_bo_get_handle(bo).u32;



        // int ret = drmModeAddFB(m_drm.fd, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
        //     24, 32, gbm_bo_get_stride(bo), primeHandle, &fbID);
        // if (ret) {
        //     fprintf(stderr, "ViewBackend: failed to add FB: %s, fbID %d\n", strerror(errno), fbID);
        //     return;
        // }

        m_display.fbMap.insert({ bufferCommit.handle, { bo, fbID } });
        m_display.pageFlipData.nextFB = { true, bufferCommit.handle };

        our_bo = bo;
        //printf("h2\n");
    } else {
        auto it = m_display.fbMap.find(bufferCommit.handle);
        assert(it != m_display.fbMap.end());

        fbID = it->second.second;
        our_bo = it->second.first;
        m_display.pageFlipData.nextFB = { true, bufferCommit.handle };
        //printf("h1\n");
    }

    // struct timeval diff, startTV, endTV;
    // gettimeofday(&startTV, NULL); 

    // uint32_t stride = 0;
    // void *map_data = 0;
    // void *addr=0;
    // addr = gbm_bo_map(our_bo, 0, 0, gbm_bo_get_width(our_bo), gbm_bo_get_height(our_bo), GBM_BO_TRANSFER_READ, &stride, &map_data );
    // printf("%d  %d %d %d %d\n", gbm_bo_get_width(our_bo), EWIDTH, gbm_bo_get_height(our_bo), EHEIGHT, stride);
    // if(addr == 0)
    // {
    //         fprintf(stderr, "ViewBackend: failed to lock bo\n");
    //         abort();
    // }
    // static int ti = 0;
    // ti ++;
    // //printf("Bo W %d WS %d H %d WH %d S  %d\n", gbm_bo_get_width(our_bo), EWIDTH, gbm_bo_get_height(our_bo), EHEIGHT, stride);
    //   FILE *fp;
    //   char name[256];
    //   snprintf(name, 255, "raw%d.dat", ti);
    //   fp = fopen(name , "wb" );
      
    //   if(fwrite(addr , 1 ,  EWIDTH * EHEIGHT * 4 , fp ) != EWIDTH * EHEIGHT * 4)
    //   {
    //     abort();
    //   }
      
    //   fclose(fp);
   
   

    // gbm_bo_unmap(our_bo, map_data);

    int fb_share = gbm_bo_get_fd(our_bo);
    if(fb_share < 0)
    {
        printf("Unable to dump fd\n");
        abort();
    }
    //printf("%d\n", fb_share);
    gbm_deliver_frame_to_x11(fb_share, NULL);

    close(fb_share);

    pthread_t inc_x_thread;

    pthread_create(&inc_x_thread, NULL, pageFlipHandler, &m_display.pageFlipData);

    //  gettimeofday(&endTV, NULL); 

    //  timersub(&endTV, &startTV, &diff);

    // printf("**time taken = %ld %ld\n", diff.tv_sec, diff.tv_usec);
    // int ret = drmModePageFlip(m_drm.fd, m_drm.crtcId, fbID, DRM_MODE_PAGE_FLIP_EVENT, &m_display.pageFlipData);
    // if (ret)
    //     fprintf(stderr, "ViewBackend: failed to queue page flip\n");
}

} // namespace DRM

extern "C" {

struct wpe_view_backend_interface drm_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new DRM::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<DRM::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto* backend = static_cast<DRM::ViewBackend*>(data);
        wpe_view_backend_dispatch_set_size(backend->backend,
            EWIDTH, EHEIGHT);
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto* backend = static_cast<DRM::ViewBackend*>(data);
        return backend->m_renderer.ipcHost.releaseClientFD();
    },
};

}
