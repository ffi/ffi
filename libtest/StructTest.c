/*
 * Copyright (c) 2007 Wayne Meissner. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef float f32;
typedef double f64;

struct test1 {
    char b;
    short s;
    int i;
    long long j;
    long l;
    float f;
    double d;
    char string[32];
};

#define T(x, type) \
    type struct_field_##type(struct test1* t) { return t->x; } \
    struct type##_align { char first; type value; }; \
    type struct_align_##type(struct type##_align* a) { return a->value; }

T(b, s8);
T(s, s16);
T(i, s32);
T(j, s64);
T(f, f32);
T(d, f64);
T(l, long);

void 
struct_set_string(struct test1* t, char* s) 
{
    strcpy(t->string, s);
}

struct test1*
struct_make_struct(char b, short s, int i, long long ll, float f, double d) 
{
    static struct test1 t;
    memset(&t, 0, sizeof(t));
    t.b = b;
    t.s = s;
    t.i = i;
    t.j = ll;
    t.f = f;
    t.d = d;
    return &t;
}

typedef int (*add_cb)(int a1, int a2);
typedef int (*sub_cb)(int a1, int a2);
struct test2 {
    add_cb  add_callback;
    sub_cb  sub_callback;
};

int
struct_call_add_cb(struct test2* t, int a1, int a2)
{
    return t->add_callback(a1, a2);
}

int
struct_call_sub_cb(struct test2* t, int a1, int a2)
{
    return t->sub_callback(a1, a2);
}
