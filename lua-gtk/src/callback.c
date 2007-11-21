/* vim:sw=4:sts=4
 * Lua binding for the Gtk 2 toolkit.
 * This code handles callbacks from Gtk to Lua.
 * Copyright (C) 2005, 2007 Wolfgang Oertl
 *
 * Exported functions:
 *   luagtk_connect
 *   luagtk_disconnect
 */

/**
 * @class module
 * @name gtk_internal.callback
 * @description Handle callbacks from Gtk to Lua.
 */

#include "luagtk.h"
#include <lauxlib.h>	    // luaL_check*, luaL_ref/unref
#include <stdarg.h>

/* one such structure per connected callback */
struct callback_info {
    int handler_ref;		/* reference to the function to call */
    int args_ref;		/* reference to a table with additional args */
    int widget_ref;		/* reference to the widget: avoids GC */
    lua_State *L;		/* the Lua state this belongs to */
    GSignalQuery query;		/* information about the signal, see below */
};
/* query: signal_id, signal_name, itype, signal_flags, return_type, n_params,
 * param_types */

/**
 * Handle return values from the Lua handler to pass back to Gtk.  Not many
 * different types are supported - but I think no others are actually used.
 *
 * @param L  lua_State
 * @param return_type  The GType of the expected return value
 * @param cbi  callback_info of this signal
 * @return  An integer to return to Gtk.
 */
static int _callback_return_value(lua_State *L, int return_type,
    struct callback_info *cbi)
{
    int val = 0;

    switch (return_type) {
	case G_TYPE_NONE:
	    break;
	case G_TYPE_BOOLEAN:
	    val = lua_toboolean(L, -1);
	    break;
	case G_TYPE_INT:
	    val = lua_tointeger(L, -1);
	    break;
	default:
	    printf("%s unhandled callback return type %ld of callback %s\n",
		msgprefix, (long int) return_type, cbi->query.signal_name);
    }

    return val;
}

/**
 * Destroy all Lua proxy objects for Gtk objects on the stack.
 *
 * This is required before returning from a callback.  Objects on the
 * stack will most likely not be there anymore later.
 */
static void sol_free(lua_State *L, struct stack_obj_list *sol)
{
    int i, ref_nr;
    struct widget *w, *w_start;

    if (sol->n_used == 0)
	return;

    lua_getfield(L, LUA_ENVIRONINDEX, LUAGTK_WIDGETS);  // widgets
    lua_getfield(L, LUA_ENVIRONINDEX, LUAGTK_ALIASES);  // widgets aliases

    for (i=0; i<sol->n_used; i++) {
	ref_nr = sol->ref_list[i];
	lua_rawgeti(L, -1, ref_nr);			// widgets aliases w
	w = (struct widget*) lua_topointer(L, -1);
	w_start = w;

	// remove the entry in gtk.widgets
	lua_pushlightuserdata(L, w->p);
	lua_pushnil(L);					// wi al w *p nil
	lua_rawset(L, -5);				// wi al w

	// remove all aliases
	do {
	    printf("Destroying widget with ref %d, %p %p %s\n", w->own_ref,
		w, w->p, w->class_name);
	    luaL_unref(L, -2, w->own_ref);
	    w->p = NULL;
	    w = w->next;
	} while (w && w != w_start);

	lua_pop(L, 1);
    }

    lua_pop(L, 2);
}


/**
 * Handler for Gtk signal callbacks.  Find the proper Lua callback, build the
 * parameters, call, and optionally return something to Gtk.  This runs in the
 * lua_State that was used to call luagtk_connect with, and therefore probably
 * uses the stack of the main function, which mustn't be modified.
 *
 * @param data   a pointer to a struct callback_info
 * @param ...    Variable arguments, and finally the widget pointer.
 * @return       A value to return to Gtk.
 */
static int _callback(void *data, ...)
{
    va_list ap;
    int i, arg_cnt, return_count, extra_args=0;
    struct callback_info *cbi = (struct callback_info*) data;
    lua_State *L = cbi->L;
    int stack_top = lua_gettop(L);
    struct stack_obj_list stack_obj_list = { 0, };

    /* get the handler function */
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbi->handler_ref);

    /* push all the signal arguments to the Lua stack */
    arg_cnt = cbi->query.n_params;

    // retrieve the additional parameters using the stdarg mechanism.
    // XXX it might be nesessary to differentiate between 4 and 8 byte
    // arguments, which could be derived from the type...
    va_start(ap, data);
    for (i=0; i<arg_cnt; i++) {
	GType type = cbi->query.param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE;
	long int val = va_arg(ap, long int);
	(void) luagtk_push_value(L, type, (char*) &val, &stack_obj_list);
    }

    /* The widget is the last parameter to this function.  The Lua callback
     * gets it as the first parameter, though. */
    void *widget = va_arg(ap, void*);
    get_widget(L, widget, 0, 0);
    // sol_add(L, &stack_obj_list);
    va_end(ap);

    if (lua_isnil(L, -1))
	fprintf(stderr, "Warning: _callback couldn't find widget %p\n", widget);
    lua_insert(L, stack_top + 2);

    /* copy all the extra arguments (user provided) to the stack. */
    if (cbi->args_ref) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, cbi->args_ref);
	lua_pushnil(L);
	// stack: ..., argstable, key
	while (lua_next(L, -2) != 0) {
	    lua_insert(L, -3);
	    extra_args ++;
	}
	lua_pop(L, 1);
    }

    /* determine whether a return value is expected */
    GType return_type = cbi->query.return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
    return_count = (return_type == G_TYPE_NONE) ? 0 : 1;

    /* Call the callback! */
    lua_call(L, arg_cnt+extra_args+1, return_count);

    /* Determine the return value (default is zero) */
    int val = _callback_return_value(L, return_type, cbi);

    /* remove stack objects with all aliases that might have been created */
    sol_free(L, &stack_obj_list);

    /* make sure the stack is back to the original state */
    lua_settop(L, stack_top);

    return val;
}


