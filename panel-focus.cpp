/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Scott Moreau <oreaus@gmail.com>
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

#include <wayfire/plugin.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/view-helpers.hpp>
#include <linux/input-event-codes.h>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/plugins/ipc/ipc-activator.hpp>


namespace wf
{
class wayfire_panel_focus : public wf::plugin_interface_t
{
    wf::ipc_activator_t cycle{"panel-focus/cycle"};
    wayfire_view current_focus_view, toplevel_focus_view;

  public:

    void init() override
    {
        cycle.set_handler(cycle_panels);
        current_focus_view = wf::get_core().seat->get_active_view();
        toplevel_focus_view = nullptr;
        wf::get_core().connect(&on_key_event);
        wf::get_core().connect(&on_view_mapped);
        wf::get_core().connect(&on_view_unmapped);
        wf::get_core().connect(&on_button_event);
    }

    wf::signal::connection_t<wf::input_event_signal<wlr_keyboard_key_event>> on_key_event =
        [=] (wf::input_event_signal<wlr_keyboard_key_event> *ev)
    {
        if (!ev || !ev->event)
        {
            return;
        }

        if (ev->event->keycode == KEY_ESC && ev->event->state == WL_KEYBOARD_KEY_STATE_RELEASED)
        {
            if (toplevel_focus_view)
            {
                wf::get_core().seat->focus_view(toplevel_focus_view);
                current_focus_view = toplevel_focus_view;
                toplevel_focus_view = nullptr;
            }
        }
    };

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_button_event>> on_button_event =
        [=] (wf::input_event_signal<wlr_pointer_button_event> *ev)
    {
		auto view = wf::get_core().seat->get_active_view();
        if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT)
        {
            wf::get_core().seat->focus_view(toplevel_focus_view);
        }
    };

    wf::signal::connection_t<wf::view_mapped_signal> on_view_mapped = [=] (wf::view_mapped_signal *ev)
    {
        if (ev && ev->view->role == wf::VIEW_ROLE_TOPLEVEL &&
            wf::get_view_layer(ev->view) == wf::scene::layer::WORKSPACE)
        {
            toplevel_focus_view = ev->view;
        }
    };

    wf::signal::connection_t<wf::view_unmapped_signal> on_view_unmapped = [=] (wf::view_unmapped_signal *ev)
    {
        if (ev->view == wf::get_core().seat->get_active_view())
        {
            toplevel_focus_view = nullptr;
        }
    };

    wf::ipc_activator_t::handler_t cycle_panels = [=] (wf::output_t *output, wayfire_view)
    {
        wayfire_view last_focus_view = current_focus_view;
        bool focus_view_found = false;
        if (!toplevel_focus_view)
        {
            auto view = wf::get_core().seat->get_active_view();
            if (view->role == wf::VIEW_ROLE_TOPLEVEL &&
                wf::get_view_layer(view) == wf::scene::layer::WORKSPACE)
            {
                toplevel_focus_view = view;
            }
        }
        for (auto& view : wf::get_core().get_all_views())
        {
            if (view == current_focus_view && !focus_view_found)
            {
                focus_view_found = true;
                continue;
            }
            if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT &&
                wf::get_view_layer(view) > wf::scene::layer::WORKSPACE &&
                focus_view_found)
            {
                wf::get_core().seat->focus_view(view);
                current_focus_view = view;
                break;
            }
        }
        if (last_focus_view == current_focus_view)
        {
            for (auto& view : wf::get_core().get_all_views())
            {
                if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT &&
                    wf::get_view_layer(view) > wf::scene::layer::WORKSPACE)
                {
                    wf::get_core().seat->focus_view(view);
                    current_focus_view = view;
                    break;
                }
            }
        }
        if (last_focus_view == current_focus_view)
        {
            LOGI("Failed to cycle views!");
        }
        return true;
    };

    void fini() override
    {
        on_key_event.disconnect();
        on_view_mapped.disconnect();
        on_view_unmapped.disconnect();
        on_button_event.disconnect();
    }
};
}

DECLARE_WAYFIRE_PLUGIN(wf::wayfire_panel_focus);
