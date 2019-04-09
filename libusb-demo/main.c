#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkTreeModel *model = NULL;

enum
{
  COLUMN_VENDOR_ID,
  COLUMN_PRODUCT_ID,
  COLUMN_DESCRIPTION,
  NUM_COLUMNS
};

#include <libusb.h>

static GtkTreeModel *create_model(void) {
	GtkListStore *store;
	GtkTreeIter iter;
	libusb_device **devs;
	libusb_device *dev;
	int r, j, i = 0;
	ssize_t cnt;
	char vendor_id[5], product_id[5];

	/* create list store */
	store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	r = libusb_init(NULL);
	if (r < 0) return NULL;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) return NULL;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return NULL;
		}

		for(j = 3; j >= 0; j--) {
			if((desc.idVendor & 0xF) >= 10)
			{
				vendor_id[j] = 'A' + (desc.idVendor & 0xF) - 10;
			}
			else
			{
				vendor_id[j] = '0' + (desc.idVendor & 0xF);
			}
			desc.idVendor >>= 4;
		}
		vendor_id[4] = 0;

		for(j = 3; j >= 0; j--) {
			if(((int)desc.idProduct & 0xF) >= 10)
			{
				product_id[j] = 'A' + (desc.idProduct & 0xF) - 10;
			}
			else
			{
				product_id[j] = '0' + (desc.idProduct & 0xF);
			}
			desc.idProduct >>= 4;
		}
		product_id[4] = 0;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
								COLUMN_VENDOR_ID, vendor_id,
								COLUMN_PRODUCT_ID, product_id,
								COLUMN_DESCRIPTION, "",
								-1);
	}

	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);

	return GTK_TREE_MODEL (store);
}

static void add_columns (GtkTreeView *treeview) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Vendor ID",
																		renderer,
																		"text",
																		COLUMN_VENDOR_ID,
																		NULL);
	gtk_tree_view_column_set_sort_column_id (column, COLUMN_VENDOR_ID);
	gtk_tree_view_append_column (treeview, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Product ID",
																		renderer,
																		"text",
																		COLUMN_PRODUCT_ID,
																		NULL);
	gtk_tree_view_column_set_sort_column_id (column, COLUMN_PRODUCT_ID);
	gtk_tree_view_append_column (treeview, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Description",
																		renderer,
																		"text",
																		COLUMN_DESCRIPTION,
																		NULL);
	gtk_tree_view_column_set_sort_column_id (column, COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (treeview, column);
}

static gboolean
window_closed (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	model = NULL;
	window = NULL;
	return FALSE;
}

void widget_destroyed(GtkWidget *widget, GtkWidget **widget_pointer) {
	gtk_widget_destroyed(widget, widget_pointer);
	gtk_main_quit();
}

GtkWidget *do_list_store (GtkWidget *do_widget) {
	if (!window) {
		GtkWidget *vbox;
		GtkWidget *label;
		GtkWidget *sw;
		GtkWidget *treeview;

		/* create window, etc */
		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_screen (GTK_WINDOW (window),
										gtk_widget_get_screen (do_widget));
		gtk_window_set_title (GTK_WINDOW (window), "Programa demostrativo de libusb y gtk");

		g_signal_connect (window, "destroy",
								G_CALLBACK (widget_destroyed), &window);

		gtk_container_set_border_width (GTK_CONTAINER (window), 8);

		vbox = gtk_vbox_new (FALSE, 8);
		gtk_container_add (GTK_CONTAINER (window), vbox);

		label = gtk_label_new ("La siguiente es una lista de los dispositivos USB conectados en este equipo");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
															GTK_SHADOW_ETCHED_IN);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
													GTK_POLICY_NEVER,
													GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

		/* create tree model */
		model = create_model ();

		/* create tree view */
		treeview = gtk_tree_view_new_with_model (model);
		gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
		gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
													COLUMN_DESCRIPTION);

		g_object_unref (model);

		gtk_container_add (GTK_CONTAINER (sw), treeview);

		/* add columns to the tree view */
		add_columns (GTK_TREE_VIEW (treeview));

		/* finish & show */
		gtk_window_set_default_size (GTK_WINDOW (window), 350, 250);
		g_signal_connect (window, "delete-event",
								G_CALLBACK (window_closed), NULL);
	}

	if (!gtk_widget_get_visible (window))
	{
		GtkWidget *msgbox;
		gtk_widget_show_all (window);
		msgbox = gtk_message_dialog_new(GTK_WINDOW (window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", "El siguiente es un programa muy sencillo realizado con la unica intension de demostrar habilidades basicas con el lenguaje C, GTK y la libreria libusb");
		g_signal_connect_swapped(msgbox, "response",
			G_CALLBACK(gtk_widget_destroy),
			msgbox);
		gtk_widget_show_all(msgbox);
	}
	else
	{
		gtk_widget_destroy (window);
		window = NULL;
	}

	return window;
}

#include "minwindef.h"

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	gtk_init (0, 0);

	do_list_store(NULL);

	gtk_main ();
}