#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "interface.h"
#include "quantity.h"
#include "oslogo.h"

#define HANDLE_ERROR(string, error) \
    if(error) g_error("(%s): %s: %s", __func__, string, error->message);
#define SPEED_OF_LIGHT 2.99792458e8
#define PLANCK 6.62606896e-34
#define PUMP_WAVELENGTH (1064.1e-9 / 2)

enum BeamCombination {
    SIGNAL_IDLER,
    SIGNAL_1064,
    IDLER_1064,
    NUM_BEAM_COMBINATIONS
};
enum BeamUnit {
    WAVELENGTHS,
    FREQUENCIES,
    NUM_BEAM_UNITS
};
enum EnergyUnit {
    WAVENUMBERS,
    TERAHERTZ,
    ZEPTOJOULES,
    NUM_ENERGY_UNITS
};
enum LockFlags {
    PUMP_UNLOCK = 1 << 0,
    PUMP_LOCK = 1 << 1,
    STOKES_UNLOCK = 1 << 2,
    STOKES_LOCK = 1 << 3,
    PROBE_UNLOCK = 1 << 4,
    PROBE_LOCK = 1 << 5,
    ANTISTOKES_UNLOCK = 1 << 6,
    ANTISTOKES_LOCK = 1 << 7,
    RAMAN_UNLOCK = 1 << 8,
    RAMAN_LOCK = 1 << 9
};

static PQuantityUnitInfo beam_units[] = {
    { "nm", 1.0e9, FALSE, 1, 0.1 },
    { "THz", SPEED_OF_LIGHT * 1.0e-12, TRUE, 1, 0.1 }
};
static PQuantityUnitInfo energy_units[] = {
    { "cm<sup>-1</sup>", 1.0e-2, FALSE, 0, 1 },
    { "THz", SPEED_OF_LIGHT * 1.0e-12, FALSE, 1, 0.1 },
    { "zJ", PLANCK * SPEED_OF_LIGHT * 1.0e21, FALSE, 2, 0.01 }
};

struct Data {
    /* widgets */
    GtkWidget *main_window;
    GtkWidget *beam_combination;
    GtkWidget *beam_units;
    GtkWidget *energy_units;
    GtkWidget *degenerate_box;

    /* Quantity displays */
    PQuantity *raman;
    PQuantity *signal;
    PQuantity *antistokes;
    PQuantity *pump;
    PQuantity *stokes;
    PQuantity *probe;
    PQuantity *free_antistokes;
    PQuantity *free_raman;

    /* state */
    enum EnergyUnit units;
    enum BeamUnit display;
    enum BeamCombination mode;
    gboolean degenerate;
    guint link_handler[2];
};
static struct Data *d = NULL;

static void
calculate_opo_from_signal(void)
{
    gdouble signal = p_quantity_get_value(d->signal);
    gdouble invpump = 1.0 / PUMP_WAVELENGTH;
    gdouble raman = 0.0, antistokes = 0.0;

    switch(d->mode) {
        case SIGNAL_IDLER:
            raman = 2.0 / signal - invpump;
            antistokes = 1.0 / (3.0 / signal - invpump);
            break;
        case SIGNAL_1064:
            raman = 1.0 / signal - 0.5 * invpump;
            antistokes = 1.0 / (2.0 / signal - 0.5 * invpump);
            break;
        case IDLER_1064:
            raman = 1.0 / signal - 0.5 * invpump;
            antistokes = signal;
            break;
        default:
            g_assert_not_reached();
    }

    p_quantity_set_value_no_notify(d->raman, raman);
    p_quantity_set_value_no_notify(d->antistokes, antistokes);
}

