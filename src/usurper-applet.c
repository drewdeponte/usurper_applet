#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include <panel-applet.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <unistd.h>

#include <poppler.h>

#include "usurper-applet.h"

/* State and Preferences Tracking Variables */
prefs_t prefs;
app_state_t state;
int *rand_subset;
int cur_r_index;

/* Globally Accessible Widgets */
GtkWidget *q_textarea;
GtkWidget *a_textarea;
GtkWidget *q_draw_area;
GdkPixmap *q_pixmap;
GtkWidget *entry;
GtkWidget *card_disp_window;

static void disp_answer_clicked_callback(GtkWidget *widget,
    gpointer user_data) {

    int r_index;

    r_index = rand_subset[cur_r_index];

    /* display the answer for the cur_r_index */
    render_pdf_flash_card(prefs.cards[r_index].a_path, q_pixmap,
                          q_draw_area);
}

static void incorrect_clicked_callback(GtkWidget *widget,
    gpointer user_data) {

    int r_index;

    if (cur_r_index < (prefs.num_cards_to_quiz - 1)) {
        cur_r_index++;
    
        clear_drawing_area(q_pixmap, q_draw_area);
        r_index = rand_subset[cur_r_index];
        render_pdf_flash_card(prefs.cards[r_index].q_path, q_pixmap,
                              q_draw_area);
    } else {
        gtk_widget_destroy(card_disp_window);

        free((void *)rand_subset);
        rand_subset = NULL;

        state.quiz_src_id = g_timeout_add(((prefs.quiz_interval * 1000) * 60),
                                          quiz, NULL);
    }
}

static void correct_clicked_callback(GtkWidget *widget,
    gpointer user_data) {
    
    int r_index;

    if (cur_r_index < (prefs.num_cards_to_quiz - 1)) {
        cur_r_index++;
    
        clear_drawing_area(q_pixmap, q_draw_area);
        r_index = rand_subset[cur_r_index];
        render_pdf_flash_card(prefs.cards[r_index].q_path, q_pixmap,
                              q_draw_area);
    } else {
        gtk_widget_destroy(card_disp_window);

        free((void *)rand_subset);
        rand_subset = NULL;

        state.quiz_src_id = g_timeout_add(((prefs.quiz_interval * 1000) * 60),
                                          quiz, NULL);
    }
}

static gboolean q_conf_event_callback(GtkWidget *widget,
    GdkEventConfigure *event) {

    if (q_pixmap)
        g_object_unref(q_pixmap);

    q_pixmap = gdk_pixmap_new(widget->window,
                              widget->allocation.width,
                              widget->allocation.height, -1);

    gdk_draw_rectangle(q_pixmap, widget->style->white_gc, TRUE, 0, 0,
                       widget->allocation.width, widget->allocation.height);

    return TRUE;
}

static gboolean q_expose_event_callback(GtkWidget *widget,
    GdkEventExpose *event, gpointer data) {

    gdk_draw_drawable(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)], GDK_DRAWABLE(q_pixmap),
        event->area.x, event->area.y, event->area.x, event->area.y,
        event->area.width, event->area.height);

    return FALSE;
}

/**
 * Creates the Card Display Window
 *
 * Creates a window which allows for the rendering of the question and
 * answer portions of flash cards as well as control interface for
 * displaying the answer and recording ones successes and failures.
 */
