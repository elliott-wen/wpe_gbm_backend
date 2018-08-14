/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * Copyright (C) 2015, 2016 Metrological
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

#include "inputserver.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <wpe/wpe.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
struct event_header_t
{ 
  unsigned char type;
  int x;
  int y;
  uint32_t keycode;
};

#define EVENT_TOUCH_DOWN 0xa1
#define EVENT_TOUCH_UP 0xa2
#define EVENT_TOUCH_MOTION 0xa3
#define EVENT_KEYBOARD_DOWN 0xa4
#define EVENT_KEYBOARD_UP 0xa5

#define INPUT_SERVER_PATH "/tmp/webkit_input"
namespace WPE {



LibinputServer& LibinputServer::singleton()
{
    static LibinputServer server;
    return server;
}

LibinputServer::LibinputServer()
{ 
    m_client = 0;
    unixFD = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(unixFD <= 0)
    {
        fprintf(stderr, "Unable to init a unix socket\n");
        abort();
    }
    
    int tflags = fcntl(unixFD, F_GETFL, 0);
    fcntl(unixFD, F_SETFL, tflags | O_NONBLOCK);

    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, INPUT_SERVER_PATH, 64);
    unlink(client_addr.sun_path);

    if(bind(unixFD,  (struct sockaddr*) (&client_addr), (socklen_t) sizeof(client_addr)) == -1)
    {
        printf("Unable to send bind clientaddr \n");
        close(unixFD);
        abort();
    }

    GSource* baseSource = g_source_new(&EventSource::s_sourceFuncs, sizeof(EventSource));
    auto* source = reinterpret_cast<EventSource*>(baseSource);
    source->pfd.fd = unixFD;
    source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source->pfd.revents = 0;
    g_source_add_poll(baseSource, &source->pfd);
    source->server = this;

    g_source_set_name(baseSource, "[WPE] libinput");
    g_source_set_priority(baseSource, G_PRIORITY_DEFAULT);
    g_source_attach(baseSource, g_main_context_get_thread_default());


    m_keyboardEventHandler = Input::KeyboardEventHandler::create();

    fprintf(stderr, "[LibinputServer] Initialization of linux input system succeeded.\n");
}

LibinputServer::~LibinputServer()
{
    if(unixFD >= 0)
        close(unixFD);
}
void LibinputServer::setClient(Client* client)
{
    m_client = client;
}

void LibinputServer::handleKeyboardEvent(struct wpe_input_keyboard_event* actionEvent)
{
    if (m_client)
        m_client->handleKeyboardEvent(actionEvent);
}


void LibinputServer::processEvents()
{
    

    
    while(1)
    {
        struct event_header_t t;
        int byteR = read(unixFD, &t, sizeof(struct event_header_t));
        if(byteR <= 0)
            break;
        if(!m_client)
            continue;
        uint32_t currentT = (uint32_t)time(NULL);
        if(t.type == EVENT_TOUCH_DOWN)
        {
         
            struct wpe_input_touch_event_raw rawEvent{wpe_input_touch_event_type_down, currentT, 0, t.x, t.y};
            struct wpe_input_touch_event dispatchedEvent{ &rawEvent, 1, wpe_input_touch_event_type_down, 0, currentT };
            m_client->handleTouchEvent(&dispatchedEvent);
        }
        else if(t.type == EVENT_TOUCH_UP)
        {
           
            struct wpe_input_touch_event_raw rawEvent{wpe_input_touch_event_type_up, currentT, 0, t.x, t.y};
            struct wpe_input_touch_event dispatchedEvent{ &rawEvent, 1, wpe_input_touch_event_type_up, 0, currentT };
            m_client->handleTouchEvent(&dispatchedEvent);
        }
        else if(t.type == EVENT_TOUCH_MOTION)
        {
             
        }
        else if(t.type == EVENT_KEYBOARD_DOWN)
        {
                //printf("Handle %d down\n", t.keycode);
                struct wpe_input_keyboard_event rawEvent{ currentT, t.keycode, 0, true, 0 };
                Input::KeyboardEventHandler::Result result = m_keyboardEventHandler->handleKeyboardEvent(&rawEvent);
                struct wpe_input_keyboard_event event{ rawEvent.time, std::get<0>(result), std::get<1>(result), rawEvent.pressed, std::get<2>(result) };
                m_client->handleKeyboardEvent(&event);
        }
        else if(t.type == EVENT_KEYBOARD_UP)
        {
            //printf("Handle %d up\n", t.keycode);
                struct wpe_input_keyboard_event rawEvent{ currentT, t.keycode, 0, false, 0 };
                Input::KeyboardEventHandler::Result result = m_keyboardEventHandler->handleKeyboardEvent(&rawEvent);
                struct wpe_input_keyboard_event event{ rawEvent.time, std::get<0>(result), std::get<1>(result), rawEvent.pressed, std::get<2>(result) };
                m_client->handleKeyboardEvent(&event);
        }
        else
        {
            fprintf(stderr, "Unknown event %d\n", t.type);
        }
    }
    


}

GSourceFuncs LibinputServer::EventSource::s_sourceFuncs = {
    nullptr, // prepare
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        return !!source->pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        if (source->pfd.revents & G_IO_IN)
            source->server->processEvents();
        source->pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};



} // namespace WPE