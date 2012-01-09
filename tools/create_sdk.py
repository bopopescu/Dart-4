#!/usr/bin/env python
#
# Copyright (c) 2011, the Dart project authors.  Please see the AUTHORS file
# for details. All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.
#
# A script which will be invoked from gyp to create an SDK. 
#
# Usage: create_sdk.py sdk_directory
#
# The SDK will be used either from the command-line or from the editor. 
# Top structure is
# 
# ..dart-sdk/
# ....bin/
# ......dart or dart.exe (executable)
# ......frogc.dart
# ......frogsh (coming later)
# ....lib/
# ......builtin/
# ........builtin_runtime.dart
# ........runtime/
# ......core/
# ........core_{frog, runtime}.dart
# ........{frog, runtime}/ 
# ......coreimpl/
# ........coreimpl_{frog, runtime}.dart
# ........{frog, runtime}/ 
# ......dom/
# ........dom.dart
# ......frog/
# ......html/ 
# ........html.dart
# ......htmlimpl/ 
# ........htmlimpl.dart
# ......json/
# ........json_{frog}.dart
# ........{frog}/
# ......(more will come here - io, etc)
# ....util/
# ......(more will come here)



import os
import re
import sys
import tempfile
import utils

from os.path import dirname, join, realpath, exists, isdir
from shutil import copyfile, copymode, copytree, ignore_patterns, rmtree, move