static void
calculate_opo_from_raman(void)
{
    double raman = p_quantity_get_value(d->raman);
    double invpump = 1.0 / PUMP_WAVELENGTH;
    double signal = 0.0, antistokes = 0.0;

    switch(d->mode) {
        case SIGNAL_IDLER:
            signal = 2.0 / (raman + invpump);
            antistokes = 1.0 / (3.0 / signal - invpump);
            break;
        case SIGNAL_1064:
            signal = 1.0 / (raman + 0.5 * invpump);
            antistokes = 1.0 / (2.0 / signal - 0.5 * invpump);
            break;
        case IDLER_1064:
            signal = 1.0 / (raman + 0.5 * invpump);
            antistokes = signal;
            break;
        default:
            g_assert_not_reached();
    }

    p_quantity_set_value_no_notify(d->signal, signal);
    p_quantity_set_value_no_notify(d->antistokes, antistokes);
}

static void
calculate_opo_from_antistokes(void)
{
    double antistokes = p_quantity_get_value(d->antistokes);
    double invpump = 1.0 / PUMP_WAVELENGTH;
    double raman = 0.0, signal = 0.0;

    switch(d->mode) {
        case SIGNAL_IDLER:
            signal = 3.0 / (1.0 / antistokes + invpump);
            raman = 2.0 / signal - invpump;
            break;
        case SIGNAL_1064:
            signal = 2.0 / (1.0 / antistokes + 0.5 * invpump);
            raman = 1.0 / signal - 0.5 * invpump;
            break;
        case IDLER_1064:
            signal = antistokes;
            raman = 1.0 / signal - 0.5 * invpump;
            break;
        default:
            g_assert_not_reached();
    }

    p_quantity_set_value_no_notify(d->raman, raman);
    p_quantity_set_value_no_notify(d->signal, signal);

}

static gboolean
check_locked(guint flags)
{
    if(flags & PUMP_UNLOCK && p_quantity_get_locked(d->pump))
        return FALSE;
    if(flags & PUMP_LOCK && !p_quantity_get_locked(d->pump))
        return FALSE;
    if(flags & STOKES_UNLOCK && p_quantity_get_locked(d->stokes))
        return FALSE;
    if(flags & STOKES_LOCK && !p_quantity_get_locked(d->stokes))
        return FALSE;
    if(flags & PROBE_UNLOCK && p_quantity_get_locked(d->probe))
        return FALSE;
    if(flags & PROBE_LOCK && !p_quantity_get_locked(d->probe))
        return FALSE;
    if(flags & ANTISTOKES_UNLOCK && p_quantity_get_locked(d->free_antistokes))
        return FALSE;
    if(flags & ANTISTOKES_LOCK && !p_quantity_get_locked(d->free_antistokes))
        return FALSE;
    if(flags & RAMAN_UNLOCK && p_quantity_get_locked(d->free_raman))
        return FALSE;
    if(flags & RAMAN_LOCK && !p_quantity_get_locked(d->free_raman))
        return FALSE;
    return TRUE;
}

static void
error_locked(PQuantity *quantity)
{
    p_quantity_set_inconsistent(quantity, TRUE);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(d->main_window),
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
        "You have locked too many parameters to complete that calculation.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
set_consistent(void)
{
    p_quantity_set_inconsistent(d->pump, FALSE);
    p_quantity_set_inconsistent(d->stokes, FALSE);
    p_quantity_set_inconsistent(d->probe, FALSE);
    p_quantity_set_inconsistent(d->free_antistokes, FALSE);
    p_quantity_set_inconsistent(d->free_raman, FALSE);
}

static void
calculate_free_from_pump(void)
{
    gdouble pump = p_quantity_get_value(d->pump);
    if(check_locked(ANTISTOKES_UNLOCK | RAMAN_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble probe = p_quantity_get_value(d->probe);
        p_quantity_set_value_no_notify(d->free_raman,
            1.0 / pump - 1.0 / stokes);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / pump - 1.0 / stokes + 1.0 / probe));
    } else if(check_locked(RAMAN_LOCK | ANTISTOKES_UNLOCK | STOKES_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        gdouble probe = p_quantity_get_value(d->probe);
        gdouble stokes = 1.0 / (1.0 / pump - raman);
        p_quantity_set_value_no_notify(d->stokes, stokes);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / pump - 1.0 / stokes + 1.0 / probe));
    } else if(check_locked(ANTISTOKES_LOCK | RAMAN_UNLOCK | STOKES_UNLOCK)) {
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        gdouble probe = p_quantity_get_value(d->probe);
        gdouble raman = 1.0 / antistokes - 1.0 / probe;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->stokes, 1.0 / (1.0 / pump - raman));
    } else if(!d->degenerate &&
        check_locked(ANTISTOKES_LOCK | RAMAN_LOCK | STOKES_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        p_quantity_set_value_no_notify(d->stokes,
            1.0 / (1.0 / pump - raman));
    } else if(!d->degenerate && check_locked(STOKES_LOCK | ANTISTOKES_LOCK |
        PROBE_UNLOCK | RAMAN_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        gdouble raman = 1.0 / pump - 1.0 / stokes;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->probe,
            1.0 / (1.0 / antistokes - raman));
    } else {
        error_locked(d->pump);
        return;
    }
    set_consistent();
}

