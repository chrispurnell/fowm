#include "screen.hh"
#include "frame.hh"
#include "decorfunc.hh"

title_window * title_window::create(frame_window * fwin)
{
	uchar style = fwin->style();
	cfg_titlebar * cfg = &config::style[style].titlebar;
	uint ci = decor_window::color_index(cfg->num_col, fwin->focused());
	rectangle rect;
	decor_window::make_rectangle(&rect, &cfg->position, fwin->width(), fwin->height());

	Window win = decor_window::create_window(fwin->id(),
		ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | 
		EnterWindowMask | LeaveWindowMask | ExposureMask,
		&rect, cfg->bg_color[ci], cfg->pixmap[ci]);
	title_window * twin = mnew(title_window, win, fwin, cfg->act_idx, cfg->num_col);

	twin->w_width = rect.width;
	twin->w_height = rect.height;

#ifdef USE_XFT
	twin->w_draw = XftDrawCreate(dpy, win, screen->visual(), screen->colormap());
#else
	XGCValues values;
	values.foreground = cfg->fg_color[ci];
	values.font = config::font->fid;
	twin->w_gc = XCreateGC(dpy, win, GCForeground | GCFont, &values);
#endif
	twin->calc_title();

	return twin;
}

void title_window::destroy()
{
#ifdef USE_XFT
	XftDrawDestroy(w_draw);
#else
	XFreeGC(dpy, w_gc);
#endif
	XDestroyWindow(dpy, id());
	this->~title_window();
	free(this);
}

void title_window::set_background()
{
	cfg_titlebar * cfg = &config::style[frame()->style()].titlebar;
	uint ci = color_index();
	decor_window::set_background(id(), cfg->bg_color[ci], cfg->pixmap[ci]);
#ifndef USE_XFT
	XGCValues values;
	values.foreground = cfg->fg_color[ci];
	XChangeGC(dpy, w_gc, GCForeground, &values);
#endif
	expose(nullptr);
}

void title_window::set_size()
{
	cfg_titlebar * cfg = &config::style[frame()->style()].titlebar;
	rectangle rect;
	decor_window::make_rectangle(&rect, &cfg->position, frame()->width(), frame()->height());
	decor_window::set_size(id(), &rect);
	w_width = rect.width;
	w_height = rect.height;
	calc_title();
	expose(nullptr);
}

void title_window::calc_title()
{
	uint len = frame()->name_len();
	if (!len) return;
	const char * str = frame()->name();

	cfg_rect * cfg = &config::style[frame()->style()].titlebar.title;
	rectangle rect;
	decor_window::make_rectangle(&rect, cfg, w_width, w_height);

#ifdef USE_XFT
	XGlyphInfo info;
	XftTextExtentsUtf8(dpy, config::font, str_cast(str), len, &info);
	int width = info.xOff;
#else
	int width = XTextWidth(config::font, str, len);
#endif
	int ascent = config::font->ascent;
	int descent = config::font->descent;

	t_x = rect.x + (rect.width - width) / 2;
	if (t_x < rect.x) t_x = rect.x;
	t_y = rect.y + (rect.height + ascent - descent) / 2;
}

#ifdef USE_XFT
void title_window::expose(XExposeEvent * ev)
{
	uint len = frame()->name_len();
	if (!len) return;

	if (ev)
	{
		XRectangle rect = {};
		rect.x = ev->x;
		rect.y = ev->y;
		rect.width = ev->width;
		rect.height = ev->height;
		XftDrawSetClipRectangles(w_draw, 0, 0, &rect, 1);
	}
	else
	{
		XftDrawSetClip(w_draw, nullptr);
	}

	const char * str = frame()->name();
	cfg_titlebar * cfg = &config::style[frame()->style()].titlebar;
	uint ci = color_index();
	XftDrawStringUtf8(w_draw, cfg->fg_color + ci, config::font, t_x, t_y, str_cast(str), len);
}
#else
void title_window::expose(XExposeEvent * ev)
{
	if (ev && ev->count) return;
	uint len = frame()->name_len();
	if (!len) return;
	const char * str = frame()->name();
	XDrawString(dpy, id(), w_gc, t_x, t_y, str, len);
}
#endif
