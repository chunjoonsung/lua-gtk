-- vim:sw=4:sts=4

name = "Pango"
pkg_config_name = "pango"

includes = {}
includes.all = {
    "<pango/pango.h>"
}

libraries = {
    linux = { "/usr/lib/libpango-1.0.so.0" },
    win32 = { "libpango-1.0-0.dll" },
}

include_dirs = { "pango-1.0" }

function_flags = {
    pango_color_to_string = CHAR_PTR,
    pango_font_description_get_family = CONST_CHAR_PTR,
    pango_font_description_to_filename = CHAR_PTR,
    pango_font_description_to_string = CHAR_PTR,
    pango_font_face_get_face_name = CONST_CHAR_PTR,
    pango_font_family_get_name = CONST_CHAR_PTR,
    pango_language_get_sample_string = CONST_CHAR_PTR,
    pango_layout_get_text = CONST_CHAR_PTR,
    pango_trim_string = CHAR_PTR,
    pango_version_check = CONST_CHAR_PTR,
    pango_version_string = CONST_CHAR_PTR,

    pango_attr_iterator_get_attrs = 0x10,
    pango_glyph_item_apply_attrs = 0x20,
}

linklist = {
    "pango_attr_list_ref",
    "pango_attr_list_unref",
    "pango_tab_array_get_tabs",
    "pango_tab_array_get_size",
    "g_free",
}

-- extra settings for the module_info structure
module_info = {
    prefix_func = '"pango_"',
    prefix_constant = '"PANGO_"',
    prefix_type = '"Pango"',
    depends = '"glib\\0"',
    overrides = 'pango_overrides',
    arg_flags_handler = 'pango_arg_flags',
}