def Main(argv):
  # Pull in all of the gpyi files which will be munged into the sdk.
  builtin_runtime_sources = \
    (eval(open("runtime/bin/builtin_sources.gypi").read()))['sources']
  corelib_sources = \
    (eval(open("corelib/src/corelib_sources.gypi").read()))['sources']
  corelib_frog_sources = \
    (eval(open("frog/lib/frog_corelib_sources.gypi").read()))['sources']
  corelib_runtime_sources = \
    (eval(open("runtime/lib/lib_sources.gypi").read()))['sources']
  coreimpl_sources = \
    (eval(open("corelib/src/implementation/corelib_impl_sources.gypi").read()))\
    ['sources']
  coreimpl_frog_sources = \
    (eval(open("frog/lib/frog_coreimpl_sources.gypi").read()))['sources']
  coreimpl_runtime_sources = \
    (eval(open("runtime/lib/lib_impl_sources.gypi").read()))['sources']
  json_frog_sources = \
    (eval(open("frog/lib/frog_json_sources.gypi").read()))['sources']

  HOME = dirname(dirname(realpath(__file__)))

  SDK_tmp = tempfile.mkdtemp()
  SDK = argv[1]

  # TODO(dgrove) - deal with architectures that are not ia32.

  if exists(SDK):
    rmtree(SDK)

  # Create and populate sdk/bin.
  BIN = join(SDK_tmp, 'bin')
  os.makedirs(BIN)

  # Copy the Dart VM binary into sdk/bin.
  # TODO(dgrove) - deal with architectures that are not ia32.
  build_dir = os.path.dirname(argv[1])
  frogc_file_extension = ''
  dart_file_extension = ''
  if utils.GuessOS() == 'win32':
    dart_file_extension = '.exe'
    frogc_file_extension = '.bat'
  dart_src_binary = join(HOME, build_dir, 'dart' + dart_file_extension)
  dart_dest_binary = join(BIN, 'dart' + dart_file_extension)
  frogc_src_binary = join(HOME, 'frog', 'frogc' + frogc_file_extension)
  frogc_dest_binary = join(BIN, 'frogc' + frogc_file_extension)
  copyfile(dart_src_binary, dart_dest_binary)
  copymode(dart_src_binary, dart_dest_binary)
  copyfile(frogc_src_binary, frogc_dest_binary)
  copymode(frogc_src_binary, frogc_dest_binary)

  # Create sdk/bin/frogc.dart, and hack as needed.
  frog_src_dir = join(HOME, 'frog')

  # Convert frogc.dart's imports from import('*') -> import('frog/*').
  frogc_contents = open(join(frog_src_dir, 'frogc.dart')).read()
  frogc_dest = open(join(BIN, 'frogc.dart'), 'w')
  frogc_dest.write(
    re.sub("#import\('", "#import('../lib/frog/", frogc_contents))
  frogc_dest.close()

  # TODO(dgrove): copy and fix up frog.dart, minfrogc.dart.

  #
  # Create and populate sdk/lib.
  #

  LIB = join(SDK_tmp, 'lib')
  os.makedirs(LIB)
  corelib_dest_dir = join(LIB, 'core')
  os.makedirs(corelib_dest_dir)
  os.makedirs(join(corelib_dest_dir, 'frog'))
  os.makedirs(join(corelib_dest_dir, 'runtime'))

  coreimpl_dest_dir = join(LIB, 'coreimpl')
  os.makedirs(coreimpl_dest_dir)
  os.makedirs(join(coreimpl_dest_dir, 'frog'))
  os.makedirs(join(coreimpl_dest_dir, 'frog', 'node'))
  os.makedirs(join(coreimpl_dest_dir, 'runtime'))


  #
  # Create and populate lib/runtime.
  #
  builtin_dest_dir = join(LIB, 'builtin')
  os.makedirs(builtin_dest_dir)
  os.makedirs(join(builtin_dest_dir, 'runtime'))
  for filename in builtin_runtime_sources:
    if filename.endswith('.dart'):
      copyfile(join(HOME, 'runtime', 'bin', filename), 
               join(builtin_dest_dir, 'runtime', filename))

  # Construct lib/builtin/builtin_runtime.dart from whole cloth.
  dest_file = open(join(builtin_dest_dir, 'builtin_runtime.dart'), 'w')
  dest_file.write('#library("dart:builtin");\n')
  for filename in builtin_runtime_sources:
    if filename.endswith('.dart'):
      dest_file.write('#source("runtime/' + filename + '");\n')
  dest_file.close()

  #
  # Create and populate lib/frog.
  #
  frog_dest_dir = join(LIB, 'frog')
  os.makedirs(frog_dest_dir)

  for filename in os.listdir(frog_src_dir):
    if filename == 'frog_options.dart':
      # change config from 'dev' to 'sdk' in frog_options.dart
      frog_options_contents = open(join(frog_src_dir, filename)).read()
      frog_options_dest = open(join(frog_dest_dir, filename), 'w')
      frog_options_dest.write(re.sub("final config = \'dev\';", 
                                     "final config = \'sdk\';", 
                                     frog_options_contents))
      frog_options_dest.close()
    elif filename.endswith('.dart'):
      copyfile(join(frog_src_dir, filename), join(frog_dest_dir, filename))

  copytree(join(frog_src_dir, 'leg'), join(frog_dest_dir, 'leg'), 
           ignore=ignore_patterns('.svn'))

  copytree(join(frog_src_dir, 'server'), join(frog_dest_dir, 'server'), 
           ignore=ignore_patterns('.svn'))

  #
  # Create and populate lib/html and lib/htmlimpl.
  #
  html_src_dir = join(HOME, 'client', 'html')
  html_dest_dir = join(LIB, 'html')
  os.makedirs(html_dest_dir)
  htmlimpl_dest_dir = join(LIB, 'htmlimpl')
  os.makedirs(htmlimpl_dest_dir)

  copyfile(join(html_src_dir, 'release', 'html.dart'), 
           join(html_dest_dir, 'html.dart'))
  copyfile(join(html_src_dir, 'release', 'htmlimpl.dart'), 
           join(htmlimpl_dest_dir, 'htmlimpl.dart'))

  # TODO(dgrove): prune the correct files in html and htmlimpl.
  for target_dir in [html_dest_dir, htmlimpl_dest_dir]:
    copytree(join(html_src_dir, 'src'), join(target_dir, 'src'),
             ignore=ignore_patterns('.svn'))
    copytree(join(html_src_dir, 'generated'), join(target_dir, 'generated'),
             ignore=ignore_patterns('.svn'))


  #
  # Create and populate lib/json.
  #
  json_dest_dir = join(LIB, 'json')
  os.makedirs(json_dest_dir)

  for filename in json_frog_sources:
    copyfile(join(HOME, 'frog', 'lib', filename), 
             join(json_dest_dir, filename))
 
  #
  # Create and populate lib/dom.
  #
  dom_src_dir = join(HOME, 'client', 'dom')
  dom_dest_dir = join(LIB, 'dom')
  os.makedirs(dom_dest_dir)

  for filename in os.listdir(dom_src_dir):
    src_file = join(dom_src_dir, filename)
    dest_file = join(dom_dest_dir, filename)
    if filename.endswith('.dart') or filename.endswith('.js'):
      copyfile(src_file, dest_file)
    elif isdir(src_file):
      if filename not in ['benchmarks', 'idl', 'scripts', 'snippets', '.svn']:
        copytree(src_file, dest_file, 
                 ignore=ignore_patterns('.svn', 'interface', 'wrapping', 
                                        '*monkey*'))

  #
  # Create and populate lib/core.
  #

  # First, copy corelib/* to lib/{frog, runtime}
  for filename in corelib_sources:
    for target_dir in ['frog', 'runtime']:
      copyfile(join('corelib', 'src', filename), 
               join(corelib_dest_dir, target_dir, filename))

  # Next, copy the frog library source on top of core/frog
  # TOOD(dgrove): move json to top-level
  for filename in corelib_frog_sources:
    copyfile(join('frog', 'lib', filename), 
             join(corelib_dest_dir, 'frog', filename))

  # Next, copy the runtime library source on top of core/runtime
  for filename in corelib_runtime_sources:
    if filename.endswith('.dart'):
      copyfile(join('runtime', 'lib', filename), 
               join(corelib_dest_dir, 'runtime', filename))
  
  #
  # At this point, it's time to create lib/core/core*dart .
  #
  # munge frog/lib/corelib.dart into lib/core_frog.dart .
  src_file = join('frog', 'lib', 'corelib.dart')
  dest_file = open(join(corelib_dest_dir, 'core_frog.dart'), 'w')
  contents = open(src_file).read()
  contents = re.sub('source\(\"../../corelib/src/', 'source(\"', contents)
  contents = re.sub("source\(\"", "source(\"frog/", contents)
  dest_file.write(contents)
  dest_file.close()

  # construct lib/core_runtime.dart from whole cloth.
  dest_file = open(join(corelib_dest_dir, 'core_runtime.dart'), 'w')
  dest_file.write('#library("dart:core");\n')
  dest_file.write('#import("dart:coreimpl");\n')
  for filename in corelib_sources:
    dest_file.write('#source("runtime/' + filename + '");\n')
  for filename in corelib_runtime_sources:
    if filename.endswith('.dart'):
      dest_file.write('#source("runtime/' + filename + '");\n')
  dest_file.close()

  #
  # Create and populate lib/coreimpl.
  #

  # First, copy corelib/src/implementation to corelib/{frog, runtime}.
  for filename in coreimpl_sources:
    for target_dir in ['frog', 'runtime']:
      copyfile(join('corelib', 'src', 'implementation', filename), 
               join(coreimpl_dest_dir, target_dir, filename))

  for filename in coreimpl_frog_sources:
    copyfile(join('frog', 'lib', filename), 
             join(coreimpl_dest_dir, 'frog', filename))

  for filename in coreimpl_runtime_sources:
    if filename.endswith('.dart'):
      copyfile(join('runtime', 'lib', filename), 
               join(coreimpl_dest_dir, 'runtime', filename))

  
  # Create and fix up lib/coreimpl/coreimpl_frog.dart .
  src_file = join('frog', 'lib', 'corelib_impl.dart')
  dest_file = open(join(coreimpl_dest_dir, 'coreimpl_frog.dart'), 'w')
  contents = open(src_file).read()
  contents = re.sub('source\(\"../../corelib/src/implementation/', 
                    'source(\"', contents)
  contents = re.sub('source\(\"', 'source(\"frog/', contents)
  dest_file.write(contents)
  dest_file.close()

  # Construct lib/coreimpl/coreimpl_runtime.dart from whole cloth.
  dest_file = open(join(coreimpl_dest_dir, 'coreimpl_runtime.dart'), 'w')
  dest_file.write('#library("dart:coreimpl");\n')
  for filename in coreimpl_sources:
    dest_file.write('#source("runtime/' + filename + '");\n')
  for filename in coreimpl_runtime_sources:
    if filename.endswith('.dart'):
      dest_file.write('#source("runtime/' + filename + '");\n')
  dest_file.close()

  # Create and copy tools.

  UTIL = join(SDK_tmp, 'util')
  os.makedirs(UTIL)

  move(SDK_tmp, SDK)

if __name__ == '__main__':
  sys.exit(Main(sys.argv))
