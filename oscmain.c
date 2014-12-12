#include <errno.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <iio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "osc.h"

extern GtkWidget *notebook;
extern GtkWidget *infobar;
extern GtkWidget *tooltips_en;
extern GtkWidget *main_window;
extern struct iio_context *ctx;

static void infobar_hide_cb(GtkButton *btn, gpointer user_data)
{
	gtk_widget_set_visible(infobar, false);
}

static void infobar_reconnect_cb(GtkMenuItem *btn, gpointer user_data)
{
	struct iio_context *new_ctx = iio_context_clone(ctx);
	if (new_ctx) {
		application_reload(new_ctx);
		gtk_widget_set_visible(infobar, false);
	}
}

static void tooltips_enable_cb (GtkCheckMenuItem *item, gpointer data)
{
	gboolean enable;
	GdkScreen *screen;
	GtkSettings *settings;

	screen = gtk_window_get_screen(GTK_WINDOW(main_window));
	settings = gtk_settings_get_for_screen(screen);
	enable = gtk_check_menu_item_get_active(item);
	g_object_set(settings, "gtk-enable-tooltips", enable, NULL);
}

static void init_application (const char *ini_fn)
{
	GtkBuilder *builder = NULL;
	GtkWidget  *window;
	GtkWidget  *btn_capture;
	GtkWidget  *infobar_close, *infobar_reconnect;

	builder = gtk_builder_new();

	if (!gtk_builder_add_from_file(builder, "./osc.glade", NULL)) {
		gtk_builder_add_from_file(builder, OSC_GLADE_FILE_PATH "osc.glade", NULL);
	} else {
		GtkImage *logo;
		GtkAboutDialog *about;
		GdkPixbuf *pixbuf;
		GError *err = NULL;

		/* We are running locally, so load the local files */
		logo = GTK_IMAGE(gtk_builder_get_object(builder, "about_ADI_logo"));
		g_object_set(logo, "file","./icons/ADIlogo.png", NULL);
		logo = GTK_IMAGE(gtk_builder_get_object(builder, "about_IIO_logo"));
		g_object_set(logo, "file","./icons/IIOlogo.png", NULL);
		about = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "About_dialog"));
		logo = GTK_IMAGE(gtk_builder_get_object(builder, "image_capture"));
		g_object_set(logo, "file","./icons/osc_capture.png", NULL);
		logo = GTK_IMAGE(gtk_builder_get_object(builder, "image_generator"));
		g_object_set(logo, "file","./icons/osc_generator.png", NULL);
		pixbuf = gdk_pixbuf_new_from_file("./icons/osc128.png", &err);
		if (pixbuf) {
			g_object_set(about, "logo", pixbuf,  NULL);
			g_object_unref(pixbuf);
		}
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "main_menu"));
	notebook = GTK_WIDGET(gtk_builder_get_object(builder, "notebook"));
	btn_capture = GTK_WIDGET(gtk_builder_get_object(builder, "new_capture_plot"));
	tooltips_en = GTK_WIDGET(gtk_builder_get_object(builder, "menuitem_tooltips_en"));
	main_window = window;

	infobar = GTK_WIDGET(gtk_builder_get_object(builder, "infobar1"));
	infobar_close = GTK_WIDGET(gtk_builder_get_object(builder, "infobar_close"));
	infobar_reconnect = GTK_WIDGET(gtk_builder_get_object(builder, "infobar_reconnect"));

	/* Connect signals. */
	g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(application_quit), NULL);
	g_signal_connect(G_OBJECT(btn_capture), "activate", G_CALLBACK(new_plot_cb), NULL);
	g_signal_connect(G_OBJECT(tooltips_en), "toggled", G_CALLBACK(tooltips_enable_cb), NULL);

	g_signal_connect(G_OBJECT(infobar_close), "clicked", G_CALLBACK(infobar_hide_cb), NULL);
	g_signal_connect(G_OBJECT(infobar_reconnect), "clicked", G_CALLBACK(infobar_reconnect_cb), NULL);

	dialogs_init(builder);

	ctx = osc_create_context();
	if (ctx)
		do_init(ctx);
	gtk_widget_show(window);
}

static void usage(char *program)
{
	printf("%s: the IIO visualization and control tool\n", program);
	printf( " Copyright (C) Analog Devices, Inc. and others\n"
		" This is free software; see the source for copying conditions.\n"
		" There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A\n"
		" PARTICULAR PURPOSE.\n\n");

	/* please keep this list sorted in alphabetical order */
	printf( "Command line options:\n"
		"\t-p\tload specific profile\n");

	printf("\nEnvironmental variables:\n"
		"\tOSC_FORCE_PLUGIN\tforce loading of a specfic plugin\n");

	exit(-1);
}

static void sigterm (int signum)
{
	application_quit();
}

gint main (int argc, char **argv)
{
	int c;

	char *profile = NULL;

	opterr = 0;
	while ((c = getopt (argc, argv, "p:")) != -1)
		switch (c) {
			case 'p':
				profile = strdup(optarg);
				break;
			case '?':
				usage(argv[0]);
				break;
			default:
				printf("Unknown command line option\n");
				usage(argv[0]);
				break;
		}

#ifndef __MINGW32__
	/* XXX: Enabling threading when compiling for Windows will lock the UI
	 * as soon as the main window is moved. */
	gdk_threads_init();
#endif
	gtk_init(&argc, &argv);

	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);
#ifndef __MINGW32__
	signal(SIGHUP, sigterm);
#endif

	if (profile && strncmp(profile, "-", 1) == 0)
		profile = NULL;

	if (profile) {
		char buf[1024];
		strncpy(buf, profile, sizeof(buf));
		profile = check_inifile(buf) ? strdup(buf) : NULL;
	}

	if (!profile) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "%s/" DEFAULT_PROFILE_NAME,
				getenv("HOME"));
		if (check_inifile(buf))
			profile = strdup(buf);
	}

	gdk_threads_enter();
	init_application(profile);
	c = load_default_profile(profile, false);
	create_default_plot();
	if (c == 0)
		gtk_main();
	else
		application_quit();
	gdk_threads_leave();

	if (profile)
	    free(profile);

	if (c == 0 || c == -ENOTTY)
		return 0;
	else
		return -1;
}