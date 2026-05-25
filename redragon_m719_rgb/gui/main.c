#include <gtk/gtk.h>
#include <hidapi/hidapi.h>
#include <string.h>
#include <stdlib.h>

#define REDRAGON_VID      0x25a7 
#define REDRAGON_PID_CABO 0xfab0  
#define REDRAGON_PID_24GH 0xfa7e  
#define TARGET_INTERFACE  1 

// --- ESTRUTURA PARA CONTROLAR OS COMPONENTES DA INTERFACE ---
typedef struct {
    int static_selected_color_idx; 
    int resp_selected_color_idx;   

    GtkWidget *static_brightness_scale;
    GtkWidget *resp_brightness_scale;
    GtkWidget *resp_speed_drop; 
    GtkWidget *status_label;
} AppWidgets;

// Estrutura de dados para mapear as cores da paleta
typedef struct {
    double r, g, b;         // Escala 0.0 a 1.0 para renderização na interface (Cairo)
    int r_usb, g_usb, b_usb; // Escala 0 a 255 enviada no payload do Hardware
    const char *nome;
} CorPaleta;

// Definição estrita: Primárias, Secundárias e Branco
CorPaleta paleta[] = {
    {1.0, 0.0, 0.0, 255, 0, 0, "Vermelho"},
    {0.0, 1.0, 0.0, 0, 255, 0, "Verde"},
    {0.0, 0.0, 1.0, 0, 0, 255, "Azul"},
    {1.0, 1.0, 0.0, 255, 255, 0, "Amarelo"},
    {0.0, 1.0, 1.0, 0, 255, 255, "Ciano"},
    {1.0, 0.0, 1.0, 255, 0, 255, "Magenta"},
    {1.0, 1.0, 1.0, 255, 255, 255, "Branco"}
};
#define NUM_CORES (sizeof(paleta) / sizeof(paleta[0]))

// --- FUNÇÃO AUXILIAR PARA AJUSTAR MARGENS ---
void set_margins_all(GtkWidget *widget, int margin) {
    gtk_widget_set_margin_top(widget, margin);
    gtk_widget_set_margin_bottom(widget, margin);
    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
}