/**
 * Free memory on signal handler disconnection.
 *
 * The struct callback_info contains references to entries in the registry
 * of the Lua state.  They must be unreferenced, then the structure itself
 * is freed.
 *
 * XXX the Lua state might not exist anymore?
 */
static void _free_callback_info(gpointer data, GClosure *closure)
{
    // data is a struct callback_info.  It contains references, free them.
    // see luagtk_connect().
    struct callback_info *cb_info = (struct callback_info*) data;

    // remove the reference to the callback function (closure) & the widget
    luaL_unref(cb_info->L, LUA_REGISTRYINDEX, cb_info->handler_ref);
    luaL_unref(cb_info->L, LUA_REGISTRYINDEX, cb_info->widget_ref);

    // remove the reference to the table with the extra arguments
    if (cb_info->args_ref)
	luaL_unref(cb_info->L, LUA_REGISTRYINDEX, cb_info->args_ref);

    // Is this required? I guess so.  See
    // glib/gobject/gclosure.c:g_closure_unref() - closure->data is not
    // freed there.
    g_slice_free(struct callback_info, cb_info);
}


/**
 * @class module
 * @name gtk
 */

/**
 * Connect a signal to a Lua function.
 *
 * @name connect
 * @luaparam widget
 * @luaparam signal_name  Name of the signal, like "clicked"
 * @luaparam handler  A Lua function (the callback)
 * @luaparam ...  (optional) extra parameters to the callback
 *
 * @return  The handler id, which can be used to disconnect the signal.
 */
int luagtk_connect(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TUSERDATA);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    int stack_top, i;
    gulong handler_id;
    struct callback_info *cb_info;
    guint signal_id;

    // get the widget
    struct widget *w = (struct widget*) lua_topointer(L, 1);
    if (!w || !w->p)
	luaL_error(L, "trying to connect to a NULL widget\n");

    // determine the signal
    const char *signame = lua_tostring(L, 2);
    signal_id = g_signal_lookup(signame, G_OBJECT_TYPE(w->p));
    if (!signal_id)
	luaL_error(L, "Can't find signal %s::%s\n", w->class_name, signame);

    cb_info = g_slice_new(struct callback_info);
    cb_info->L = L;
    g_signal_query(signal_id, &cb_info->query);

    if (cb_info->query.signal_id != signal_id) {
	g_slice_free(struct callback_info, cb_info);
	luaL_error(L, "invalid signal ID %d for signal %s::%s\n",
	    signal_id, w->class_name, signame);
    }

    /* stack: widget - signame - func - .... */

    /* The callback is either a function, or a table with the function and
     * additional paramters.
     */

    // make a reference to the function, and store it.
    stack_top = lua_gettop(L);
    lua_pushvalue(L, 3);
    cb_info->handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // make a reference to the widget - to avoid it being garbage collected.
    lua_pushvalue(L, 1);
    cb_info->widget_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // if there are more arguments, put them into a table, and store a
    // reference to it.  When called just with NIL as "more arguments", ignore
    // that.
    if (stack_top > 3 && (stack_top != 4 || lua_type(L, 4) != LUA_TNIL)) {
	lua_newtable(L);
	for (i=4; i<=stack_top; i++) {
	    lua_pushvalue(L, i);
	    lua_rawseti(L, -2, i - 3);	// [1] etc. are the parameters
	}
	cb_info->args_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else
	cb_info->args_ref = 0;

    handler_id = g_signal_connect_data(w->p, signame,
	(GCallback) _callback, cb_info, _free_callback_info,
	G_CONNECT_SWAPPED);

    lua_pushnumber(L, handler_id);

    return 1;
}


/**
 * Disconnect a signal handler from a given widget.
 *
 * @name disconnect
 * @luaparam widget  The widget to disconnect a handler for
 * @luaparam handler_id  The handler ID of the connection, as returned from
 *   the connect function.
 */
int luagtk_disconnect(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TUSERDATA);
    luaL_checktype(L, 2, LUA_TNUMBER);

    struct widget *w = (struct widget*) lua_topointer(L, 1);
    gulong handler_id = lua_tointeger(L, 2);
    g_signal_handler_disconnect(w->p, handler_id);

    return 0;
}

