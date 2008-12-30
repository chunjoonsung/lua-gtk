#! /usr/bin/env lua
-- vim:sw=4:sts=4

require "gtk"
require "pango"

gnome.set_debug_flags "memory"

win_count = 0

function mywin_new(msg)
    local self = { pixmap=nil, msg=msg }
    self.win = gtk.window_new(gtk.WINDOW_TOPLEVEL)
    self.win:connect('destroy', on_destroy, self)
    self.win:set_title('Pixmap Test')
    local da = gtk.drawing_area_new()
    self.win:add(da)
    da:connect('configure-event', on_configure, self)
    da:connect('expose-event', on_expose, self)
    self.win:show_all()
    win_count = win_count + 1
    return self
end

function on_destroy(w)
    win_count = win_count - 1
    if win_count == 0 then
	gtk.main_quit()
    end
end

--
-- On configure (size change), allocate a pixmap, fill with white and draw
-- something in it.
--
function on_configure(da, ev, ifo)
    local window = ifo.win.window
    -- print("WINDOW", window)
    -- gnome.breakfunc()
    local width, height = window:get_size(0, 0)
    -- print("WIDTH, HEIGHT", width, height)
    assert(width)
    assert(height)

    if ifo.pixmap then
	-- mostly seems to work.  maybe check some more
	gnome.destroy(ifo.pixmap)
    end

    -- allocates memory in X server... default drawable, width, height, depth
    -- loses the reference to the previous pixmap, if any.
    ifo.pixmap = gdk.pixmap_new(window, width, height, -1)

    local style = ifo.win:get_style()
    local white_gc = style.white_gc
    local black_gc = style.black_gc

    -- clear the whole pixmap
    ifo.pixmap:draw_rectangle(white_gc, true, 0, 0, width, height)

    -- draw a rectangle
    if width > 20 and height > 20 then
	ifo.pixmap:draw_rectangle(black_gc, false, 10, 10, width - 20,
	    height - 20)
    end

    -- draw a text message
    local message = "Hello, World! " .. ifo.msg
    local layout = ifo.win:create_pango_layout(message)
    ifo.pixmap:draw_layout(black_gc, 15, 15, layout)

    -- get size of message
    local region = layout:get_clip_region(0, 0, {0, string.len(message)}, 1)
    local rect = gdk.new "Rectangle"
    region:get_clipbox(rect)
    if rect.width > 0 then
	ifo.pixmap:draw_layout(black_gc, width - 15 - rect.width,
	    height - 15 - rect.height, layout)
    end

    -- Make sure that the unreferenced pixmap is freed NOW and not eventually,
    -- because this can eat up loads of memory of the X server.
    -- collectgarbage()
    -- collectgarbage()

    return true
end

-- on expose, copy the newly visible part of the pixmap to the GdkWindow
-- of the drawing area.
--
-- Note: this creates a pixmap with w*h pixels, copies the relevant part of
-- ifo.pixmap into it, then copies that pixmap to the window, and destroys
-- the pixmap.
--
function on_expose(da, ev, ifo)
    local area = ev.expose.area
    local x, y, w, h = area.x, area.y, area.width, area.height
    local window = da.window
    local style = ifo.win:get_style()
    local white_gc = style.white_gc
    gdk.draw_drawable(window, white_gc, ifo.pixmap, x, y, x, y, w, h)
    return false
end

-- gnome.set_debug_flags("memory")
mywin1 = mywin_new("One")
mywin2 = mywin_new("Two")
gtk.main()

mywin1 = nil
mywin2 = nil

if false then
    collectgarbage("collect")
    collectgarbage("collect")
    collectgarbage("collect")
    gnome.dump_memory()
end

glib.mem_profile()

