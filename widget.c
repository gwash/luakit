/*
 * widget.c - widget managing
 *
 * Copyright (C) 2010 Mason Larobina <mason.larobina@gmail.com>
 * Copyright (C) 2007-2009 Julien Danjou <julien@danjou.info>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "luakit.h"
#include "common/util.h"
#include "common/luaclass.h"
#include "common/luaobject.h"

#include "widget.h"
#include "luah.h"

LUA_OBJECT_FUNCS(widget_class, widget_t, widget);

/** Collect a widget structure.
 * \param L The Lua VM state.
 * \return 0
 */
static gint
luaH_widget_gc(lua_State *L)
{
    widget_t *widget = luaH_checkudata(L, 1, &widget_class);
    if(widget->destructor)
        widget->destructor(widget);
    return luaH_object_gc(L);
}

/** Create a new widget.
 * \param L The Lua VM state.
 *
 * \luastack
 * \lparam A table with at least a type value.
 * \lreturn A brand new widget.
 */
static gint
luaH_widget_new(lua_State *L)
{
    luaH_class_new(L, &widget_class);
    widget_t *w = luaH_checkudata(L, -1, &widget_class);

    w->parent = NULL;
    w->window = NULL;

    /* save ref to the lua class instance */
    lua_pushvalue(L, -1);
    w->ref = luaH_object_ref_class(L, -1, &widget_class);

    return 1;
}

/** Generic widget.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 * \luastack
 * \lfield visible The widget visibility.
 * \lfield mouse_enter A function to execute when the mouse enter the widget.
 * \lfield mouse_leave A function to execute when the mouse leave the widget.
 */
static gint
luaH_widget_index(lua_State *L)
{
    size_t len;
    const char *prop = luaL_checklstring(L, 2, &len);
    luakit_token_t token = l_tokenize(prop, len);

    /* Try standard method */
    if(luaH_class_index(L))
        return 1;

    /* Then call special widget index */
    widget_t *widget = luaH_checkudata(L, 1, &widget_class);
    return widget->index ? widget->index(L, token) : 0;
}

/** Generic widget newindex.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 */
static gint
luaH_widget_newindex(lua_State *L)
{
    size_t len;
    const char *prop = luaL_checklstring(L, 2, &len);
    luakit_token_t token = l_tokenize(prop, len);

    /* Try standard method */
    luaH_class_newindex(L);

    /* Then call special widget newindex */
    widget_t *widget = luaH_checkudata(L, 1, &widget_class);
    return widget->newindex ? widget->newindex(L, token) : 0;
}

static gint
luaH_widget_set_type(lua_State *L, widget_t *w)
{
    if (w->type)
        luaL_error(L, "widget is already of type \"%s\"", w->type);

    size_t len;
    const gchar *type = luaL_checklstring(L, -1, &len);
    widget_constructor_t *wc = NULL;
    luakit_token_t tok = l_tokenize(type, len);

    switch(tok)
    {
      case L_TK_WEBVIEW:
        wc = widget_webview;
        break;

      case L_TK_NOTEBOOK:
        wc = widget_notebook;
        break;

      case L_TK_TEXTAREA:
        wc = widget_textarea;
        break;

      case L_TK_HBOX:
        wc = widget_hbox;
        break;

      case L_TK_VBOX:
        wc = widget_vbox;
        break;

      default:
        break;
    }

    if(!wc)
        luaL_error(L, "unknown widget type: %s", type);

    wc(w);
    // TODO: This is very wasteful
    w->type = g_strdup(type);

    luaH_object_emit_signal(L, -3, "init", 0);
    return 0;
}

static gint
luaH_widget_get_type(lua_State *L, widget_t *w)
{
    if (!w->type)
        return 0;

    lua_pushstring(L, w->type);
    return 1;
}

static gint
luaH_widget_get_parent(lua_State *L, widget_t *w)
{
    if (w->parent) {
        luaH_object_push(L, w->parent->ref);
        return 1;
    }

    if (w->window) {
        luaH_object_push(L, w->window->ref);
        return 1;
    }

    return 0;
}

void
widget_class_setup(lua_State *L)
{
    static const struct luaL_reg widget_methods[] =
    {
        LUA_CLASS_METHODS(widget)
        { "__call", luaH_widget_new },
        { NULL, NULL }
    };

    static const struct luaL_reg widget_meta[] =
    {
        LUA_OBJECT_META(widget)
        { "__index", luaH_widget_index },
        { "__newindex", luaH_widget_newindex },
        { "__gc", luaH_widget_gc },
        { NULL, NULL }
    };

    luaH_class_setup(L, &widget_class, "widget", (lua_class_allocator_t) widget_new,
                     NULL, NULL,
                     widget_methods, widget_meta);

    luaH_class_add_property(&widget_class, L_TK_PARENT,
                            NULL,
                            (lua_class_propfunc_t) luaH_widget_get_parent,
                            NULL);

    luaH_class_add_property(&widget_class, L_TK_TYPE,
                            (lua_class_propfunc_t) luaH_widget_set_type,
                            (lua_class_propfunc_t) luaH_widget_get_type,
                            NULL);
}

// vim: ft=c:et:sw=4:ts=8:sts=4:enc=utf-8:tw=80