# Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
# for details. All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.

# This file contains all sources (vm and tests) for the dart virtual machine.
# Unit test files need to have a '_test' suffix appended to the name.
{
  'sources': [
    'allocation.cc',
    'allocation.h',
    'allocation_test.cc',
    'assembler.cc',
    'assembler.h',
    'assembler_arm.h',
    'assembler_ia32.cc',
    'assembler_ia32.h',
    'assembler_ia32_test.cc',
    'assembler_macros.h',
    'assembler_macros_ia32.cc',
    'assembler_macros_ia32.h',
    'assembler_macros_x64.cc',
    'assembler_macros_x64.h',
    'assembler_x64.cc',
    'assembler_x64.h',
    'assembler_x64_test.cc',
    'assert_test.cc',
    'ast.cc',
    'ast.h',
    'ast_test.cc',
    'ast_printer.h',
    'ast_printer.cc',
    'ast_printer_test.cc',
    'base_isolate.h',
    'bigint_operations.cc',
    'bigint_operations.h',
    'bigint_operations_test.cc',
    'bitfield.h',
    'bitfield_test.cc',
    'bitmap.cc',
    'bitmap.h',
    'bitmap_test.cc',
    'boolfield.h',
    'boolfield_test.cc',
    'bootstrap.h',
    'bootstrap_natives.h',
    'bootstrap_natives.cc',
    'class_finalizer.h',
    'class_finalizer.cc',
    'class_finalizer_test.cc',
    'code_descriptors.cc',
    'code_descriptors.h',
    'code_descriptors_test.cc',
    'code_generator.cc',
    'code_generator.h',
    'code_generator_test.cc',
    'code_generator_arm.h',
    'code_generator_ia32.cc',
    'code_generator_ia32.h',
    'code_generator_x64.cc',
    'code_generator_x64.h',
    'code_patcher.h',
    'code_patcher_arm.cc',
    'code_patcher_ia32.cc',
    'code_patcher_ia32_test.cc',
    'code_patcher_x64.cc',
    'code_patcher_x64_test.cc',
    'compiler.h',
    'compiler.cc',
    'compiler_stats.h',
    'compiler_stats.cc',
    'compiler_test.cc',
    'constants_ia32.h',
    'constants_x64.h',
    'constants_arm.h',
    'cpu.h',
    'cpu_ia32.cc',
    'cpu_x64.cc',
    'cpu_arm.cc',
    'cpu_test.cc',
    'custom_isolate_test.cc',
    'dart.cc',
    'dart.h',
    'dart_api_impl.h',
    'dart_api_state.h',
    'dart_api_impl_test.cc',
    'dart_api_message.cc',
    'dart_api_message.h',
    'dart_entry.cc',
    'dart_entry.h',
    'dart_entry_test.cc',
    'debugger.cc',
    'debugger.h',
    'debugger_ia32.cc',
    'debugger_x64.cc',
    'debugger_arm.cc',
    'debugger_api_impl_test.cc',
    'disassembler.cc',
    'disassembler.h',
    'disassembler_ia32.cc',
    'disassembler_x64.cc',
    'disassembler_arm.cc',
    'disassembler_test.cc',
    'debuginfo.h',
    'debuginfo_linux.cc',
    'debuginfo_macos.cc',
    'debuginfo_win.cc',
    'double_conversion.cc',
    'double_conversion.h',
    'exceptions.cc',
    'exceptions.h',
    'exceptions_test.cc',
    'find_code_object_test.cc',
    'flags.cc',
    'flags.h',
    'flags_test.cc',
    'flow_graph_builder.cc',
    'flow_graph_builder.h',
    'flow_graph_compiler.h',
    'flow_graph_compiler_arm.cc',
    'flow_graph_compiler_arm.h',
    'flow_graph_compiler_ia32.cc',
    'flow_graph_compiler_ia32.h',
    'flow_graph_compiler_x64.cc',
    'flow_graph_compiler_x64.h',
    'freelist.cc',
    'freelist.h',
    'freelist_test.cc',
    'gc_marker.cc',
    'gc_marker.h',
    'gc_sweeper.cc',
    'gc_sweeper.h',
    'gdbjit_linux.cc',
    'gdbjit_linux.h',
    'globals.h',
    'growable_array.h',
    'growable_array_test.cc',
    'handles.cc',
    'handles.h',
    'handles_impl.h',
    'handles_test.cc',
    'heap.cc',
    'heap.h',
    'heap_test.cc',
    'instructions.h',
    'instructions_ia32.cc',
    'instructions_ia32.h',
    'instructions_ia32_test.cc',
    'instructions_x64.cc',
    'instructions_x64.h',
    'instructions_x64_test.cc',
    'intermediate_language.cc',
    'intermediate_language.h',
    'intermediate_language_test.cc',
    'intrinsifier.h',
    'intrinsifier_arm.cc',
    'intrinsifier_ia32.cc',
    'intrinsifier_x64.cc',
    'isolate.cc',
    'isolate.h',
    'isolate_test.cc',
    'json.cc',
    'json.h',
    'json_test.cc',
    'longjump.cc',
    'longjump.h',
    'longjump_test.cc',
    'memory_region.cc',
    'memory_region.h',
    'memory_region_test.cc',
    'message.cc',
    'message.h',
    'message_test.cc',
    'native_arguments.cc',
    'native_arguments.h',
    'native_entry.cc',
    'native_entry.h',
    'native_entry_test.cc',
    'native_entry_test.h',
    'native_message_handler.cc',
    'native_message_handler.h',
    'object.cc',
    'object.h',
    'object_test.cc',
    'object_arm_test.cc',
    'object_ia32_test.cc',
    'object_x64_test.cc',
    'object_store.cc',
    'object_store.h',
    'object_store_test.cc',
    'opt_code_generator.h',
    'opt_code_generator_arm.h',
    'opt_code_generator_ia32.h',
    'opt_code_generator_ia32.cc',
    'opt_code_generator_x64.h',
    'os_linux.cc',
    'os_macos.cc',
    'os_win.cc',
    'os.h',
    'os_test.cc',
    'parser.cc',
    'parser.h',
    'parser_test.cc',
    'pages.cc',
    'pages.h',
    'pages_test.cc',
    'port.cc',
    'port.h',
    'port_test.cc',
    'random.cc',
    'random.h',
    'random_test.cc',
    'raw_object.cc',
    'raw_object.h',
    'raw_object_snapshot.cc',
    'resolver.cc',
    'resolver.h',
    'resolver_test.cc',
    'runtime_entry.h',
    'runtime_entry_arm.cc',
    'runtime_entry_ia32.cc',
    'runtime_entry_x64.cc',
    'runtime_entry_test.cc',
    'scanner.cc',
    'scanner.h',
    'scanner_test.cc',
    'scavenger.cc',
    'scavenger.h',
    'scopes.cc',
    'scopes.h',
    'scopes_test.cc',
    'snapshot.cc',
    'snapshot.h',
    'snapshot_test.cc',
    'stack_frame.cc',
    'stack_frame_arm.cc',
    'stack_frame_ia32.cc',
    'stack_frame_x64.cc',
    'stack_frame.h',
    'stack_frame_test.cc',
    'store_buffer.cc',
    'store_buffer.h',
    'stub_code.cc',
    'stub_code.h',
    'stub_code_arm.cc',
    'stub_code_ia32.cc',
    'stub_code_ia32_test.cc',
    'stub_code_x64.cc',
    'stub_code_x64_test.cc',
    'thread.h',
    'thread_test.cc',
    'thread_pool.cc',
    'thread_pool.h',
    'thread_pool_test.cc',
    'timer.cc',
    'timer.h',
    'token.cc',
    'token.h',
    'unicode.cc',
    'unicode.h',
    'unicode_data.cc',
    'unicode_test.cc',
    'unit_test.cc',
    'unit_test.h',
    'utils_test.cc',
    'virtual_memory.cc',
    'virtual_memory.h',
    'virtual_memory_linux.cc',
    'virtual_memory_macos.cc',
    'virtual_memory_test.cc',
    'virtual_memory_win.cc',
    'verifier.cc',
    'verifier.h',
    'visitor.h',
    'zone.cc',
    'zone.h',
    'zone_test.cc',
  ],
}
