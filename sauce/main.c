#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <vte/vte.h>
#include <pango/pango.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#define MOD(k) (event->keyval == (k) && modifiers == (GDK_CONTROL_MASK|GDK_SHIFT_MASK))
static char *cmd[16] = {SHELL, NULL};
static char *cwd = NULL;
static char *embed = NULL;

static struct {
    GtkWindow *window;
    GtkWidget *term;
    GdkRGBA palette[16];
    GdkRGBA background;
    double font_scale;
    PangoFontDescription *font;
} fuck;

// fuck callbacks
static void fuck_quit(VteTerminal *term, int status);
static void fuck_update_title(VteTerminal *term);
static gboolean fuck_key_press(GtkWidget *widget, GdkEventKey *event);

// fuck functions
static void fuck_set_font_scale(double scale);
static void fuck_set_alpha_scale(double scale);
static void fuck_init(VteTerminal *term);


static void
fuck_quit(VteTerminal *term, int status)
{
    gtk_window_close(fuck.window);
}

static void
fuck_update_title(VteTerminal *term)
{
    gtk_window_set_title(fuck.window, vte_terminal_get_window_title(term));
}

static gboolean
fuck_key_press(GtkWidget *widget, GdkEventKey *event)
{
    GdkModifierType modifiers = event->state & gtk_accelerator_get_default_mod_mask();
    if (MOD(GDK_KEY_C)) {
        vte_terminal_copy_clipboard_format(VTE_TERMINAL(fuck.term), VTE_FORMAT_TEXT);
    }
    else if (MOD(GDK_KEY_V)) {
        vte_terminal_paste_clipboard(VTE_TERMINAL(fuck.term));
    }
    else if (MOD(GDK_KEY_plus)) {
        fuck_set_font_scale(fuck.font_scale + 0.1);
    }
    else if (MOD(GDK_KEY_underscore)) {
        fuck_set_font_scale(fuck.font_scale - 0.1);
    }
    else if (MOD(GDK_KEY_BackSpace)) {
        fuck_set_font_scale(1);
    }
    else if (MOD(GDK_KEY_less)) {
        fuck_set_alpha_scale(fuck.background.alpha > 0? fuck.background.alpha - 0.05 : 0);
    }
    else if (MOD(GDK_KEY_greater)) {
        fuck_set_alpha_scale(fuck.background.alpha < 1? fuck.background.alpha + 0.05 : 1);
    }
    else if (MOD(GDK_KEY_colon)) {
        fuck_set_alpha_scale(ALPHA);
    }
    else {
        return FALSE;
    }
    return TRUE;
}

static void
fuck_set_font_scale(double scale)
{
    fuck.font_scale = scale;
    vte_terminal_set_font_scale(VTE_TERMINAL(fuck.term), fuck.font_scale);
}

static void
fuck_set_alpha_scale(double scale)
{
    fuck.background.alpha = scale;
    gtk_widget_override_background_color(GTK_WIDGET(fuck.window),
                GTK_STATE_FLAG_NORMAL, &fuck.background);

    // this is deprecated, but it works so fuck it
    vte_terminal_set_colors(VTE_TERMINAL(fuck.term),
                &fuck.palette[15], &fuck.background, fuck.palette, 16);
}

static void
fuck_init(VteTerminal *term)
{
    fuck.font_scale = 1;
    fuck.font = pango_font_description_from_string(FONT);
    for (int i = 0; i < 16; ++i) {
        gdk_rgba_parse(fuck.palette+i, colors[i]);
    }
    fuck.background = fuck.palette[0];

    vte_terminal_spawn_async(
        term, VTE_PTY_DEFAULT,
        cwd, cmd, NULL,
        G_SPAWN_DEFAULT,
        NULL, NULL, NULL,
        -1,
        NULL, NULL, NULL);

    vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_font(term, fuck.font);
    vte_terminal_set_bold_is_bright(term, TRUE);
    vte_terminal_set_font_scale(term, fuck.font_scale);
    vte_terminal_set_enable_bidi(term, FALSE);

    g_signal_connect(term, "window-title-changed", G_CALLBACK(fuck_update_title), NULL);
    g_signal_connect(term, "child-exited", G_CALLBACK(fuck_quit), NULL);
}

void
usage(char *prg)
{
    printf("usage: %s [options...]\n\n", prg);
    printf("options:\n");
    printf("  -h           show help and exit\n");
    printf("  -d <dir>     starts %s in selected directory\n", prg);
    printf("  -e <xid>     embeds %s within another X application\n", prg);
    printf("  -a <alpha>   set background opacity (from 0..1, default: %.2f)\n", ALPHA);
    exit(0);
}

void
main(int argc, char **argv)
{
    float alpha = ALPHA;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h")) {
            usage(argv[0]);
        }
        if (!strcmp(argv[i], "-e")) {
            embed = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            cwd = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "-a")) {
            alpha = atof(argv[++i]);
            continue;
        }
        else {
            // TODO: check bounds...
            cmd[1] = "-c";
            for (int j = i; j < argc; ++j) {
                cmd[j-i+2] = argv[j];
            }
            break;
        }
    }

    if (alpha < 0 || alpha > 1) alpha = 1.0;

    gtk_init(&argc, &argv);
    fuck.window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_default_size(fuck.window, WIDTH, HEIGHT);
    gtk_window_set_title(fuck.window, TITLE);

    GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(GTK_WIDGET(fuck.window)));
    gtk_widget_set_visual(GTK_WIDGET(fuck.window), visual);

    g_signal_connect(GTK_WIDGET(fuck.window), "key-press-event", G_CALLBACK(fuck_key_press), NULL);
    g_signal_connect(GTK_WIDGET(fuck.window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    fuck.term = vte_terminal_new();
    gtk_container_add(GTK_CONTAINER(fuck.window), fuck.term);
    fuck_init(VTE_TERMINAL(fuck.term));

    // calling it here because this function conveniently also sets the palette
    fuck_set_alpha_scale(alpha);
    gtk_widget_show_all(GTK_WIDGET(fuck.window));
    gtk_widget_grab_focus(fuck.term);

    Window parent;
    if (embed && (parent = strtol(embed, NULL, 0))) {
        Display *display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
        Window xwindow = gdk_x11_window_get_xid(gtk_widget_get_window(GTK_WIDGET(fuck.window)));
        XReparentWindow(display, xwindow, parent, 0, 0);
    }

    gtk_main();
}
