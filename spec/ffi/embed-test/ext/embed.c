/*
 * Copyright (C) 2017 Thomas Martitz <kugel@rockbox.org>
 * Copyright (C) 2008-2017, Ruby FFI project contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Ruby FFI project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <ruby.h>

const char script[] =
"require 'rubygems'\n"
"require 'ffi'\n"
"module LibWrap\n"
"  extend FFI::Library\n"
"  ffi_lib FFI::CURRENT_PROCESS\n"
"  callback :completion_function, [:string, :long, :uint8], :void\n"
"  attach_function :do_work, [:pointer, :completion_function], :int\n"
"  Callback = Proc.new do |buf_ptr, count, code|\n"
"    nil\n"
"  end\n"
"end\n"
"\n"
"LibWrap.do_work(\"test\", LibWrap::Callback)\n";

typedef void completion_function(const char *buffer, long count, unsigned char code);

static completion_function *ruby_func;

#ifdef _WIN32
  __declspec(dllexport)
#endif
int do_work(const char *buffer, completion_function *fn)
{
  /* Calling fn directly here works */
  ruby_func = fn;
  return 0;
}

static VALUE testfunc(VALUE args);

void Init_embed_test(void)
{
  VALUE mod = rb_define_module("EmbedTest");
  rb_define_module_function(mod, "testfunc", testfunc, 0);
}

static VALUE testfunc(VALUE self)
{
  int state = 0;
  VALUE ret;

  rb_eval_string_protect(script, &state);

  if (state)
  {
    VALUE e = rb_errinfo();
    ret = rb_funcall(e, rb_intern("message"), 0);
    fprintf(stderr, "exc %s\n", StringValueCStr(ret));
    rb_set_errinfo(Qnil);
    exit(1);
  }
  else
  {
    /* Calling fn here hangs, because ffi expects an initial ruby stack
     * frame. Spawn a thread to kill the process, otherwise the deadlock
     * would prevent completing the test. */
    ruby_func("hello", 5, 0);
  }
  return Qnil;
}