static void
calculate_free_from_stokes(void)
{
    gdouble stokes = p_quantity_get_value(d->stokes);
    if(check_locked(ANTISTOKES_UNLOCK | RAMAN_UNLOCK)) {
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble probe = p_quantity_get_value(d->probe);
        p_quantity_set_value_no_notify(d->free_raman,
            1.0 / pump - 1.0 / stokes);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / pump - 1.0 / stokes + 1.0 / probe));
    } else if(check_locked(RAMAN_LOCK | ANTISTOKES_UNLOCK |
        PUMP_UNLOCK | PROBE_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        gdouble pump = 1.0 / (1.0 / stokes + raman);
        gdouble probe = d->degenerate? pump : p_quantity_get_value(d->probe);
        p_quantity_set_value_no_notify(d->pump, pump);
        p_quantity_set_value_no_notify(d->probe, probe);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / pump - 1.0 / stokes + 1.0 / probe));
    } else if(!d->degenerate && check_locked(ANTISTOKES_LOCK | PUMP_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        p_quantity_set_value_no_notify(d->pump,
            1.0 / (1.0 / stokes + raman));
    } else if(!d->degenerate && check_locked(ANTISTOKES_LOCK | PUMP_LOCK |
        PROBE_UNLOCK | RAMAN_UNLOCK)) {
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        gdouble raman = 1.0 / pump - 1.0 / stokes;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->probe,
            1.0 / (1.0 / antistokes - raman));
    } else {
        error_locked(d->stokes);
        return;
    }
    set_consistent();
}

static void
calculate_free_from_probe(void)
{
    gdouble probe = p_quantity_get_value(d->probe);
    if(!d->degenerate && check_locked(ANTISTOKES_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / probe + raman));
    } else if(check_locked(ANTISTOKES_UNLOCK | RAMAN_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble pump = p_quantity_get_value(d->pump);
        p_quantity_set_value_no_notify(d->free_raman,
            1.0 / pump - 1.0 / stokes);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / pump - 1.0 / stokes + 1.0 / probe));
    } else if(check_locked(RAMAN_LOCK | ANTISTOKES_UNLOCK | STOKES_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        gdouble pump = p_quantity_get_value(d->pump);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / probe + raman));
        p_quantity_set_value_no_notify(d->stokes, 1.0 / (1.0 / pump - raman));
    } else if(check_locked(ANTISTOKES_LOCK | RAMAN_UNLOCK | STOKES_UNLOCK)) {
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble raman = 1.0 / antistokes - 1.0 / probe;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->stokes, 1.0 / (1.0 / pump - raman));
    } else if(!d->degenerate && check_locked(STOKES_LOCK | ANTISTOKES_LOCK |
        PUMP_UNLOCK | RAMAN_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        gdouble raman = 1.0 / antistokes - 1.0 / probe;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->pump,
            1.0 / (1.0 / stokes + raman));
    } else {
        error_locked(d->probe);
        return;
    }
    set_consistent();
}