static GtkWidget *create_card_display_window(void) {
    GtkWidget *window, *h_box, *v_button_box;
    GtkWidget *q_frame, *control_frame, *response_frame;
    GtkWidget *disp_answer_button, *correct_button, *incorrect_button;
    GtkWidget *v_response_box;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    h_box = gtk_hbox_new(FALSE, 5);

    /* Create a frame for the question/answer display and setup a
     * drawing area inside it so that the question/answer portion of the
     * card may be rendered. */
    q_frame = gtk_frame_new("Question/Answer");
    q_draw_area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(q_draw_area),
                          2*(5 * 72), 2*(3 * 72));
    g_signal_connect(G_OBJECT(q_draw_area), "realize",
                     G_CALLBACK(q_conf_event_callback), NULL);
    g_signal_connect(G_OBJECT(q_draw_area), "expose_event",
                     G_CALLBACK(q_expose_event_callback), NULL);
    gtk_container_add(GTK_CONTAINER(q_frame), q_draw_area);
    gtk_box_pack_start(GTK_BOX(h_box), q_frame, FALSE, FALSE, 0);

    /* Create a button box, with two frames in it. One for the control
     * buttons, the response buttons. */
    v_button_box = gtk_vbox_new(TRUE, 5);

    control_frame = gtk_frame_new("Control");
    disp_answer_button = gtk_button_new_with_label("Display Answer");
    g_signal_connect(G_OBJECT(disp_answer_button), "clicked",
                     G_CALLBACK(disp_answer_clicked_callback), NULL);
    gtk_container_add(GTK_CONTAINER(control_frame), disp_answer_button);
    gtk_box_pack_start(GTK_BOX(v_button_box), control_frame, FALSE, FALSE, 0);

    response_frame = gtk_frame_new("Got Answer?");
    v_response_box = gtk_vbox_new(TRUE, 5);
    correct_button = gtk_button_new_with_label("Correct");
    g_signal_connect(G_OBJECT(correct_button), "clicked",
                     G_CALLBACK(correct_clicked_callback), NULL);
    gtk_box_pack_start(GTK_BOX(v_response_box), correct_button, FALSE,
        FALSE, 0);
    incorrect_button = gtk_button_new_with_label("Incorrect");
    g_signal_connect(G_OBJECT(incorrect_button), "clicked",
                     G_CALLBACK(incorrect_clicked_callback), NULL);
    gtk_box_pack_start(GTK_BOX(v_response_box), incorrect_button, FALSE,
        FALSE, 0);
    gtk_container_add(GTK_CONTAINER(response_frame), v_response_box);
    gtk_box_pack_start(GTK_BOX(v_button_box), response_frame, FALSE,
        FALSE, 0);

    gtk_box_pack_start(GTK_BOX(h_box), v_button_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), h_box);
    
    return window;
}

char *get_card_sets_path(void) {
    char *p_home_env;
    char *card_sets_path;
    int path_size;

    p_home_env = getenv("HOME");
    if (p_home_env == NULL) {
        printf("Err: Failed to match HOME environment variable.\n");
        return NULL;
    }

    path_size = strlen(p_home_env) + 20 + 1;
    card_sets_path = (char *)malloc((size_t)path_size);
    if (card_sets_path == NULL) {
        return NULL;
    }

    strncpy((char *)card_sets_path, p_home_env, (size_t)path_size);
    strncat((char *)card_sets_path, "/.usurper/card_sets/", 20);
    card_sets_path[path_size] = '\0';

    return card_sets_path;
}

char *build_new_set_path(const char *new_set_name) {
    int path_size;
    char *path;

    if (prefs.card_sets_path == NULL) {
        prefs.card_sets_path = get_card_sets_path();
        if (prefs.card_sets_path == NULL) {
            return NULL;
        }
    }

    path_size = strlen(prefs.card_sets_path) + strlen(new_set_name) + 1;

    path = (char *)malloc((size_t)path_size);
    if (path == NULL) {
        return NULL;
    }

    strncpy(path, prefs.card_sets_path, path_size);
    strncat(path, new_set_name, strlen(new_set_name));

    return path;
}

