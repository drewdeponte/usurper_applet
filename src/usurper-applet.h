#ifndef USURPER_APPLET_H
#define USURPER_APPLET_H

#include <panel-applet.h>
#include <gtk/gtk.h>
#include <glib.h>

#define MAX_CARD_PATH_LEN 500  /* The max allowed length of path to a card */
#define MAX_SET_PATH_LEN 500    /* The max allowed length of a set's path */
#define MAX_SET_NAME_LEN 64     /* The max allowed length of a set's name */
#define MAX_NUM_SETS 50         /* The max allowed number of sets */
#define MAX_NUM_CARDS 200        /* The max allowed num of cards per set */

/**
 * Card Info Structure
 *
 * This structure and typedef create data type that can be used to
 * represent a flash card.
 */
typedef struct card {
    gchar q_path[MAX_CARD_PATH_LEN];    /* path to cards question pdf file */
    gchar a_path[MAX_CARD_PATH_LEN];    /* path to cands answer pdf file */
} card_t;

/**
 * Card Set Structure
 *
 * This structure and typedef create a data type that can be used to
 * represent a set of flash cards.
 */
typedef struct card_set {
    gchar path[MAX_SET_PATH_LEN];   /* path to associated card set */
    gchar name[MAX_SET_NAME_LEN];   /* name of associated card set */
} card_set_t;

/**
 * Preferences Structure
 *
 * A structure to encapselate all the info needed to run an instance of
 * the flash card program, the interval at which to quiz, the num of
 * cards to quiz each interval, the listing of the necessary card's
 * info, and the number of cards in the set.
 */
typedef struct prefs {
    guint quiz_interval;             /* interval at which to quiz in mins */
    gint num_cards_to_quiz;          /* num cards to quiz each interval */
    card_set_t card_sets[MAX_NUM_SETS];  /* array of set info entries */
    gint num_card_sets;              /* tot num of cards in this set */
    gint cur_card_set;               /* index selec card set (-1 = none) */
    gint edit_card_set;              /* indx sel edit card set (-1 = none) */
    card_t cards[MAX_NUM_CARDS];     /* array of cards for cur set */
    gint num_cards_in_cur_set;;      /* num of cards in cur set */
    char *card_sets_path;            /* path of dir containing card sets */
    char *new_set_path;              /* path of the new set being created */
    int new_card_count;              /* counter for creating new cards */
} prefs_t;

/**
 * Applet State Structure
 *
 * A structure designed to store any state tracking information for the
 * applet.
 */
typedef struct {
    gboolean pref_window_shown;
    guint quiz_src_id;
} app_state_t;

/* --- FUNCTIONS --- */

/**
 * Display Quick Message
 *
 * The quick_message() function takes in a message and creates and
 * displays a small dialog box to display the message.
 */
static void quick_message(gchar *message);

/* === PDF Rendering/Support Functions === */

/**
 * Clear Drawing Area
 *
 * The clear_drawing_area() function clears the drawing area of the
 * passed dest_draw_area to all white. It also clears the provided
 * dest_pixmap to all white as well and then updates the display.
 */
static int clear_drawing_area(GdkPixmap *dest_pixmap,
                              GtkWidget *dest_draw_area);

/**
 * Render a PDF flash card
 *
 * The render_pdf_flash_card() takes a given 3in by 5in pdf file and
 * renders it to the given dest_pixmap, and dest_draw_area. Once, it has
 * rendered the PDF it procedes to update the drawing area so that the
 * new rendered PDF is displayed.
 */
static int render_pdf_flash_card(const char *path_to_file,
                                 GdkPixmap *dest_pixmap,
                                 GtkWidget *dest_draw_area);

/**
 * Give Quiz
 *
 * Quiz the user on the randomly selected subset.
 */
static gboolean quiz(gpointer data);

#endif