static void
calculate_free_from_antistokes(void)
{
    gdouble antistokes = p_quantity_get_value(d->free_antistokes);
    if(!d->degenerate && check_locked(PROBE_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        p_quantity_set_value_no_notify(d->probe,
            1.0 / (1.0 / antistokes - raman));
    } else if(d->degenerate && check_locked(PROBE_UNLOCK | STOKES_UNLOCK)) {
        gdouble raman = p_quantity_get_value(d->free_raman);
        gdouble probe = 1.0 / (1.0 / antistokes - raman);
        p_quantity_set_value_no_notify(d->pump, probe);
        p_quantity_set_value_no_notify(d->probe, probe);
        p_quantity_set_value_no_notify(d->stokes,
            1.0 / (1.0 / probe - raman));
    } else if(check_locked(PROBE_LOCK | RAMAN_UNLOCK | STOKES_UNLOCK)) {
        gdouble probe = p_quantity_get_value(d->probe);
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble raman = 1.0 / antistokes - 1.0 / probe;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->stokes, 1.0 / (1.0 / pump - raman));
    } else if(!d->degenerate && check_locked(STOKES_LOCK | PROBE_LOCK |
        PUMP_UNLOCK | RAMAN_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble probe = p_quantity_get_value(d->probe);
        gdouble raman = 1.0 / antistokes - 1.0 / probe;
        p_quantity_set_value_no_notify(d->free_raman, raman);
        p_quantity_set_value_no_notify(d->pump,
            1.0 / (1.0 / stokes + raman));
    } else {
        error_locked(d->free_antistokes);
        return;
    }
    set_consistent();
}

static void
calculate_free_from_raman(void)
{
    gdouble raman = p_quantity_get_value(d->free_raman);
    if(!d->degenerate && check_locked(STOKES_UNLOCK | PROBE_UNLOCK)) {
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        p_quantity_set_value_no_notify(d->stokes,
            1.0 / (1.0 / pump - raman));
        p_quantity_set_value_no_notify(d->probe,
            1.0 / (1.0 / antistokes - raman));
    } else if(check_locked(STOKES_UNLOCK | ANTISTOKES_UNLOCK)) {
        gdouble pump = p_quantity_get_value(d->pump);
        gdouble probe = p_quantity_get_value(d->probe);
        p_quantity_set_value_no_notify(d->stokes,
            1.0 / (1.0 / pump - raman));
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / probe + raman));
    } else if(check_locked(STOKES_LOCK | PUMP_UNLOCK | ANTISTOKES_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble pump = 1.0 / (1.0 / stokes + raman);
        gdouble probe = d->degenerate? pump : p_quantity_get_value(d->probe);
        p_quantity_set_value_no_notify(d->pump, pump);
        if(d->degenerate)
            p_quantity_set_value_no_notify(d->probe, probe);
        p_quantity_set_value_no_notify(d->free_antistokes,
            1.0 / (1.0 / probe + raman));
    } else if(!d->degenerate && check_locked(STOKES_LOCK | ANTISTOKES_LOCK |
        PUMP_UNLOCK | PROBE_UNLOCK)) {
        gdouble stokes = p_quantity_get_value(d->stokes);
        gdouble antistokes = p_quantity_get_value(d->free_antistokes);
        p_quantity_set_value_no_notify(d->pump,
            1.0 / (1.0 / stokes + raman));
        p_quantity_set_value_no_notify(d->probe,
            1.0 / (1.0 / antistokes - raman));
    } else {
        error_locked(d->free_raman);
        return;
    }
    set_consistent();
}

void G_MODULE_EXPORT
on_beam_combination_changed(GtkComboBox *combobox)
{
    d->mode = gtk_combo_box_get_active(combobox);
}

void G_MODULE_EXPORT
on_beam_units_changed(GtkComboBox *combobox)
{
    d->display = gtk_combo_box_get_active(combobox);
    p_quantity_set_unit(d->signal, d->display);
    p_quantity_set_unit(d->antistokes, d->display);
    p_quantity_set_unit(d->pump, d->display);
    p_quantity_set_unit(d->stokes, d->display);
    p_quantity_set_unit(d->probe, d->display);
    p_quantity_set_unit(d->free_antistokes, d->display);
}

void G_MODULE_EXPORT
on_energy_units_changed(GtkComboBox *combobox)
{
    d->units = gtk_combo_box_get_active(combobox);
    p_quantity_set_unit(d->raman, d->units);
    p_quantity_set_unit(d->free_raman, d->units);
}