int create_new_card(const char *q_buff, const char *a_buff) {
    FILE *tex_file;
    gboolean retval;
    char buff[256];

    char preamb[] = "\\documentclass[12pt]{article}\n"
        "\\setlength{\\pdfpageheight}{3in}\n"
        "\\setlength{\\pdfpagewidth}{5in}\n"
        "\\setlength{\\oddsidemargin}{-.75in}\n"
        "\\setlength{\\evensidemargin}{-.75in}\n"
        "\\setlength{\\topmargin}{-.75in}\n"
        "\\setlength{\\headheight}{0pt}\n"
        "\\setlength{\\headsep}{0pt}\n"
        "\\setlength{\\textwidth}{4.5in}\n"
        "\\setlength{\\textheight}{2.5in}\n"
        "\\begin{document}\n";
    char postamb[] = "\n\\end{document}\n";

    prefs.new_card_count++;

    tex_file = fopen("/tmp/usurper.tex", "w");
    fwrite((const void *)preamb, 1, strlen(preamb), tex_file);
    fwrite(q_buff, 1, strlen(q_buff), tex_file);
    fwrite((const void *)postamb, 1, strlen(postamb), tex_file);
    fclose(tex_file);
    
    /* pdflatex -output-directory /tmp /tmp/usurper.tex */
    retval = g_spawn_command_line_sync(
        "pdflatex -output-directory /tmp /tmp/usurper.tex",
        NULL, NULL, NULL, NULL);

    if (prefs.new_card_count < 10) {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card00%d.0.pdf",
            prefs.new_set_path, prefs.new_card_count);
    } else if (prefs.new_card_count < 100) {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card0%d.0.pdf",
            prefs.new_set_path, prefs.new_card_count);
    } else {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card%d.0.pdf",
            prefs.new_set_path, prefs.new_card_count);
    }
    
    retval = g_spawn_command_line_sync((gchar *)buff,
        NULL, NULL, NULL, NULL);

    /* mv /tmp/usurper.pdf */

    tex_file = fopen("/tmp/usurper.tex", "w");
    fwrite((const void *)preamb, 1, strlen(preamb), tex_file);
    fwrite(a_buff, 1, strlen(a_buff), tex_file);
    fwrite((const void *)postamb, 1, strlen(postamb), tex_file);
    fclose(tex_file);
    
    /* pdflatex -output-directory /tmp /tmp/usurper.tex */
    retval = g_spawn_command_line_sync(
        "pdflatex -output-directory /tmp /tmp/usurper.tex",
        NULL, NULL, NULL, NULL);

    if (prefs.new_card_count < 10) {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card00%d.1.pdf",
            prefs.new_set_path, prefs.new_card_count);
    } else if (prefs.new_card_count < 100) {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card0%d.1.pdf",
            prefs.new_set_path, prefs.new_card_count);
    } else {
        sprintf((char *)buff, "mv /tmp/usurper.pdf %s/card%d.1.pdf",
            prefs.new_set_path, prefs.new_card_count);
    }
    
    retval = g_spawn_command_line_sync((gchar *)buff,
        NULL, NULL, NULL, NULL);
    
    return 0;
}

/**
 * Filter Directory Entries
 *
 * my_dir_filter is a function specifically designed to work as a filter
 * for the scandir() function calls. It basically is passed a directory
 * entry and returns a value of 0 if scandir() should count (keep) the
 * entry and a value of 1 if scandir() should discard (not keep) the
 * entry.
 */
int my_dir_filter(const struct dirent *p_dirent) {
    if (!((strcmp(p_dirent->d_name, ".") == 0) ||
        (strcmp(p_dirent->d_name, "..") == 0))) {
        return 1;
    }
    return 0;
}


/**
 * Display Is Correct Dialog
 *
 * Displays a dialog asking the user if he/she got the answer correct to
 * the question that was asked. This function blocks the calling
 * process until the user selects a response, at which point the
 * function returns a value of type GtkResponseType which represents the
 * users response.
 */
static gint disp_correct_dialog(void) {
    GtkWidget *dialog, *label;
    gint result;

    dialog = gtk_dialog_new_with_buttons("Did You Get It Correct?", NULL,
        GTK_DIALOG_MODAL, GTK_STOCK_YES, GTK_RESPONSE_YES,
        GTK_STOCK_NO, GTK_RESPONSE_NO, NULL);
    label = gtk_label_new("Did You Get It Correct?");

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

    gtk_widget_show_all(dialog);

    result = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return result;
}

/**
 * Show the Display Answer Dialog
 *
 * Displays a dialog asking the user if he/she wants to see the answer
 * to the question that was asked. This function blocks the calling
 * process until the user selects a response, at which point the
 * function returns a value of type GtkResponseType which represents the
 * users response.
 */
static gint disp_answer_dialog(void) {
    GtkWidget *dialog, *label;
    gint result;

    dialog = gtk_dialog_new_with_buttons("Display Answer?", NULL,
        GTK_DIALOG_MODAL, GTK_STOCK_YES, GTK_RESPONSE_YES, NULL);
    label = gtk_label_new("Would you like the answer displayed?");

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

    gtk_widget_show_all(dialog);

    result = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return result;
}


