#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "quantity.h"

G_DEFINE_TYPE(PQuantity, p_quantity, G_TYPE_OBJECT);

enum {
    CHANGED_SIGNAL,
    LOCK_CHANGED_SIGNAL,
    LAST_SIGNAL
};
static guint p_quantity_signals[LAST_SIGNAL] = { 0 };

static void
p_quantity_init(PQuantity *self)
{
    self->box = NULL;
    self->label = NULL;
    self->toggle = NULL;
    self->value = 0.0;
    self->unit = 0;
    self->num_units = 0;
    self->unit_info = NULL;
    self->adjustments = NULL;
    self->handler = 0;
    self->locked = FALSE;
}

static void
p_quantity_finalize(GObject *obj)
{
    PQuantity *self = P_QUANTITY(obj);
    guint i;
    for(i = 0; i < self->num_units; i++) {
        g_free(self->unit_info[i]);
        g_object_unref(self->adjustments[i]);
    }
    g_free(self->unit_info);
    g_free(self->adjustments);

    G_OBJECT_CLASS(p_quantity_parent_class)->finalize(obj);
}

static void
p_quantity_class_init(PQuantityClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = p_quantity_finalize;

    p_quantity_signals[CHANGED_SIGNAL] = g_signal_new("changed",
        G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PQuantityClass, changed), NULL, NULL,
        g_cclosure_marshal_VOID__DOUBLE, G_TYPE_NONE, 1, G_TYPE_DOUBLE);
    p_quantity_signals[LOCK_CHANGED_SIGNAL] = g_signal_new("lock-changed",
        G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PQuantityClass, lock_changed), NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static gdouble
value_with_unit(gdouble value, PQuantityUnitInfo *unit)
{
    if(unit->inverse && value == 0)
        return G_MAXDOUBLE;
    return unit->scale_factor * (unit->inverse? (1.0 / value) : value);
}

static void
on_spin_button_changed(GtkSpinButton *button, PQuantity *quantity)
{
    quantity->value = gtk_spin_button_get_value(button) /
        quantity->unit_info[quantity->unit]->scale_factor;
    if(quantity->unit_info[quantity->unit]->inverse)
        quantity->value = 1.0 / quantity->value;
    g_signal_emit_by_name(quantity, "changed", quantity->value);
}

static void
on_lock_toggled(GtkToggleButton *button, PQuantity *quantity)
{
    quantity->locked = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(GTK_WIDGET(quantity->box), !quantity->locked);
    g_signal_emit_by_name(quantity, "lock-changed");
}

PQuantity *
p_quantity_new(GtkSpinButton *box, GtkLabel *label, GtkToggleButton *toggle,
    gdouble value, gdouble min, gdouble max, guint num_units,
    const PQuantityUnitInfo *units)
{
    PQuantity *self = P_QUANTITY(g_object_new(P_TYPE_QUANTITY, NULL));

    self->box = box;
    self->label = label;
    self->toggle = toggle; /* may be NULL */
    self->value = value;
    self->num_units = num_units;

    self->unit_info = g_new0(PQuantityUnitInfo *, num_units);
    self->adjustments = g_new0(GtkAdjustment *, num_units);
    guint i;
    for(i = 0; i < self->num_units; i++) {
        self->unit_info[i] = g_new0(PQuantityUnitInfo, 1);
        self->unit_info[i]->display_name = g_strdup(units[i].display_name);
        self->unit_info[i]->scale_factor = units[i].scale_factor;
        self->unit_info[i]->inverse = units[i].inverse;
        self->unit_info[i]->precision = units[i].precision;
        self->unit_info[i]->step = units[i].step;
        gdouble minconv = value_with_unit(min, self->unit_info[i]);
        gdouble maxconv = value_with_unit(max, self->unit_info[i]);
        self->adjustments[i] = GTK_ADJUSTMENT(
            gtk_adjustment_new(value_with_unit(value, self->unit_info[i]),
            MIN(minconv, maxconv), MAX(minconv, maxconv),
            self->unit_info[i]->step, 10.0 * self->unit_info[i]->step, 0));
        g_object_ref_sink(self->adjustments[i]);
    }

    self->handler = g_signal_connect(self->box, "value-changed",
        G_CALLBACK(on_spin_button_changed), self);
    if(self->toggle)
        g_signal_connect(self->toggle, "toggled",
            G_CALLBACK(on_lock_toggled), self);

    p_quantity_set_unit(self, 0);

    return self;
}

void
p_quantity_set_unit(PQuantity *quantity, guint unit)
{
    quantity->unit = unit;
    gtk_label_set_markup(quantity->label,
        quantity->unit_info[unit]->display_name);
    g_signal_handler_block(quantity->box, quantity->handler);
    gtk_spin_button_set_adjustment(quantity->box, quantity->adjustments[unit]);
    gtk_spin_button_set_digits(quantity->box,
        quantity->unit_info[unit]->precision);
    gtk_spin_button_set_value(quantity->box,
        value_with_unit(quantity->value, quantity->unit_info[unit]));
    g_signal_handler_unblock(quantity->box, quantity->handler);
}

void
p_quantity_set_value(PQuantity *quantity, gdouble value)
{
    g_return_if_fail(!(quantity->toggle && quantity->locked));

    quantity->value = value;
    gtk_spin_button_set_value(quantity->box, value_with_unit(value,
        quantity->unit_info[quantity->unit]));
}

void
p_quantity_set_value_no_notify(PQuantity *quantity, gdouble value)
{
    g_return_if_fail(!(quantity->toggle && quantity->locked));

    quantity->value = value;
    g_signal_handler_block(quantity->box, quantity->handler);
    gtk_spin_button_set_value(quantity->box, value_with_unit(value,
        quantity->unit_info[quantity->unit]));
    g_signal_handler_unblock(quantity->box, quantity->handler);
}

gdouble
p_quantity_get_value(PQuantity *quantity)
{
    return quantity->value;
}

void
p_quantity_set_locked(PQuantity *quantity, gboolean locked)
{
    g_return_if_fail(quantity->toggle);
    gtk_toggle_button_set_active(quantity->toggle, locked);
}

gboolean
p_quantity_get_locked(PQuantity *quantity)
{
    g_return_val_if_fail(quantity->toggle, FALSE);
    return quantity->locked;
}

void
p_quantity_set_inconsistent(PQuantity *quantity, gboolean inconsistent)
{
    if(inconsistent) {
        GdkColor red;
        gdk_color_parse("red", &red);
        gtk_widget_modify_text(GTK_WIDGET(quantity->box), GTK_STATE_NORMAL,
            &red);
    } else
        gtk_widget_modify_text(GTK_WIDGET(quantity->box), GTK_STATE_NORMAL,
            NULL);
}