void G_MODULE_EXPORT
on_degenerate_toggled(GtkToggleButton *togglebutton)
{
    d->degenerate = gtk_toggle_button_get_active(togglebutton);
    if(d->degenerate) {
        p_quantity_set_value(d->probe, p_quantity_get_value(d->pump));
        g_signal_handler_unblock(d->pump, d->link_handler[0]);
        g_signal_handler_unblock(d->probe, d->link_handler[1]);
    } else {
        g_signal_handler_block(d->pump, d->link_handler[0]);
        g_signal_handler_block(d->probe, d->link_handler[1]);
    }
}

static void
on_pump_probe_lock_changed(PQuantity *quantity)
{
    if(d->degenerate) {
        gboolean lock = p_quantity_get_locked(quantity);
        if(p_quantity_get_locked(d->pump) != lock)
            p_quantity_set_locked(d->pump, lock);
        if(p_quantity_get_locked(d->probe) != lock)
            p_quantity_set_locked(d->probe, lock);
    }
}

static void
on_pump_probe_changed(PQuantity *quantity, gdouble value,
    PQuantity *other_quantity)
{
    p_quantity_set_value_no_notify(other_quantity, value);
}

static void
create_main_window(void)
{
    GError *error = NULL;
    GtkBuilder *builder = gtk_builder_new();

    /* Build interface */
    gtk_builder_add_from_string(builder, interface_string, -1, &error);
    HANDLE_ERROR("Could not build interface", error);

    /* Get pointers to widgets */
    d->main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    d->beam_combination =
        GTK_WIDGET(gtk_builder_get_object(builder, "beam_combination"));
    d->beam_units =
        GTK_WIDGET(gtk_builder_get_object(builder, "beam_units"));
    d->energy_units =
        GTK_WIDGET(gtk_builder_get_object(builder, "energy_units"));

    /* Calculate initial values */
    gdouble raman = 300000.0;
    gdouble invpump = 1.0 / PUMP_WAVELENGTH;
    gdouble pumpprobe = 2.0 / (raman + invpump);
    gdouble stokes = 1.0 / (invpump - 1.0 / pumpprobe);
    gdouble antistokes = 1.0 / (3.0 / pumpprobe - invpump);

    /* Set up quantity displays */
    d->raman = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "raman_shift")),
        GTK_LABEL(gtk_builder_get_object(builder, "raman_shift_unit")), NULL,
        raman, 0.0, 1018900.0, NUM_ENERGY_UNITS, energy_units);
    d->signal = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "signal")),
        GTK_LABEL(gtk_builder_get_object(builder, "signal_unit")), NULL,
        pumpprobe, 690.0e-9, 1064.1e-9, NUM_BEAM_UNITS, beam_units);
    d->antistokes = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "antistokes")),
        GTK_LABEL(gtk_builder_get_object(builder, "antistokes_unit")), NULL,
        antistokes, 405.1e-9, 1064.1e-9, NUM_BEAM_UNITS, beam_units);
    d->pump = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "pump")),
        GTK_LABEL(gtk_builder_get_object(builder, "pump_unit")),
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "pump_lock")),
        pumpprobe, 0.0, 2000.0e-9, NUM_BEAM_UNITS, beam_units);
    d->stokes = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "stokes")),
        GTK_LABEL(gtk_builder_get_object(builder, "stokes_unit")),
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "stokes_lock")),
        stokes, 0.0, 2000.0e-9, NUM_BEAM_UNITS, beam_units);
    d->probe = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "probe")),
        GTK_LABEL(gtk_builder_get_object(builder, "probe_unit")),
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "probe_lock")),
        pumpprobe, 0.0, 2000.0e-9, NUM_BEAM_UNITS, beam_units);
    d->free_antistokes = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "free_antistokes")),
        GTK_LABEL(gtk_builder_get_object(builder, "free_antistokes_unit")),
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "antistokes_lock")),
        antistokes, 0.0, 4000.0e-9, NUM_BEAM_UNITS, beam_units);
    d->free_raman = p_quantity_new(
        GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "free_raman_shift")),
        GTK_LABEL(gtk_builder_get_object(builder, "free_raman_shift_unit")),
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "raman_shift_lock")),
        raman, 0.0, 500000.0, NUM_ENERGY_UNITS, energy_units);

    /* Build combo box menu for Raman shift units; can't do this automatically,
    because we have a custom renderer that uses the "markup" property instead
    of "text", which is necessary because of the superscript in cm^-1 */
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    GtkTreeIter iter;
    int i;
    for(i = 0; i < NUM_ENERGY_UNITS; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, energy_units[i].display_name, -1);
    }
    gtk_combo_box_set_model(GTK_COMBO_BOX(d->energy_units),
        GTK_TREE_MODEL(store));
    g_object_unref(store); /* Reference now owned by combo box */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(d->energy_units),
        renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(d->energy_units), renderer,
        "markup", 0);

    /* Set active items on combo boxes */
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->beam_combination), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->beam_units), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->energy_units), 0);

    /* Connect signals */
    gtk_builder_connect_signals(builder, NULL);
    g_object_unref(builder);
    g_signal_connect(d->beam_combination, "changed",
        G_CALLBACK(calculate_opo_from_signal), NULL);
    g_signal_connect_after(d->raman, "changed",
        G_CALLBACK(calculate_opo_from_raman), NULL);
    g_signal_connect_after(d->signal, "changed",
        G_CALLBACK(calculate_opo_from_signal), NULL);
    g_signal_connect_after(d->antistokes, "changed",
        G_CALLBACK(calculate_opo_from_antistokes), NULL);
    g_signal_connect_after(d->pump, "changed",
        G_CALLBACK(calculate_free_from_pump), NULL);
    g_signal_connect_after(d->stokes, "changed",
        G_CALLBACK(calculate_free_from_stokes), NULL);
    g_signal_connect_after(d->probe, "changed",
        G_CALLBACK(calculate_free_from_probe), NULL);
    g_signal_connect_after(d->free_antistokes, "changed",
        G_CALLBACK(calculate_free_from_antistokes), NULL);
    g_signal_connect_after(d->free_raman, "changed",
        G_CALLBACK(calculate_free_from_raman), NULL);
    g_signal_connect(d->pump, "lock-changed",
        G_CALLBACK(on_pump_probe_lock_changed), NULL);
    g_signal_connect(d->probe, "lock-changed",
        G_CALLBACK(on_pump_probe_lock_changed), NULL);
    d->link_handler[0] = g_signal_connect(d->pump, "changed",
        G_CALLBACK(on_pump_probe_changed), d->probe);
    d->link_handler[1] = g_signal_connect(d->probe, "changed",
        G_CALLBACK(on_pump_probe_changed), d->pump);

    /* Initialize state */
    d->units = WAVENUMBERS;
    d->display = WAVELENGTHS;
    d->mode = SIGNAL_IDLER;
    d->degenerate = TRUE;
}

static void
data_free(void)
{
    g_object_unref(d->raman);
    g_object_unref(d->signal);
    g_object_unref(d->antistokes);
    g_slice_free(struct Data, d);
}

int
main(int argc, char *argv[])
{
    /* Initialize GTK+ */
    gtk_init(&argc, &argv);

    /* Load icons */
    GdkPixbuf *oslogo =
        gdk_pixbuf_new_from_inline(-1, oslogo_data, FALSE, NULL);
    GdkPixbuf *oslogo_16 =
        gdk_pixbuf_new_from_inline(-1, oslogo_16_data, FALSE, NULL);
    gtk_icon_theme_add_builtin_icon("oslogo", 48, oslogo);
    gtk_icon_theme_add_builtin_icon("oslogo", 16, oslogo_16);
    g_object_unref(oslogo);
    g_object_unref(oslogo_16);

    d = g_slice_new0(struct Data);

    /* Create the main window */
    create_main_window();

    /* Enter the main loop */
    gtk_widget_show_all(d->main_window);
    gtk_main();

    data_free();
    return 0;
}
