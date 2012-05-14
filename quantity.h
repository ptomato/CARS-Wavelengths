#ifndef __P_QUANTITY_H__
#define __P_QUANTITY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define P_TYPE_QUANTITY             (p_quantity_get_type())
#define P_QUANTITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                    P_TYPE_QUANTITY, PQuantity))
#define P_QUANTITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), \
                                    P_TYPE_QUANTITY, PQuantityClass))
#define P_IS_QUANTITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    P_TYPE_QUANTITY))
#define P_IS_QUANTITY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), \
                                    P_TYPE_QUANTITY))
#define P_QUANTITY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                    P_TYPE_QUANTITY, PQuantityClass))

typedef struct _PQuantity PQuantity;
typedef struct _PQuantityClass PQuantityClass;
typedef struct _PQuantityUnitInfo PQuantityUnitInfo;

struct _PQuantity {
    GObject parent;

    GtkSpinButton *box;
    GtkLabel *label;
    GtkToggleButton *toggle;
    gdouble value;
    guint unit;
    guint num_units;
    PQuantityUnitInfo **unit_info;
    GtkAdjustment **adjustments;
    guint handler;
    gdouble locked;
};

struct _PQuantityClass {
    GObjectClass parent_class;

    void (*changed)(PQuantity *quantity);
    void (*lock_changed)(PQuantity *quantity);
};

struct _PQuantityUnitInfo {
    gchar *display_name;
    gdouble scale_factor;
    gboolean inverse;
    guint precision;
    gdouble step;
};

GType p_quantity_get_type(void) G_GNUC_CONST;
PQuantity *p_quantity_new(GtkSpinButton *box, GtkLabel *label,
    GtkToggleButton *toggle, gdouble value, gdouble min, gdouble max,
    guint num_units, const PQuantityUnitInfo *units);
void p_quantity_set_unit(PQuantity *quantity, guint unit);
void p_quantity_set_value(PQuantity *quantity, gdouble value);
void p_quantity_set_value_no_notify(PQuantity *quantity, gdouble value);
gdouble p_quantity_get_value(PQuantity *quantity);
void p_quantity_set_locked(PQuantity *quantity, gboolean locked);
gboolean p_quantity_get_locked(PQuantity *quantity);
void p_quantity_set_inconsistent(PQuantity *quantity, gboolean inconsistent);

G_END_DECLS

#endif // __P_QUANTITY_H__