// --- FUNÇÃO DE COMUNICAÇÃO E ENVIO USB ---
void enviar_payload_mouse(AppWidgets *widgets, unsigned char mode_byte, int r, int g, int b, int brilho, int velocidade) {
    unsigned char payload[17];
    memset(payload, 0, sizeof(payload));

    payload[0] = 0x08; payload[1] = 0x07; payload[2] = 0x00; payload[3] = 0x00;
    payload[4] = 0xa0; payload[5] = 0x07; payload[6] = mode_byte; 
    payload[7] = (unsigned char)r; payload[8] = (unsigned char)g; payload[9] = (unsigned char)b;

    int cores_ativas = (r > 0) + (g > 0) + (b > 0);

    if (mode_byte == 0x04) { // APAGADO (OFF)
        payload[10] = 0x96; payload[11] = 0xff; payload[12] = 0xbe;
    } 
    else if (mode_byte == 0x00) { // STREAMING (RGB ONDA)
        payload[10] = 0x13; payload[11] = 0xff; payload[12] = 0x44;
    } 
    else if (mode_byte == 0x01) { // RESPIRAÇÃO
        unsigned char brightness_byte = (brilho == 10) ? 0x19 : (brilho == 50) ? 0x7f : 0xff;
        unsigned char speed_byte = (velocidade == 1) ? 0xff : (velocidade == 3) ? 0x28 : 0x96;
        payload[10] = speed_byte; payload[11] = brightness_byte;  
        
        unsigned char base_idx12 = 0xc0; 
        if (speed_byte == 0x28) { 
            base_idx12 = (brightness_byte == 0xff) ? 0x2e : (brightness_byte == 0x19) ? 0x48 : 0x3b; 
        } else if (speed_byte == 0x96) { 
            base_idx12 = (brightness_byte == 0xff) ? 0xc0 : (brightness_byte == 0x19) ? 0xa6 : 0xb3;
        } else if (speed_byte == 0xff) { 
            base_idx12 = (brightness_byte == 0xff) ? 0x57 : (brightness_byte == 0x19) ? 0x3d : 0x4a;
        }
        payload[12] = (cores_ativas > 0) ? base_idx12 + (cores_ativas - 1) : base_idx12;
    } 
    else { // ESTÁTICO (0x02)
        if (brilho == 10) {
            payload[10] = 0x96; payload[11] = 0x19; 
            unsigned char base_idx12_static = 0xa5;
            payload[12] = (cores_ativas > 1) ? base_idx12_static + (cores_ativas - 1) : base_idx12_static;
        } 
        else if (brilho == 50) {
            payload[10] = 0xcb; payload[11] = 0x7f; 
            unsigned char base_idx12_static = 0x7d;
            payload[12] = (cores_ativas > 1) ? base_idx12_static + (cores_ativas - 1) : base_idx12_static;
        } 
        else { // 100% Brilho
            if (cores_ativas <= 1) {
                payload[10] = 0xff; payload[11] = 0xff; payload[12] = 0x56;
            } else {
                payload[10] = 0x96; payload[11] = 0xff;
                payload[12] = (r == 255 && g == 255 && b == 255) ? 0xc1 : 0xc0;
            }
        }
    }
    payload[13] = 0x00; payload[14] = 0x00; payload[15] = 0x00; payload[16] = 0x4a;

    if (hid_init() < 0) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Erro: Falha ao inicializar HIDAPI.");
        return;
    }

    struct hid_device_info *devs = hid_enumerate(REDRAGON_VID, 0x0000);
    struct hid_device_info *cur_dev = devs;
    char *path_to_open = NULL;
    int pid_encontrado = 0;
    
    while (cur_dev) {
        if ((cur_dev->product_id == REDRAGON_PID_CABO || cur_dev->product_id == REDRAGON_PID_24GH) 
            && cur_dev->interface_number == TARGET_INTERFACE) {
            path_to_open = g_strdup(cur_dev->path);
            pid_encontrado = cur_dev->product_id;
            break;
        }
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);

    if (!path_to_open) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Status: Mouse M719 não encontrado!");
        hid_exit();
        return;
    }

    hid_device *handle = hid_open_path(path_to_open);
    g_free(path_to_open);

    if (!handle) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Status: Erro de permissão (use udev/sudo).");
        hid_exit();
        return;
    }

    int res = hid_send_feature_report(handle, payload, sizeof(payload));
    if (res < 0) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Status: Falha ao enviar comando USB.");
    } else {
        const char *conexao = (pid_encontrado == REDRAGON_PID_24GH) ? "Sem Fio (2.4G)" : "Com Cabo";
        char msg[128];
        g_snprintf(msg, sizeof(msg), "Status: Aplicado com sucesso via %s!", conexao);
        gtk_label_set_text(GTK_LABEL(widgets->status_label), msg);
    }

    hid_close(handle);
    hid_exit();
}

// --- FUNÇÃO DE DESENHO CUSTOMIZADA PARA OS BOTÕES (CAIRO) ---
static void draw_color_block(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    int idx = GPOINTER_TO_INT(user_data);
    
    // Pinta a área total do botão com a cor correspondente da paleta
    cairo_set_source_rgb(cr, paleta[idx].r, paleta[idx].g, paleta[idx].b);
    cairo_paint(cr);

    // Contorno escuro interno para dar acabamento estético (essencial para o Branco)
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_stroke(cr);
}

// --- CALLBACKS DOS SELETORES EM GRADE ---
static void on_static_color_selected(GtkToggleButton *button, gpointer user_data) {
    if (gtk_toggle_button_get_active(button)) {
        AppWidgets *w = (AppWidgets *)g_object_get_data(G_OBJECT(button), "widgets");
        w->static_selected_color_idx = GPOINTER_TO_INT(user_data);
    }
}

static void on_resp_color_selected(GtkToggleButton *button, gpointer user_data) {
    if (gtk_toggle_button_get_active(button)) {
        AppWidgets *w = (AppWidgets *)g_object_get_data(G_OBJECT(button), "widgets");
        w->resp_selected_color_idx = GPOINTER_TO_INT(user_data);
    }
}

// --- CALLBACKS DE AÇÃO (CLIQUES NOS BOTÕES DE APLICAR) ---
static void on_btn_static_clicked(GtkWidget *btn, gpointer user_data) {
    AppWidgets *w = (AppWidgets *)user_data;
    int idx = w->static_selected_color_idx;
    int brilho = (int)gtk_range_get_value(GTK_RANGE(w->static_brightness_scale));
    enviar_payload_mouse(w, 0x02, paleta[idx].r_usb, paleta[idx].g_usb, paleta[idx].b_usb, brilho, 2);
}

