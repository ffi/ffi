#include <ruby.h>

void
rbffi_gc_mark_locations(VALUE *start, VALUE *end)
{
    VALUE *v = start;
    while (v < end) {
        rb_gc_mark(*v++);
    }
}