static gboolean quiz(gpointer data) {
    gboolean retval, matched_val;
    gint ans_res, cor_res, i, j, r_index, k;
    gint tmp_index;

    srand((unsigned int)time(NULL));

    /* Create a dynamically allocated array to hold a random subset of
     * index values which map to the currently select card set. This
     * randomly selected subset will be what is quized on. */
    rand_subset = NULL;
    rand_subset = (int *)malloc((sizeof(int) * prefs.num_cards_to_quiz));
    for (j = 0; j < prefs.num_cards_to_quiz; j++) {
        do {
            matched_val = FALSE;
            tmp_index = rand() % prefs.num_cards_in_cur_set;
            for (k = 0; k < j; k++) {
                if (tmp_index == rand_subset[k])
                    matched_val = TRUE;
            }
        } while (matched_val == TRUE);
        rand_subset[j] = tmp_index;
    }

    cur_r_index = 0;
    g_source_remove(state.quiz_src_id);

    card_disp_window = create_card_display_window();
    gtk_widget_show_all(card_disp_window);

    r_index = rand_subset[cur_r_index];
    render_pdf_flash_card(prefs.cards[r_index].q_path, q_pixmap,
                          q_draw_area);

    return TRUE;
}

/**
 * Create the cards array.
 *
 * The create_cards_array functions fills the cards array with card
 * structs containing the correct paths for a given index of a card set.
 */
static gint create_cards_array(gint set_index) {
    char *p_path_pref;
    int cur_card_index;
    struct dirent **namelist;
    int n, i;

    p_path_pref = prefs.card_sets[set_index].path;

    n = scandir((const char *)p_path_pref, &namelist, my_dir_filter,
        alphasort);
    if (n < 0)
        perror("scandir");

    //printf("n = %d\n", n);
    if ((n % 2) != 0) {
        printf("An uneven number of cards exist.\n");
        return -1;
    }

    i = 0;
    cur_card_index = 0;

    while (i < n) {
        //printf("Card with index = %d\n", cur_card_index);
        strncpy(prefs.cards[cur_card_index].q_path,
            (const char *)p_path_pref, (size_t)MAX_CARD_PATH_LEN);
        //printf("prefs.cards[%d].q_path = %s\n",
        //    cur_card_index, prefs.cards[cur_card_index].q_path);
        //printf("namelist[%d]->dname = %s\n", i, namelist[i]->d_name);
        strcat(prefs.cards[cur_card_index].q_path,
            (char *)namelist[i]->d_name);
        //printf("prefs.cards[%d].q_path = %s\n",
        //    cur_card_index, prefs.cards[cur_card_index].q_path);
        free(namelist[i]);

        i++;
        
        strncpy(prefs.cards[cur_card_index].a_path,
            (const char *)p_path_pref, (size_t)MAX_CARD_PATH_LEN);
        //printf("prefs.cards[%d].a_path = %s\n",
        //    cur_card_index, prefs.cards[cur_card_index].a_path);
        //printf("i = %d\n", i);
        //printf("namelist[%d]->dname = %p\n", i, namelist[i]->d_name);
        //printf("namelist[%d]->dname = %s\n", i, namelist[i]->d_name);
        //printf("FUCK!!!!\n");
        strcat(prefs.cards[cur_card_index].a_path,
            (char *)namelist[i]->d_name);
        //printf("prefs.cards[%d].a_path = %s\n",
        //    cur_card_index, prefs.cards[cur_card_index].a_path);
        free(namelist[i]);

        i++;

        cur_card_index++;
    }
    free(namelist);

    prefs.num_cards_in_cur_set = cur_card_index;

    return 0;
}

/**
 * Create the Card Set array.
 *
 * The create_card_set_array functions fills in the card_sets array with
 * all the paths and directory names of the sub-directories within
 * ~/.usurper/card_sets/.
 */
static int create_card_set_array(void) {
    char *p_home_env;
    char *path[MAX_SET_PATH_LEN];
    DIR *card_sets_dir;
    struct dirent *p_dirent;
    int cur_card_index;

    p_home_env = getenv("HOME");
    if (p_home_env == NULL) {
        printf("Err: Failed to match HOME environment variable.\n");
        return 1;
    }

    if (strlen(p_home_env) > (MAX_SET_PATH_LEN - 1)) {
        printf("Err: HOME environment variable is longer than we support.\n");
        return 2;
    }

    strncpy((char *)path, p_home_env, (size_t)MAX_SET_PATH_LEN);
    strncat((char *)path, "/.usurper/card_sets/", 20);

    printf("path = %s.\n", path);

    card_sets_dir = opendir((const char *)path);
    if (card_sets_dir == NULL) {
        return 3;
    }

    cur_card_index = 0;

    while((p_dirent = readdir(card_sets_dir)) != NULL) {
        if (!((strcmp(p_dirent->d_name, ".") == 0) ||
            (strcmp(p_dirent->d_name, "..") == 0))) {

            strncpy(prefs.card_sets[cur_card_index].path, (const char *)path,
                    (size_t)MAX_SET_PATH_LEN);
            strcat(prefs.card_sets[cur_card_index].path, (char *)p_dirent->d_name);
            strcat(prefs.card_sets[cur_card_index].path, "/");

            strncpy(prefs.card_sets[cur_card_index].name,
                    p_dirent->d_name, (size_t)MAX_SET_NAME_LEN);
            cur_card_index = cur_card_index + 1;
        }
    }

    prefs.num_card_sets = cur_card_index;

    return 0;
}

