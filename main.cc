#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

#include <X11/Xresource.h>

#include "config.hh"
#include "screen.hh"
#include "frame.hh"
#include "atoms.hh"
#include "info.hh"
#include "menu.hh"
#include "winmenu.hh"

Display * dpy = nullptr;
root_window * screen = nullptr;
bool restart = false;
bool running = false;
uint pressed_button = 0;
bool button_moved = false;

static bool got_alarm = false;

static int error_handler(Display *, XErrorEvent *)
{
	return 0;
}

static void on_alarm(int)
{
	got_alarm = true;
}

static void set_signals()
{
	struct sigaction sa = {};
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, nullptr);
	sa.sa_handler = on_alarm;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, nullptr);
}

int main(int argc, char ** argv)
{
	const char * display = nullptr;
	const char * confdir = nullptr;

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp("-display", argv[i]) && i + 1 < argc)
		{
			display = argv[++i];
			setenv("DISPLAY", display, 1);
		}
		else if (!strcmp("-config", argv[i]) && i + 1 < argc)
		{
			confdir = argv[++i];
		}
		else
		{
			fprintf(stderr, "Usage: %s [-display dpy] [-config dir]\n", argv[0]);
			return 1;
		}
	}

	dpy = XOpenDisplay(display);
	if (!dpy)
	{
		if (!display)
		{
			display = getenv("DISPLAY");
			if (!display) display = "";
		}

		fprintf(stderr, "%s: Can't open display: %s\n", argv[0], display);
		return 1;
	}

	set_signals();

	int shape_event = 0;
	int shape_error = 0;
	XShapeQueryExtension(dpy, &shape_event, &shape_error);

	atoms::init();

	screen = root_window::create(DefaultScreen(dpy));

	XSync(dpy, False);
	XSetErrorHandler(error_handler);

	config::init(confdir);
	config::read("config");
	config::fini();

	info_window::init();
	winmenu_action::init();
	menu_window::init();

	screen->init();

	pollfd fd = {};
	fd.fd = ConnectionNumber(dpy);
	fd.events = POLLIN;

	running = true;
	while (running)
	{
		while (!XPending(dpy))
		{
			if (got_alarm)
			{
				got_alarm = false;
				info_window::clear_workspace();
				XFlush(dpy);
			}

			poll(&fd, 1, -1);
		}

		XEvent event;
		window * win;

		XNextEvent(dpy, &event);

		Window id = event.xany.window;

		if (!window::find(id, &win))
		{
			if (event.type == ClientMessage)
				frame_window::xclient_message(&event.xclient);
			continue;
		}

		switch (event.type)
		{
		case KeyPress:
			win->key_press(&event.xkey);
			break;
		case ButtonPress:
			if (!pressed_button)
			{
				pressed_button = event.xbutton.button;
				button_moved = false;
			}
			win->button_press(&event.xbutton);
			break;
		case ButtonRelease:
			if (event.xbutton.button == pressed_button)
				pressed_button = 0;
			info_window::clear_frame();
			win->button_release(&event.xbutton);
			break;
		case MotionNotify:
			while (XCheckTypedWindowEvent(dpy, id, MotionNotify, &event)) {}
			win->motion_notify(&event.xmotion);
			break;
		case EnterNotify:
			win->enter_notify(&event.xcrossing);
			break;
		case LeaveNotify:
			win->leave_notify(&event.xcrossing);
			break;
		case FocusIn:
			win->focus_in(&event.xfocus);
			break;
		case FocusOut:
			win->focus_out(&event.xfocus);
			break;
		case Expose:
			win->expose(&event.xexpose);
			break;
		case DestroyNotify:
			win->destroy_notify(&event.xdestroywindow);
			break;
		case UnmapNotify:
			win->unmap_notify(&event.xunmap);
			break;
		case MapNotify:
			win->map_notify(&event.xmap);
			break;
		case MapRequest:
			win->map_request(&event.xmaprequest);
			break;
		case ConfigureNotify:
			win->configure_notify(&event.xconfigure);
			break;
		case ConfigureRequest:
			win->configure_request(&event.xconfigurerequest);
			break;
		case PropertyNotify:
			win->property_notify(&event.xproperty);
			break;
		case ClientMessage:
			win->client_message(&event.xclient);
			break;
		default:
			if (event.type != shape_event)
				break;
			win->shape_event(reinterpret_cast<XShapeEvent *>(&event));
		}
	}

	screen->destroy();
	XCloseDisplay(dpy);

	if (restart) execvp(argv[0], argv);

	return 0;
}
