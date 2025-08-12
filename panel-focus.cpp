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
#include <wayfire/matcher.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/view-helpers.hpp>
#include <wayfire/view-transform.hpp>
#include <linux/input-event-codes.h>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/plugins/ipc/ipc-activator.hpp>


namespace wf
{
namespace panel_focus
{
const std::string panel_focus_transformer_name = "panel_focus_transformer";
class panel_focus_view : public wf::scene::view_2d_transformer_t
{
  public:

    panel_focus_view(wayfire_view view) : wf::scene::view_2d_transformer_t(view)
    {}

    wf::keyboard_focus_node_t keyboard_refocus(wf::output_t *output) override
    {
        return {};
    }

    virtual ~panel_focus_view()
    {}
};

class wayfire_panel_focus : public wf::plugin_interface_t
{
    wf::ipc_activator_t cycle{"panel-focus/cycle"};
    wf::ipc_activator_t deactivate{"panel-focus/deactivate"};
    wf::view_matcher_t panel_focus_match{"panel-focus/panel_focus_match"};
    wayfire_view current_focus_view, toplevel_focus_view;
    bool panel_focus_active = false;

  public:

    void init() override
    {
        cycle.set_handler(cycle_panels);
        deactivate.set_handler(deactivate_focus);
        current_focus_view = wf::get_core().seat->get_active_view();
        toplevel_focus_view = nullptr;
        wf::get_core().connect(&on_view_mapped);
        wf::get_core().connect(&on_view_unmapped);

        for (auto& view : wf::get_core().get_all_views())
        {
            if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT &&
                wf::get_view_layer(view) > wf::scene::layer::WORKSPACE &&
                panel_focus_match.matches(view))
            {
                ensure_transformer(view);
            }
        }
    }

    void pop_transformer(wayfire_view view)
    {
        if (view->get_transformed_node()->get_transformer(panel_focus_transformer_name))
        {
            view->get_transformed_node()->rem_transformer(panel_focus_transformer_name);
        }
    }

    void remove_transformers()
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            pop_transformer(view);
        }
    }

    std::shared_ptr<wf::panel_focus::panel_focus_view> ensure_transformer(wayfire_view view)
    {
        auto tmgr = view->get_transformed_node();
        if (auto tr = tmgr->get_transformer<wf::panel_focus::panel_focus_view>(panel_focus_transformer_name))
        {
            return tr;
        }

        auto node = std::make_shared<wf::panel_focus::panel_focus_view>(view);
        tmgr->add_transformer(node, wf::TRANSFORMER_2D, panel_focus_transformer_name);
        auto tr = tmgr->get_transformer<wf::panel_focus::panel_focus_view>(panel_focus_transformer_name);

        return tr;
    }

    wf::ipc_activator_t::handler_t deactivate_focus = [=] (wf::output_t *output, wayfire_view)
    {
        if (toplevel_focus_view)
        {
            wf::get_core().seat->focus_view(toplevel_focus_view);
            current_focus_view = toplevel_focus_view;
            toplevel_focus_view = nullptr;
            panel_focus_active = false;
            return true;
        }
        return false;
    };

    wf::signal::connection_t<wf::view_mapped_signal> on_view_mapped = [=] (wf::view_mapped_signal *ev)
    {
        if (ev && ev->view->role == wf::VIEW_ROLE_TOPLEVEL &&
            wf::get_view_layer(ev->view) == wf::scene::layer::WORKSPACE &&
            panel_focus_active)
        {
            toplevel_focus_view = ev->view;
        }
        if (ev->view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT &&
            wf::get_view_layer(ev->view) > wf::scene::layer::WORKSPACE)
        {
            ensure_transformer(ev->view);
            wf::get_core().seat->refocus();
        }
    };

    wf::signal::connection_t<wf::view_unmapped_signal> on_view_unmapped = [=] (wf::view_unmapped_signal *ev)
    {
        if (ev->view == wf::get_core().seat->get_active_view())
        {
            toplevel_focus_view = nullptr;
        }
        pop_transformer(ev->view);
    };

    wf::ipc_activator_t::handler_t cycle_panels = [=] (wf::output_t *output, wayfire_view)
    {
        wayfire_view last_focus_view = current_focus_view;
        bool focus_view_found = false;
        if (!toplevel_focus_view)
        {
            auto view = wf::get_core().seat->get_active_view();
            if (view && view->role == wf::VIEW_ROLE_TOPLEVEL &&
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
                focus_view_found && panel_focus_match.matches(view))
            {
                pop_transformer(view);
                wf::get_core().seat->focus_view(view);
                current_focus_view = view;
                ensure_transformer(view);
                panel_focus_active = true;
                break;
            }
        }
        if (last_focus_view == current_focus_view)
        {
            for (auto& view : wf::get_core().get_all_views())
            {
                if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT &&
                    wf::get_view_layer(view) > wf::scene::layer::WORKSPACE &&
                    panel_focus_match.matches(view))
                {
                    pop_transformer(view);
                    wf::get_core().seat->focus_view(view);
                    current_focus_view = view;
                    ensure_transformer(view);
                    panel_focus_active = true;
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
        remove_transformers();
        on_view_mapped.disconnect();
        on_view_unmapped.disconnect();
    }
};
}
}

DECLARE_WAYFIRE_PLUGIN(wf::panel_focus::wayfire_panel_focus);
