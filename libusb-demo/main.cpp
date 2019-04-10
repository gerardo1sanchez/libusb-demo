
#include <stdlib.h>
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <gtk/gtk.h>
#include <libusb.h>

static GtkWidget *window = NULL;
static GtkTreeModel *model = NULL;
static GtkWidget *btn = NULL;

enum
{
  COLUMN_VENDOR_ID,
  COLUMN_PRODUCT_ID,
  COLUMN_VENDOR_NAME,
  COLUMN_PRODUCT_NAME,
  NUM_COLUMNS
};

void ustoa(uint16_t value, char (&dest)[5])
{
	for (int j = 3; j >= 0; j--) {
		if ((value & 0xF) >= 10)
		{
			dest[j] = 'A' + (value & 0xF) - 10;
		}
		else
		{
			dest[j] = '0' + (value & 0xF);
		}
		value >>= 4;
	}
	dest[4] = 0;
}

static GtkTreeModel *create_model(void) {
	GtkListStore *store;
	GtkTreeIter iter;
	libusb_device **devs;
	libusb_device *dev;
	int r, i = 0;
	ssize_t cnt;
	char vendor_id[5], product_id[5];

	/* create list store */
	store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

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

		ustoa(desc.idVendor, vendor_id);
		ustoa(desc.idProduct, product_id);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
								COLUMN_VENDOR_ID, vendor_id,
								COLUMN_PRODUCT_ID, product_id,
								COLUMN_VENDOR_NAME, "",
								COLUMN_PRODUCT_NAME, "",
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
	column = gtk_tree_view_column_new_with_attributes ("Vendor Name",
																		renderer,
																		"text",
																		COLUMN_VENDOR_NAME,
																		NULL);
	gtk_tree_view_column_set_sort_column_id (column, COLUMN_VENDOR_NAME);
	gtk_tree_view_append_column (treeview, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Product Name",
																		renderer,
																		"text",
																		COLUMN_PRODUCT_NAME,
																		NULL);
	gtk_tree_view_column_set_sort_column_id (column, COLUMN_PRODUCT_NAME);
	gtk_tree_view_append_column (treeview, column);
}

static gboolean window_closed (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	model = NULL;
	window = NULL;
	btn = NULL;
	return FALSE;
}

void widget_destroyed(GtkWidget *widget, GtkWidget **widget_pointer) {
	gtk_widget_destroyed(widget, widget_pointer);
	gtk_main_quit();
}

size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
	data->append((char*)ptr, size * nmemb);
	return size * nmemb;
}

int requestUrl(const std::string &url, std::string &response) {
	CURL* curl = curl_easy_init();
	if (!curl) return 0;

	CURLcode ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	ret = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	ret = curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.42.0");
	ret = curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
	ret = curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

	std::string header_string;
	std::string response_string;

	ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
	ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
	ret = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

	long response_code;
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	ret = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	size_t begin = response_string.find("Name: ");
	if (begin == std::string::npos) return 0;
	begin += 6;
	size_t end = response_string.find("</div>", begin);
	if (end == std::string::npos) return 0;
	response = response_string.substr(begin, end - begin);
	return 1;
}

int requestUsbInfo(std::string & vendorId, std::string & productId, std::string &vendorName, std::string &productName) {

	std::string url = "https://usb-ids.gowdy.us/read/UD/";
	url += vendorId;

	if (!requestUrl(url, vendorName)) return 0;

	url += "/";
	url += productId;
	if (requestUrl(url, productName)) return 2;
	return 1;
}

std::string vendorIdSelected, productIdSelected;
GtkTreeSelection *treeSel = NULL;

void on_button_clicked(GtkButton *button, gpointer user_data) {
	if (!treeSel) return;
	std::string vendorName, productName;
	if (!requestUsbInfo(vendorIdSelected, productIdSelected, vendorName, productName)) {
		GtkWidget* msgbox = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", "Disculpe, pero ha ocurrido un error durante la consulta web");
		g_signal_connect_swapped(msgbox, "response", G_CALLBACK(gtk_widget_destroy), msgbox);
		gtk_widget_show_all(msgbox);
		return;
	}
	if (productName.length() == 0) productName = "** no disponible **";
	GtkTreeIter iter;
	GtkTreeModel *model;
	if (!gtk_tree_selection_get_selected(treeSel, &model, &iter)) return;
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_VENDOR_NAME, vendorName.c_str());
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_PRODUCT_NAME, productName.c_str());
}

void on_treeview_item_changed(GtkWidget *widget, gpointer user_data) {
	if (!btn) return;

	GtkTreeIter iter;
	GtkTreeModel *model;
	char *value;

	treeSel = GTK_TREE_SELECTION(widget);
	if (!gtk_tree_selection_get_selected(treeSel, &model, &iter)) return;
	gtk_tree_model_get(model, &iter, COLUMN_VENDOR_ID, &value, -1);
	vendorIdSelected = value;
	g_free(value);
	gtk_tree_model_get(model, &iter, COLUMN_PRODUCT_ID, &value, -1);
	productIdSelected = value;
	g_free(value);
	gtk_widget_set_sensitive(btn, TRUE);
	gtk_button_set_label(GTK_BUTTON(btn), "Buscar Vendor Id & Product Id seleccionado en internet");
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
		gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview), COLUMN_VENDOR_NAME);

		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		g_signal_connect(sel, "changed", G_CALLBACK(on_treeview_item_changed), NULL);

		g_object_unref (model);

		gtk_container_add (GTK_CONTAINER (sw), treeview);

		btn = gtk_button_new_with_label("Seleccione un dispositivo USB de la lista para buscarlo en Internet usando libcurl");
		gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);
		gtk_widget_set_sensitive(btn, FALSE);
		g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), NULL);
		gtk_widget_show(btn);

		/* add columns to the tree view */
		add_columns (GTK_TREE_VIEW (treeview));

		/* finish & show */
		gtk_window_set_default_size (GTK_WINDOW (window), 350, 250);
		g_signal_connect (window, "delete-event", G_CALLBACK (window_closed), NULL);
	}

	if (!gtk_widget_get_visible (window))
	{
		GtkWidget *msgbox;
		gtk_widget_show_all (window);
		msgbox = gtk_message_dialog_new(GTK_WINDOW (window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", "El siguiente programa fue realizado con la intension de demostrar habilidades basicas en los siguientes topicos:\r\n\r\n1. lenguaje C/C++\r\n2. GTK\r\n3. libusb\r\n4. libcurl\r\n5. Git/GitHub\r\n6. Visual Studio 2017\r\n\r\n------> Instrucciones de Uso <------\r\n\r\nAl arrancar el programa aparecera una lista de dispositivos USB conectados al equipo, mostrando solo el ID de vendedor y producto, seleccione un dispositivo de la lista y pulse el boton de buscar para realizar la busqueda en Internet del nombre del vendedor y producto\r\n\r\n----> MEJORAS PENDIENTES <----\r\n1. Multithreading\r\n2.Google Test/Mock\r\n3. Compilar en Eclipse\r\n4. Ejecutar en Ubuntu");
		g_signal_connect_swapped(msgbox, "response", G_CALLBACK(gtk_widget_destroy), msgbox);
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