/**
 * Interval Updated Event
 *
 * This function is the callback function that is associated with the
 * interval being updated.
 */
static void interval_updated(GtkSpinButton *spinbutton, gpointer data) {
    prefs.quiz_interval = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(spinbutton));
}

/**
 * Create Quiz Interval Spinner
 *
 * The create_interval_spinner functions creates a GtkSpinner with an
 * attached label appropriately for use in the Preferences window. This
 * functions returns the packing box that holds the label and spinner.
 * The box widget that is returned needs to be shown and handled there
 * after.
 */
static GtkWidget *create_interval_spinner(void) {
    GtkWidget *label, *box, *interval_spinner;
    GtkAdjustment *spinner_adj;

    box = gtk_hbox_new(TRUE, 5);

    spinner_adj = (GtkAdjustment *)gtk_adjustment_new(
                                    (gdouble)prefs.quiz_interval, 1.0, 720.0,
                                    1.0, 5.0, 5.0);
    label = gtk_label_new("Quizing Interval:");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    interval_spinner = gtk_spin_button_new(spinner_adj, 1.0, 0);
    g_signal_connect(G_OBJECT(interval_spinner), "value-changed",
                     G_CALLBACK(interval_updated), NULL);
    gtk_box_pack_start(GTK_BOX(box), interval_spinner, FALSE, FALSE, 0);
    gtk_widget_show(interval_spinner);

    return box;
}

/**
 * Num Cards to Quiz Updated Event
 *
 * This function is the callback function that is associated with the
 * number of cards to be quized per session being updated.
 */
static void num_cards_to_quiz_updated(GtkSpinButton *spinbutton,
    gpointer data) {

    prefs.num_cards_to_quiz = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(spinbutton));
}

/**
 * Create Num Cards to Quiz Spinner
 *
 * The create_num_cards_spinner functions creates and packs a label and
 * GtkSpinner into a packing box appropriate for use in the Preferences
 * window. This function returns a pointer to the packing box widget,
 * which needs to be shown and handled there after.
 */
static GtkWidget *create_num_cards_spinner(void) {
    GtkWidget *label, *box, *num_cards_spinner;
    GtkAdjustment *spinner_adj;

    box = gtk_hbox_new(TRUE, 5);

    spinner_adj = (GtkAdjustment *)gtk_adjustment_new(
                                (gdouble)prefs.num_cards_to_quiz, 1.0,
                                MAX_NUM_CARDS,
                                1.0, 5.0, 5.0);
    label = gtk_label_new("Num Cards to Quiz:");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    num_cards_spinner = gtk_spin_button_new(spinner_adj, 1.0, 0);
    g_signal_connect(G_OBJECT(num_cards_spinner), "value-changed",
                     G_CALLBACK(num_cards_to_quiz_updated), NULL);
    gtk_box_pack_start(GTK_BOX(box), num_cards_spinner, FALSE, FALSE, 0);
    gtk_widget_show(num_cards_spinner);

    return box;
}

/**
 * Card Set Changed Callback
 *
 * The card_set_changed function is a callback that is associated with
 * the current card set changing. It performs all the actions necessary
 * when a user has selected a new card set.
 */
static void card_set_changed(GtkComboBox *widget, gpointer data) {
    prefs.cur_card_set = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    create_cards_array(prefs.cur_card_set);

    g_source_remove(state.quiz_src_id);
    state.quiz_src_id = g_timeout_add(
        ((prefs.quiz_interval * 1000) * 60), quiz, NULL);

    /* quick_message("Changed your card set I see, just started timer."); */
}

/**
 * Create Cur Card Set Combo Box
 *
 * The create_card_set_combo_box creates a combo box and widget in a
 * packing box appropriate for use in the Preferences window. This
 * functions returns a pointer to the packing box so that it may by
 * shown and handled there after.
 */
