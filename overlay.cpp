// compile with
// g++ -lXfixes -lXcomposite -lX11 `pkg-config --cflags --libs cairo` overlay.c

#include <assert.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <chrono>
#include <thread>

void draw(cairo_t *cr, int w, int h) {
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.5);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_line_width(cr, 10.0);
    cairo_stroke(cr);
}

void set_monitor(Display *d, Window root, int w, int h, int x, int y) {
    XRRScreenResources *screen = XRRGetScreenResources(d, root);
    XRROutputInfo *output_info = XRRGetOutputInfo(d, screen, screen->outputs[0]);

    XRRMonitorInfo *monitors;
    int nmonitors;
    monitors = XRRGetMonitors(d, root, true, &nmonitors);

    // Create virtual monitor (equivalent to xrandr --setmonitor)
    XRRMonitorInfo monitor;
    monitor.name = XInternAtom(d, "screenshare", False);
    monitor.x = x;
    monitor.y = y;
    monitor.width = w;
    monitor.height = h;
    monitor.mwidth = w; // Aspect ratio 1/1
    monitor.mheight = h; // Aspect ratio 1/1
    monitor.noutput = 1; // Number of outputs used by this monitor
    RROutput primary_output = XRRGetOutputPrimary(d, root);
    monitor.outputs = &primary_output;

    XRRSetMonitor(d, root, &monitor);

    XRRFreeMonitors(monitors);
    XRRFreeOutputInfo(output_info);
    XRRFreeScreenResources(screen);
}

void removeMonitor(Display* display, Window root) {
    Atom monitorAtom = XInternAtom(display, "screenshare", False);

    if (!monitorAtom) {
        return;
    }

    // Remove the virtual monitor
    XRRDeleteMonitor(display, root, monitorAtom);
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <width> <height> <x> <y>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int w = atoi(argv[1]);
    int h = atoi(argv[2]);
    int x = atoi(argv[3]);
    int y = atoi(argv[4]);
    Display *d = XOpenDisplay(NULL);
    Window root = DefaultRootWindow(d);
    int default_screen = XDefaultScreen(d);
    set_monitor(d, root, w, h, x, y);

    // these two lines are really all you need
    XSetWindowAttributes attrs;
    attrs.override_redirect = true;

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(d, DefaultScreen(d), 32, TrueColor, &vinfo)) {
        printf("No visual found supporting 32 bit color, terminating\n");
        exit(EXIT_FAILURE);
    }
    // these next three lines add 32 bit depth, remove if you dont need and change the flags below
    attrs.colormap = XCreateColormap(d, root, vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;

    // Window XCreateWindow(
    //     Display *display, Window parent,
    //     int x, int y, unsigned int width, unsigned int height, unsigned int border_width,
    //     int depth, unsigned int class,
    //     Visual *visual,
    //     unsigned long valuemask, XSetWindowAttributes *attributes
    // );
    Window overlay = XCreateWindow(
        d, root,
        x, y, w, h, 0,
        vinfo.depth, InputOutput,
        vinfo.visual,
        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &attrs
    );

    XMapWindow(d, overlay);

    cairo_surface_t* surf = cairo_xlib_surface_create(d, overlay,
                                  vinfo.visual,
                                  w, h);
    cairo_t* cr = cairo_create(surf);

    draw(cr, w, h);
    XFlush(d);

    // wait for sig int here
    printf("waiting for sigint to stdout\n");
    pause();
    printf("shutting down\n");

    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    removeMonitor(d, root);

    XUnmapWindow(d, overlay);
    XCloseDisplay(d);
    return 0;
}