static void on_btn_respiration_clicked(GtkWidget *btn, gpointer user_data) {
    AppWidgets *w = (AppWidgets *)user_data;
    int idx = w->resp_selected_color_idx;
    int brilho = (int)gtk_range_get_value(GTK_RANGE(w->resp_brightness_scale));
    int velocidade = gtk_drop_down_get_selected(GTK_DROP_DOWN(w->resp_speed_drop)) + 1; 
    enviar_payload_mouse(w, 0x01, paleta[idx].r_usb, paleta[idx].g_usb, paleta[idx].b_usb, brilho, velocidade);
}

static void on_btn_streaming_clicked(GtkWidget *btn, gpointer user_data) {
    AppWidgets *w = (AppWidgets *)user_data;
    enviar_payload_mouse(w, 0x00, 255, 0, 0, 100, 2);
}

static void on_btn_off_clicked(GtkWidget *btn, gpointer user_data) {
    AppWidgets *w = (AppWidgets *)user_data;
    enviar_payload_mouse(w, 0x04, 255, 0, 255, 100, 2);
}

// --- CONSTRUÇÃO DO AMBIENTE GRÁFICO (ATIVAÇÃO DO APP) ---
static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_new0(AppWidgets, 1);
    widgets->static_selected_color_idx = 0; 
    widgets->resp_selected_color_idx = 0;   

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Redragon M719 RGB Controller");
    gtk_window_set_default_size(GTK_WINDOW(window), 460, 420);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins_all(main_box, 15);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(main_box), notebook);

    // --- ABA 1: MODO ESTÁTICO ---
    GtkWidget *box_static = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins_all(box_static, 10);
    gtk_box_append(GTK_BOX(box_static), gtk_label_new("Quadro de Cores Ativas (Selecione uma):"));
    
    GtkWidget *palette_box_static = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(palette_box_static, GTK_ALIGN_CENTER);
    GtkWidget *first_btn_static = NULL;

    for (size_t i = 0; i < NUM_CORES; i++) {
        GtkWidget *btn_cor = gtk_toggle_button_new();
        gtk_widget_set_tooltip_text(btn_cor, paleta[i].nome);
        
        GtkWidget *area_desenho = gtk_drawing_area_new();
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area_desenho), 48);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area_desenho), 40);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area_desenho), draw_color_block, GINT_TO_POINTER(i), NULL);
        gtk_button_set_child(GTK_BUTTON(btn_cor), area_desenho);

        if (i == 0) {
            first_btn_static = btn_cor;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_cor), TRUE);
        } else {
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn_cor), GTK_TOGGLE_BUTTON(first_btn_static));
        }

        g_object_set_data(G_OBJECT(btn_cor), "widgets", widgets);
        g_signal_connect(btn_cor, "toggled", G_CALLBACK(on_static_color_selected), GINT_TO_POINTER(i));
        gtk_box_append(GTK_BOX(palette_box_static), btn_cor);
    }
    gtk_box_append(GTK_BOX(box_static), palette_box_static);

    gtk_box_append(GTK_BOX(box_static), gtk_label_new("Brilho (10%, 50%, 100%):"));
    widgets->static_brightness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10, 100, 45);
    gtk_scale_add_mark(GTK_SCALE(widgets->static_brightness_scale), 10, GTK_POS_BOTTOM, "10%");
    gtk_scale_add_mark(GTK_SCALE(widgets->static_brightness_scale), 50, GTK_POS_BOTTOM, "50%");
    gtk_scale_add_mark(GTK_SCALE(widgets->static_brightness_scale), 100, GTK_POS_BOTTOM, "100%");
    gtk_range_set_value(GTK_RANGE(widgets->static_brightness_scale), 100);
    gtk_box_append(GTK_BOX(box_static), widgets->static_brightness_scale);

    GtkWidget *btn_apply_static = gtk_button_new_with_label("Aplicar Modo Estático");
    g_signal_connect(btn_apply_static, "clicked", G_CALLBACK(on_btn_static_clicked), widgets);
    gtk_box_append(GTK_BOX(box_static), btn_apply_static);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_static, gtk_label_new("Estático"));

    // --- ABA 2: MODO RESPIRAÇÃO ---
    GtkWidget *box_resp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins_all(box_resp, 10);
    gtk_box_append(GTK_BOX(box_resp), gtk_label_new("Quadro de Cores Ativas (Selecione uma):"));
    
    GtkWidget *palette_box_resp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(palette_box_resp, GTK_ALIGN_CENTER);
    GtkWidget *first_btn_resp = NULL;

    for (size_t i = 0; i < NUM_CORES; i++) {
        GtkWidget *btn_cor = gtk_toggle_button_new();
        gtk_widget_set_tooltip_text(btn_cor, paleta[i].nome);
        
        GtkWidget *area_desenho = gtk_drawing_area_new();
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area_desenho), 48);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area_desenho), 40);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area_desenho), draw_color_block, GINT_TO_POINTER(i), NULL);
        gtk_button_set_child(GTK_BUTTON(btn_cor), area_desenho);

        if (i == 0) {
            first_btn_resp = btn_cor;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_cor), TRUE);
        } else {
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn_cor), GTK_TOGGLE_BUTTON(first_btn_resp));
        }

        g_object_set_data(G_OBJECT(btn_cor), "widgets", widgets);
        g_signal_connect(btn_cor, "toggled", G_CALLBACK(on_resp_color_selected), GINT_TO_POINTER(i));
        gtk_box_append(GTK_BOX(palette_box_resp), btn_cor);
    }
    gtk_box_append(GTK_BOX(box_resp), palette_box_resp);

    gtk_box_append(GTK_BOX(box_resp), gtk_label_new("Brilho (10%, 50%, 100%):"));
    widgets->resp_brightness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10, 100, 45);
    gtk_scale_add_mark(GTK_SCALE(widgets->resp_brightness_scale), 10, GTK_POS_BOTTOM, "10%");
    gtk_scale_add_mark(GTK_SCALE(widgets->resp_brightness_scale), 50, GTK_POS_BOTTOM, "50%");
    gtk_scale_add_mark(GTK_SCALE(widgets->resp_brightness_scale), 100, GTK_POS_BOTTOM, "100%");
    gtk_range_set_value(GTK_RANGE(widgets->resp_brightness_scale), 100);
    gtk_box_append(GTK_BOX(box_resp), widgets->resp_brightness_scale);

    gtk_box_append(GTK_BOX(box_resp), gtk_label_new("Velocidade do Efeito:"));
    const char *opcoes_velocidade[] = {"1 - Lento", "2 - Médio", "3 - Rápido", NULL};
    widgets->resp_speed_drop = gtk_drop_down_new_from_strings(opcoes_velocidade);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(widgets->resp_speed_drop), 1); 
    gtk_box_append(GTK_BOX(box_resp), widgets->resp_speed_drop);

    GtkWidget *btn_apply_resp = gtk_button_new_with_label("Aplicar Modo Respiração");
    g_signal_connect(btn_apply_resp, "clicked", G_CALLBACK(on_btn_respiration_clicked), widgets);
    gtk_box_append(GTK_BOX(box_resp), btn_apply_resp);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_resp, gtk_label_new("Respiração"));

    // --- ABA 3: MODO STREAMING (ONDA FLUIDA) ---
    GtkWidget *box_stream = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins_all(box_stream, 10);
    gtk_widget_set_valign(box_stream, GTK_ALIGN_CENTER);

    GtkWidget *lbl_stream = gtk_label_new("Modo Onda RGB Fluido Ativo.\n(Velocidade e Brilho Fixados pelo Hardware)");
    gtk_label_set_justify(GTK_LABEL(lbl_stream), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(box_stream), lbl_stream);

    GtkWidget *btn_apply_stream = gtk_button_new_with_label("Aplicar Modo Streaming");
    g_signal_connect(btn_apply_stream, "clicked", G_CALLBACK(on_btn_streaming_clicked), widgets);
    gtk_box_append(GTK_BOX(box_stream), btn_apply_stream);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_stream, gtk_label_new("Streaming"));

    // --- COMPONENTES GLOBAIS DE RODAPÉ ---
    GtkWidget *btn_off = gtk_button_new_with_label("Apagar Todos os LEDs");
    g_signal_connect(btn_off, "clicked", G_CALLBACK(on_btn_off_clicked), widgets);
    gtk_box_append(GTK_BOX(main_box), btn_off);

    widgets->status_label = gtk_label_new("Status: Pronto.");
    gtk_widget_set_halign(widgets->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), widgets->status_label);

    gtk_window_present(GTK_WINDOW(window));
}

// --- FUNÇÃO MAIN PRINCIPAL ---
int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.redragon.m719control", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}