static GtkWidget *create_card_set_combo_box(void) {
    GtkWidget *label, *box, *combo_box;
    gint i;

    box = gtk_hbox_new(TRUE, 5);

    label = gtk_label_new("Card Set to Quiz:");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    combo_box = gtk_combo_box_new_text();
    g_signal_connect(G_OBJECT(combo_box), "changed",
                     G_CALLBACK(card_set_changed), NULL);
    gtk_box_pack_start(GTK_BOX(box), combo_box, FALSE, FALSE, 0);

    for (i = 0; i < prefs.num_card_sets; i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), prefs.card_sets[i].name);
    }
    
    if (prefs.cur_card_set != -1) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
            prefs.cur_card_set);
    }

    gtk_widget_show(combo_box);

    return box;
}

static void edit_card_set_changed(GtkComboBox *widget, gpointer data) {
    prefs.edit_card_set = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

static gboolean edit_sel_card_callback(GtkWidget *widget,
    GdkEventButton *event, gpointer data) {

    GtkWidget *window;

    window = create_card_display_window();

    gtk_widget_show_all(window);
}

static GtkWidget *create_edit_card_combo_box(void) {
    GtkWidget *button, *box, *combo_box;
    gint i;

    box = gtk_hbox_new(TRUE, 5);
    
    combo_box = gtk_combo_box_new_text();
    g_signal_connect(G_OBJECT(combo_box), "changed",
                     G_CALLBACK(edit_card_set_changed), NULL);
    gtk_box_pack_start(GTK_BOX(box), combo_box, FALSE, FALSE, 0);

    for (i = 0; i < prefs.num_card_sets; i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), prefs.card_sets[i].name);
    }
    
    if (prefs.edit_card_set != -1) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
            prefs.edit_card_set);
    }

    gtk_widget_show(combo_box);

    button = gtk_button_new_with_label("Edit Selected Card Set");
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(edit_sel_card_callback), NULL);

    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    gtk_widget_show(button);

    return box;
}

static gboolean add_another_card_clicked(GtkWidget *widget,
    GdkEventButton *event, gpointer data) {

    GtkTextBuffer *q_buffer, *a_buffer;
    GtkTextIter begin, end;
    gchar *q_buff, *a_buff;

    /* quick_message("Clicked add another card!"); */

    q_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(q_textarea));
    a_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(a_textarea));

    gtk_text_buffer_get_start_iter(q_buffer, &begin);
    gtk_text_buffer_get_end_iter(q_buffer, &end);

    q_buff = gtk_text_buffer_get_text(q_buffer, &begin, &end, FALSE);

    gtk_text_buffer_get_start_iter(a_buffer, &begin);
    gtk_text_buffer_get_end_iter(a_buffer, &end);

    a_buff = gtk_text_buffer_get_text(a_buffer, &begin, &end, FALSE);

    create_new_card((const char *)q_buff, (const char *)a_buff);
    
    gtk_text_buffer_delete(a_buffer, &begin, &end);

    gtk_text_buffer_get_start_iter(q_buffer, &begin);
    gtk_text_buffer_get_end_iter(q_buffer, &end);

    gtk_text_buffer_delete(q_buffer, &begin, &end);
}

static gboolean done_with_card_set_clicked(GtkWidget *widget,
    GdkEventButton *event, gpointer data) {


   quick_message("Clicked done with set!"); 

}

static GtkWidget *create_cards_window(void) {
    GtkWidget *window, *v_box, *h_box;
    GtkWidget *q_frame, *a_frame;
    GtkWidget *new_card_button, *done_with_set_button;
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_title(GTK_WINDOW(window), "Create Flash Cards");

    v_box = gtk_vbox_new(FALSE, 5);

    q_frame = gtk_frame_new("Question");
    q_textarea = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(q_textarea), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(q_textarea), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(q_frame), q_textarea);
    gtk_box_pack_start(GTK_BOX(v_box), q_frame, FALSE, FALSE, 0);
    gtk_widget_show(q_textarea);
    gtk_widget_show(q_frame);

    a_frame = gtk_frame_new("Answer");
    a_textarea = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(a_textarea), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(a_textarea), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(a_frame), a_textarea);
    gtk_box_pack_start(GTK_BOX(v_box), a_frame, FALSE, FALSE, 0); 
    gtk_widget_show(a_textarea);
    gtk_widget_show(a_frame);

    h_box = gtk_hbox_new(TRUE, 5);
    done_with_set_button = gtk_button_new_with_label("Done With Card Set!");
    g_signal_connect(G_OBJECT(done_with_set_button), "clicked",
                     G_CALLBACK(done_with_card_set_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(h_box), done_with_set_button, FALSE,
        FALSE, 0);
    gtk_widget_show(done_with_set_button);
    new_card_button = gtk_button_new_with_label("Add Another Card to Set!");
    g_signal_connect(G_OBJECT(new_card_button), "clicked",
                     G_CALLBACK(add_another_card_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(h_box), new_card_button, FALSE,
        FALSE, 0);
    gtk_widget_show(new_card_button);
    gtk_box_pack_start(GTK_BOX(v_box), h_box, FALSE, FALSE, 0);
    gtk_widget_show(h_box);

    gtk_widget_show(v_box);

    gtk_container_add(GTK_CONTAINER(window), v_box);


    return window;
}

static gboolean create_card_set_button_press(GtkWidget *widget,
    GdkEventButton *event, gpointer data) {
    int r;
    GtkWidget *window;
   
    if (prefs.new_set_path != NULL) {
        free(prefs.new_set_path);
    }

    prefs.new_set_path = build_new_set_path(
        (char *)gtk_entry_get_text(GTK_ENTRY(entry)));
    if (prefs.new_set_path == NULL) {
        quick_message("Err: Failed to build_new_set_path.");
        return FALSE;
    }

    r = mkdir((const char *)prefs.new_set_path, 0755);
    if (r != 0) {
        quick_message("Err: Failed to create new set directory.");
        return FALSE;
    }

    /*
    quick_message(prefs.new_set_path);
    */

    window = create_cards_window();
    gtk_widget_show_all(window);

    return TRUE;
}

static GtkWidget *create_new_card_set_entry(void) {
    GtkWidget *button, *box;

    box = gtk_hbox_new(TRUE, 5);
    
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_SET_NAME_LEN);
    gtk_widget_show(entry);

    button = gtk_button_new_with_label("Create New Card Set");
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(create_card_set_button_press), NULL);
    gtk_widget_show(button);

    return box;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event,
    gpointer data) {

    g_source_remove(state.quiz_src_id);
    state.quiz_src_id = g_timeout_add(
        ((prefs.quiz_interval * 1000) * 60), quiz, NULL);
    state.pref_window_shown = FALSE;

    return FALSE;
}


static GtkWidget *create_pref_window(void) {
    GtkWidget *window, *mainbox, *tmp;
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_set_title(GTK_WINDOW(window), "CyphMemAid Preferences");
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(delete_event), NULL);

    mainbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), mainbox);

    tmp = create_interval_spinner();
    gtk_box_pack_start(GTK_BOX(mainbox), tmp, FALSE, FALSE, 0);
    
    tmp = create_num_cards_spinner();
    gtk_box_pack_start(GTK_BOX(mainbox), tmp, FALSE, FALSE, 0);
    
    tmp = create_card_set_combo_box();
    gtk_box_pack_start(GTK_BOX(mainbox), tmp, FALSE, FALSE, 0);
    
    tmp = create_edit_card_combo_box();
    gtk_box_pack_start(GTK_BOX(mainbox), tmp, FALSE, FALSE, 0);
    
    tmp = create_new_card_set_entry();
    gtk_box_pack_start(GTK_BOX(mainbox), tmp, FALSE, FALSE, 0);
    
    gtk_widget_show_all(window);
    
    return window;
}


static gboolean on_button_press(GtkWidget *event_box,
    GdkEventButton *event, gpointer data) {

    static GtkWidget *window;
    
    /* Don't react to anything other than the left mouse button;
     * return FALSE so the event is passed to the default handler */
    if (event->button != 1)
        return FALSE;

    if (!state.pref_window_shown) {
        window = create_pref_window();
        gtk_widget_show(window);
        state.pref_window_shown = TRUE;
    } else {
        gtk_widget_destroy(window);
        /*
        g_source_remove(state.quiz_src_id);
        state.quiz_src_id = g_timeout_add(
            ((prefs.quiz_interval * 1000) * 60), quiz, NULL);
        */
        state.pref_window_shown = FALSE;
    }

    return TRUE;
}

static gboolean applet_delete_event(GtkWidget *widget, GdkEvent *event,
    gpointer user_data) {

    if (prefs.card_sets_path != NULL) {
        free(prefs.card_sets_path);
    }

    if (prefs.new_set_path != NULL) {
        free(prefs.new_set_path);
    }

    return TRUE;
}

static gboolean usurper_applet_fill(PanelApplet *applet,
    const gchar *iid, gpointer data) {

    GtkWidget *label;
    GtkWidget *event_box;

    if (strcmp (iid, "OAFIID:UsurperApplet") != 0)
        return FALSE;
    
    prefs.quiz_interval = 15;
    prefs.num_cards_to_quiz = 5;
    prefs.cur_card_set = -1;
    prefs.edit_card_set = -1;
    prefs.num_cards_in_cur_set = 0;
    prefs.new_set_path = NULL;
    prefs.card_sets_path = get_card_sets_path();
    if (prefs.card_sets_path == NULL) {
        return FALSE;
    }

    create_card_set_array();

    /* Create some widgets */
    event_box = gtk_event_box_new();
    label = gtk_label_new ("Usurper Applet");

    /* Add the label to the event box, and connect call back */
    gtk_container_add(GTK_CONTAINER(event_box), label);
    g_signal_connect(G_OBJECT(event_box), "button_press_event",
                     G_CALLBACK(on_button_press), NULL);
    
    g_signal_connect(G_OBJECT(applet), "delete_event",
                     G_CALLBACK(applet_delete_event), NULL);

    /* Add the event box to the applet */
    gtk_container_add (GTK_CONTAINER (applet), event_box);

    /* Show the applet */
    gtk_widget_show_all (GTK_WIDGET (applet));

    return TRUE;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:UsurperApplet_Factory",
                             PANEL_TYPE_APPLET,
                             "Factory to create the Usurper applet",
                             "0",
                             usurper_applet_fill,
                             NULL);

static void quick_message(gchar *message) {
    GtkWidget *dialog, *label;

    dialog = gtk_dialog_new_with_buttons("Message",
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);

    label = gtk_label_new(message);
    g_signal_connect_swapped(dialog, "response",
        G_CALLBACK(gtk_widget_destroy), dialog);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
    gtk_widget_show_all(dialog);
}

/* === PDF Rendering/Support Functions === */
static int clear_drawing_area(GdkPixmap *dest_pixmap,
    GtkWidget *dest_draw_area) {

    gdk_draw_rectangle(dest_pixmap, dest_draw_area->style->white_gc,
                       TRUE, 0, 0,
                       dest_draw_area->allocation.width,
                       dest_draw_area->allocation.height);
    
    gdk_draw_drawable(dest_draw_area->window,
        dest_draw_area->style->fg_gc[GTK_WIDGET_STATE(dest_draw_area)],
        GDK_DRAWABLE(dest_pixmap),
        0, 0, 0, 0, -1, -1);

    return 0;
}

static int render_pdf_flash_card(const char *path_to_file,
    GdkPixmap *dest_pixmap, GtkWidget *dest_draw_area) {

    PopplerDocument *document;
    PopplerPage *page;
    GdkPixbuf *pixbuf;
    char *path_buff;
    int path_size;

    path_size = strlen(path_to_file) + 8;

    path_buff = (char *)malloc(path_size);
    if (path_buff == NULL)
        return -1;

    strncpy(path_buff, "file://", 8);
    strncat(path_buff, path_to_file, (path_size - 8));

    document = poppler_document_new_from_file(path_buff, NULL, NULL);
    page = poppler_document_get_page_by_label(document, "1");
    
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 2*(72 * 5),
        2*(72 *3));
    gdk_pixbuf_fill(pixbuf, 0x00106000);

    poppler_page_render_to_pixbuf(page, 0, 0, 2*(72 * 5), 2*(72 * 3), 2, 0,
        pixbuf);

    gdk_draw_pixbuf(dest_pixmap,
                    dest_draw_area->style->white_gc,
                    pixbuf,
                    0, 0, 0, 0,
                    -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
    
    gdk_draw_drawable(dest_draw_area->window,
        dest_draw_area->style->fg_gc[GTK_WIDGET_STATE(dest_draw_area)],
        GDK_DRAWABLE(dest_pixmap),
        0, 0, 0, 0, -1, -1);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(document));

    free(path_buff);

    return 0;
}
