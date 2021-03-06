// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/class_finalizer.h"

#include "vm/flags.h"
#include "vm/heap.h"
#include "vm/isolate.h"
#include "vm/longjump.h"
#include "vm/object_store.h"
#include "vm/parser.h"

namespace dart {

DEFINE_FLAG(bool, print_classes, false, "Prints details about loaded classes.");
DEFINE_FLAG(bool, trace_class_finalization, false, "Trace class finalization.");
DEFINE_FLAG(bool, trace_type_finalization, false, "Trace type finalization.");
DEFINE_FLAG(bool, verify_implements, false,
    "Verify that all classes implement their interface.");
DECLARE_FLAG(bool, enable_type_checks);


bool ClassFinalizer::AllClassesFinalized() {
  ObjectStore* object_store = Isolate::Current()->object_store();
  const GrowableObjectArray& classes =
      GrowableObjectArray::Handle(object_store->pending_classes());
  return classes.Length() == 0;
}


// Class finalization occurs:
// a) when bootstrap process completes (VerifyBootstrapClasses).
// b) after the user classes are loaded (dart_api).
bool ClassFinalizer::FinalizePendingClasses(bool generating_snapshot) {
  bool retval = true;
  Isolate* isolate = Isolate::Current();
  ASSERT(isolate != NULL);
  ObjectStore* object_store = isolate->object_store();
  const Error& error = Error::Handle(object_store->sticky_error());
  if (!error.IsNull()) {
    return false;
  }
  LongJump* base = isolate->long_jump_base();
  LongJump jump;
  isolate->set_long_jump_base(&jump);
  if (setjmp(*jump.Set()) == 0) {
    GrowableObjectArray& class_array = GrowableObjectArray::Handle();
    class_array = object_store->pending_classes();
    ASSERT(!class_array.IsNull());
    Class& cls = Class::Handle();
    // First resolve all superclasses.
    for (intptr_t i = 0; i < class_array.Length(); i++) {
      cls ^= class_array.At(i);
      if (FLAG_trace_class_finalization) {
        OS::Print("Resolving super and default: %s\n", cls.ToCString());
      }
      ResolveSuperType(cls);
      if (cls.is_interface()) {
        ResolveFactoryClass(cls);
      }
    }
    // Finalize all classes.
    for (intptr_t i = 0; i < class_array.Length(); i++) {
      cls ^= class_array.At(i);
      FinalizeClass(cls, generating_snapshot);
    }
    if (FLAG_print_classes) {
      for (intptr_t i = 0; i < class_array.Length(); i++) {
        cls ^= class_array.At(i);
        PrintClassInformation(cls);
      }
    }
    if (FLAG_verify_implements) {
      for (intptr_t i = 0; i < class_array.Length(); i++) {
        cls ^= class_array.At(i);
        if (!cls.is_interface()) {
          VerifyClassImplements(cls);
        }
      }
    }
    // Clear pending classes array.
    class_array = GrowableObjectArray::New();
    object_store->set_pending_classes(class_array);

    // Check to ensure there are no duplicate definitions in the library
    // hierarchy.
    const String& str = String::Handle(Library::CheckForDuplicateDefinition());
    if (!str.IsNull()) {
      ReportError("Duplicate definition : %s\n", str.ToCString());
    }
  } else {
    retval = false;
  }
  isolate->set_long_jump_base(base);
  return retval;
}


// Adds all interfaces of cls into 'collected'. Duplicate entries may occur.
// No cycles are allowed.
void ClassFinalizer::CollectInterfaces(const Class& cls,
                                       const GrowableObjectArray& collected) {
  const Array& interface_array = Array::ZoneHandle(cls.interfaces());
  AbstractType& interface = AbstractType::Handle();
  Class& interface_class = Class::Handle();
  for (intptr_t i = 0; i < interface_array.Length(); i++) {
    interface ^= interface_array.At(i);
    interface_class = interface.type_class();
    collected.Add(interface_class);
    CollectInterfaces(interface_class, collected);
  }
}


#if defined (DEBUG)
// Collect all interfaces of the class 'cls' and check that every function
// defined in each interface can be found in the class.
// No need to check instance fields since they have been turned into
// getters/setters.
void ClassFinalizer::VerifyClassImplements(const Class& cls) {
  ASSERT(!cls.is_interface());
  const GrowableObjectArray& interfaces =
      GrowableObjectArray::Handle(GrowableObjectArray::New());
  CollectInterfaces(cls, interfaces);
  const String& class_name = String::Handle(cls.Name());
  Class& interface_class = Class::Handle();
  String& interface_name = String::Handle();
  Array& interface_functions = Array::Handle();
  for (int i = 0; i < interfaces.Length(); i++) {
    interface_class ^= interfaces.At(i);
    interface_name = interface_class.Name();
    interface_functions = interface_class.functions();
    for (intptr_t f = 0; f < interface_functions.Length(); f++) {
      Function& interface_function = Function::Handle();
      interface_function ^= interface_functions.At(f);
      const String& function_name = String::Handle(interface_function.name());
      // Check for constructor/factory.
      if (function_name.StartsWith(interface_name)) {
        // TODO(srdjan): convert 'InterfaceName.' to 'ClassName.' and check.
        continue;
      }
      if (interface_function.kind() == RawFunction::kConstImplicitGetter) {
        // This interface constants are not overridable.
        continue;
      }
      // Lookup function in 'cls' and all its super classes.
      Class& test_class = Class::Handle(cls.raw());
      Function& class_function =
          Function::Handle(test_class.LookupDynamicFunction(function_name));
      while (class_function.IsNull()) {
        test_class = test_class.SuperClass();
        if (test_class.IsNull()) break;
        class_function = test_class.LookupDynamicFunction(function_name);
      }
      if (class_function.IsNull()) {
        OS::PrintErr("%s implements '%s' missing: '%s'\n",
                     class_name.ToCString(),
                     interface_name.ToCString(),
                     function_name.ToCString());
      } else {
        Error& malformed_error = Error::Handle();
        if (!class_function.IsSubtypeOf(TypeArguments::Handle(),
                                             interface_function,
                                             TypeArguments::Handle(),
                                             &malformed_error)) {
          if (!malformed_error.IsNull()) {
            OS::PrintErr("%s\n", malformed_error.ToErrorCString());
          }
          OS::PrintErr("The type of instance method '%s' in class '%s' is not "
                       "a subtype of the type of '%s' in interface '%s'\n",
                       function_name.ToCString(),
                       class_name.ToCString(),
                       function_name.ToCString(),
                       interface_name.ToCString());
        }
      }
    }
  }
}
#else

void ClassFinalizer::VerifyClassImplements(const Class& cls) {}

#endif


void ClassFinalizer::VerifyBootstrapClasses() {
  if (FLAG_trace_class_finalization) {
    OS::Print("VerifyBootstrapClasses START.\n");
  }
  ObjectStore* object_store = Isolate::Current()->object_store();

  Class& cls = Class::Handle();
#if defined(DEBUG)
  // Basic checking.
  cls = object_store->object_class();
  ASSERT(Instance::InstanceSize() == cls.instance_size());
  cls = object_store->smi_class();
  ASSERT(Smi::InstanceSize() == cls.instance_size());
  cls = object_store->one_byte_string_class();
  ASSERT(OneByteString::InstanceSize() == cls.instance_size());
  cls = object_store->two_byte_string_class();
  ASSERT(TwoByteString::InstanceSize() == cls.instance_size());
  cls = object_store->four_byte_string_class();
  ASSERT(FourByteString::InstanceSize() == cls.instance_size());
  cls = object_store->external_one_byte_string_class();
  ASSERT(ExternalOneByteString::InstanceSize() == cls.instance_size());
  cls = object_store->external_two_byte_string_class();
  ASSERT(ExternalTwoByteString::InstanceSize() == cls.instance_size());
  cls = object_store->external_four_byte_string_class();
  ASSERT(ExternalFourByteString::InstanceSize() == cls.instance_size());
  cls = object_store->double_class();
  ASSERT(Double::InstanceSize() == cls.instance_size());
  cls = object_store->mint_class();
  ASSERT(Mint::InstanceSize() == cls.instance_size());
  cls = object_store->bigint_class();
  ASSERT(Bigint::InstanceSize() == cls.instance_size());
  cls = object_store->bool_class();
  ASSERT(Bool::InstanceSize() == cls.instance_size());
  cls = object_store->array_class();
  ASSERT(Array::InstanceSize() == cls.instance_size());
  cls = object_store->immutable_array_class();
  ASSERT(ImmutableArray::InstanceSize() == cls.instance_size());
  cls = object_store->uint8_array_class();
  ASSERT(Uint8Array::InstanceSize() == cls.instance_size());
  cls = object_store->int16_array_class();
  ASSERT(Int16Array::InstanceSize() == cls.instance_size());
  cls = object_store->uint16_array_class();
  ASSERT(Uint16Array::InstanceSize() == cls.instance_size());
  cls = object_store->int32_array_class();
  ASSERT(Int32Array::InstanceSize() == cls.instance_size());
  cls = object_store->uint32_array_class();
  ASSERT(Uint32Array::InstanceSize() == cls.instance_size());
  cls = object_store->int64_array_class();
  ASSERT(Int64Array::InstanceSize() == cls.instance_size());
  cls = object_store->uint64_array_class();
  ASSERT(Uint64Array::InstanceSize() == cls.instance_size());
  cls = object_store->float32_array_class();
  ASSERT(Float32Array::InstanceSize() == cls.instance_size());
  cls = object_store->float64_array_class();
  ASSERT(Float64Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_int8_array_class();
  ASSERT(ExternalInt8Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_uint8_array_class();
  ASSERT(ExternalUint8Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_int16_array_class();
  ASSERT(ExternalInt16Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_uint16_array_class();
  ASSERT(ExternalUint16Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_int32_array_class();
  ASSERT(ExternalInt32Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_uint32_array_class();
  ASSERT(ExternalUint32Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_int64_array_class();
  ASSERT(ExternalInt64Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_uint64_array_class();
  ASSERT(ExternalUint64Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_float32_array_class();
  ASSERT(ExternalFloat32Array::InstanceSize() == cls.instance_size());
  cls = object_store->external_float64_array_class();
  ASSERT(ExternalFloat64Array::InstanceSize() == cls.instance_size());
#endif  // defined(DEBUG)

  // Remember the currently pending classes.
  const GrowableObjectArray& class_array =
      GrowableObjectArray::Handle(object_store->pending_classes());
  for (intptr_t i = 0; i < class_array.Length(); i++) {
    // TODO(iposva): Add real checks.
    cls ^= class_array.At(i);
    if (cls.is_finalized() || cls.is_prefinalized()) {
      // Pre-finalized bootstrap classes must not define any fields.
      ASSERT(!cls.HasInstanceFields());
    }
  }

  // Finalize classes that aren't pre-finalized by Object::Init().
  if (!FinalizePendingClasses()) {
    // TODO(srdjan): Exit like a real VM instead.
    const Error& err = Error::Handle(object_store->sticky_error());
    OS::PrintErr("Could not verify bootstrap classes : %s\n",
                 err.ToErrorCString());
    OS::Exit(255);
  }
  if (FLAG_trace_class_finalization) {
    OS::Print("VerifyBootstrapClasses END.\n");
  }
  Isolate::Current()->heap()->Verify();
}


// Resolve unresolved_class in the library of cls, or return null.
RawClass* ClassFinalizer::ResolveClass(
    const Class& cls, const UnresolvedClass& unresolved_class) {
  const String& class_name = String::Handle(unresolved_class.ident());
  Library& lib = Library::Handle();
  Class& resolved_class = Class::Handle();
  if (unresolved_class.library_prefix() == LibraryPrefix::null()) {
    lib = cls.library();
    ASSERT(!lib.IsNull());
    resolved_class = lib.LookupClass(class_name);
  } else {
    LibraryPrefix& lib_prefix = LibraryPrefix::Handle();
    lib_prefix = unresolved_class.library_prefix();
    ASSERT(!lib_prefix.IsNull());
    resolved_class = lib_prefix.LookupLocalClass(class_name);
  }
  return resolved_class.raw();
}


// Resolve unresolved supertype (String -> Class).
void ClassFinalizer::ResolveSuperType(const Class& cls) {
  if (cls.is_finalized()) {
    return;
  }
  Type& super_type = Type::Handle(cls.super_type());
  if (super_type.IsNull()) {
    return;
  }
  // Resolve failures lead to a longjmp.
  ResolveType(cls, super_type, kFinalizeWellFormed);
  const Class& super_class = Class::Handle(super_type.type_class());
  if (cls.is_interface() != super_class.is_interface()) {
    String& class_name = String::Handle(cls.Name());
    String& super_class_name = String::Handle(super_class.Name());
    const Script& script = Script::Handle(cls.script());
    ReportError(script, cls.token_index(),
                "class '%s' and superclass '%s' are not "
                "both classes or both interfaces",
                class_name.ToCString(),
                super_class_name.ToCString());
  }
  // If cls belongs to core lib or to core lib's implementation, restrictions
  // about allowed interfaces are lifted.
  if ((cls.library() != Library::CoreLibrary()) &&
      (cls.library() != Library::CoreImplLibrary())) {
    // Prevent extending core implementation classes Bool, Double, ObjectArray,
    // ImmutableArray, GrowableObjectArray, IntegerImplementation, Smi, Mint,
    // BigInt, OneByteString, TwoByteString, FourByteString.
    ObjectStore* object_store = Isolate::Current()->object_store();
    const Library& core_impl_lib = Library::Handle(Library::CoreImplLibrary());
    const String& integer_implementation_name =
        String::Handle(String::NewSymbol("IntegerImplementation"));
    const Class& integer_implementation_class =
        Class::Handle(core_impl_lib.LookupClass(integer_implementation_name));
    const String& growable_object_array_name =
        String::Handle(String::NewSymbol("GrowableObjectArray"));
    const Class& growable_object_array_class =
        Class::Handle(core_impl_lib.LookupClass(growable_object_array_name));
    if ((super_class.raw() == object_store->bool_class()) ||
        (super_class.raw() == object_store->double_class()) ||
        (super_class.raw() == object_store->array_class()) ||
        (super_class.raw() == object_store->immutable_array_class()) ||
        (super_class.raw() == growable_object_array_class.raw()) ||
        (super_class.raw() == object_store->int8_array_class()) ||
        (super_class.raw() == object_store->uint8_array_class()) ||
        (super_class.raw() == object_store->int16_array_class()) ||
        (super_class.raw() == object_store->uint16_array_class()) ||
        (super_class.raw() == object_store->int32_array_class()) ||
        (super_class.raw() == object_store->uint32_array_class()) ||
        (super_class.raw() == object_store->int64_array_class()) ||
        (super_class.raw() == object_store->uint64_array_class()) ||
        (super_class.raw() == object_store->float32_array_class()) ||
        (super_class.raw() == object_store->float64_array_class()) ||
        (super_class.raw() == object_store->external_int8_array_class()) ||
        (super_class.raw() == object_store->external_uint8_array_class()) ||
        (super_class.raw() == object_store->external_int16_array_class()) ||
        (super_class.raw() == object_store->external_uint16_array_class()) ||
        (super_class.raw() == object_store->external_int32_array_class()) ||
        (super_class.raw() == object_store->external_uint32_array_class()) ||
        (super_class.raw() == object_store->external_int64_array_class()) ||
        (super_class.raw() == object_store->external_uint64_array_class()) ||
        (super_class.raw() == object_store->external_float32_array_class()) ||
        (super_class.raw() == object_store->external_float64_array_class()) ||
        (super_class.raw() == integer_implementation_class.raw()) ||
        (super_class.raw() == object_store->smi_class()) ||
        (super_class.raw() == object_store->mint_class()) ||
        (super_class.raw() == object_store->bigint_class()) ||
        (super_class.raw() == object_store->one_byte_string_class()) ||
        (super_class.raw() == object_store->two_byte_string_class()) ||
        (super_class.raw() == object_store->four_byte_string_class())) {
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "'%s' is not allowed to extend '%s'",
                  String::Handle(cls.Name()).ToCString(),
                  String::Handle(super_class.Name()).ToCString());
    }
  }
  return;
}


void ClassFinalizer::ResolveFactoryClass(const Class& interface) {
  ASSERT(interface.is_interface());
  if (interface.is_finalized() ||
      !interface.HasFactoryClass() ||
      interface.HasResolvedFactoryClass()) {
    return;
  }
  const UnresolvedClass& unresolved_factory_class =
      UnresolvedClass::Handle(interface.UnresolvedFactoryClass());

  // Lookup the factory class.
  const Class& factory_class =
      Class::Handle(ResolveClass(interface, unresolved_factory_class));
  if (factory_class.IsNull()) {
    const Script& script = Script::Handle(interface.script());
    ReportError(script, unresolved_factory_class.token_index(),
                "cannot resolve factory class name '%s' from '%s'",
                String::Handle(unresolved_factory_class.Name()).ToCString(),
                String::Handle(interface.Name()).ToCString());
  }
  if (factory_class.is_interface()) {
    const String& interface_name = String::Handle(interface.Name());
    const String& factory_name = String::Handle(factory_class.Name());
    const Script& script = Script::Handle(interface.script());
    ReportError(script, unresolved_factory_class.token_index(),
                "default clause of interface '%s' names non-class '%s'",
                interface_name.ToCString(),
                factory_name.ToCString());
  }
  interface.set_factory_class(factory_class);
  // It is not necessary to finalize the bounds before comparing them between
  // the expected and actual factory class.
  const Class& factory_signature_class = Class::Handle(
      unresolved_factory_class.factory_signature_class());
  ASSERT(!factory_signature_class.IsNull());
  // If a type parameter list is included in the default factory clause (it
  // can be omitted), verify that it matches the list of type parameters of
  // the factory class in number, names, and bounds.
  if (factory_signature_class.NumTypeParameters() > 0) {
    const TypeArguments& expected_type_parameters =
        TypeArguments::Handle(factory_signature_class.type_parameters());
    const TypeArguments& actual_type_parameters =
        TypeArguments::Handle(factory_class.type_parameters());
    const TypeArguments& expected_type_parameter_bounds =
        TypeArguments::Handle(factory_signature_class.type_parameter_bounds());
    const TypeArguments& actual_type_parameter_bounds =
        TypeArguments::Handle(factory_class.type_parameter_bounds());
    if (!AbstractTypeArguments::AreIdentical(expected_type_parameters,
                                             actual_type_parameters) ||
        !AbstractTypeArguments::AreIdentical(expected_type_parameter_bounds,
                                             actual_type_parameter_bounds)) {
      const String& interface_name = String::Handle(interface.Name());
      const String& factory_name = String::Handle(factory_class.Name());
      const Script& script = Script::Handle(interface.script());
      ReportError(script, unresolved_factory_class.token_index(),
                  "mismatch in number, names, or bounds of type parameters "
                  "between default clause of interface '%s' and actual factory "
                  "class '%s'",
                  interface_name.ToCString(),
                  factory_name.ToCString());
    }
  }
  // Verify that the type parameters of the factory class and of the interface
  // have identical names.
  const TypeArguments& interface_type_parameters =
      TypeArguments::Handle(interface.type_parameters());
  const TypeArguments& factory_type_parameters =
      TypeArguments::Handle(factory_class.type_parameters());
  if (!AbstractTypeArguments::AreIdentical(interface_type_parameters,
                                           factory_type_parameters)) {
    const String& interface_name = String::Handle(interface.Name());
    const String& factory_name = String::Handle(factory_class.Name());
    const Script& script = Script::Handle(interface.script());
    ReportError(script, unresolved_factory_class.token_index(),
                "mismatch in number or names of type parameters between "
                "interface '%s' and default factory class '%s'",
                interface_name.ToCString(),
                factory_name.ToCString());
  }
}


void ClassFinalizer::ResolveType(const Class& cls,
                                 const AbstractType& type,
                                 FinalizationKind finalization) {
  if (type.IsResolved() || type.IsFinalized()) {
    return;
  }
  if (FLAG_trace_type_finalization) {
    OS::Print("Resolve type '%s'\n", String::Handle(type.Name()).ToCString());
  }

  // Resolve the type class.
  if (!type.HasResolvedTypeClass()) {
    // Type parameters are always resolved in the parser in the correct
    // non-static scope or factory scope. That resolution scope is unknown here.
    // Being able to resolve a type parameter from class cls here would indicate
    // that the type parameter appeared in a static scope. Leaving the type as
    // unresolved is the correct thing to do.

    // Lookup the type class.
    const UnresolvedClass& unresolved_class =
        UnresolvedClass::Handle(type.unresolved_class());
    const Class& type_class =
        Class::Handle(ResolveClass(cls, unresolved_class));

    // Replace unresolved class with resolved type class.
    ASSERT(type.IsType());
    Type& parameterized_type = Type::Handle();
    parameterized_type ^= type.raw();
    if (!type_class.IsNull()) {
      parameterized_type.set_type_class(Object::Handle(type_class.raw()));
    } else {
      // The type class could not be resolved. The type is malformed.
      FinalizeMalformedType(Error::Handle(),  // No previous error.
                            cls, parameterized_type, finalization,
                            "cannot resolve class name '%s' from '%s'",
                            String::Handle(unresolved_class.Name()).ToCString(),
                            String::Handle(cls.Name()).ToCString());
      return;
    }
  }

  // Resolve type arguments, if any.
  const AbstractTypeArguments& arguments =
      AbstractTypeArguments::Handle(type.arguments());
  if (!arguments.IsNull()) {
    intptr_t num_arguments = arguments.Length();
    AbstractType& type_argument = AbstractType::Handle();
    for (intptr_t i = 0; i < num_arguments; i++) {
      type_argument = arguments.TypeAt(i);
      ResolveType(cls, type_argument, finalization);
    }
  }
}


void ClassFinalizer::FinalizeTypeParameters(const Class& cls) {
  const TypeArguments& type_parameters =
      TypeArguments::Handle(cls.type_parameters());
  if (!type_parameters.IsNull()) {
    TypeParameter& type_parameter = TypeParameter::Handle();
    const intptr_t num_types = type_parameters.Length();
    for (intptr_t i = 0; i < num_types; i++) {
      type_parameter ^= type_parameters.TypeAt(i);
      type_parameter ^= FinalizeType(cls, type_parameter, kFinalizeWellFormed);
      type_parameters.SetTypeAt(i, type_parameter);
    }
  }
}


// Finalize the type argument vector 'arguments' of the type defined by the
// class 'cls' parameterized with the type arguments 'cls_args'.
// The vector 'cls_args' is already initialized as a subvector at the correct
// position in the passed in 'arguments' vector.
// The subvector 'cls_args' has length cls.NumTypeParameters() and starts at
// offset cls.NumTypeArguments() - cls.NumTypeParameters() of the 'arguments'
// vector.
// Example:
//   Declared: class C<K, V> extends B<V> { ... }
//             class B<T> extends A<int> { ... }
//   Input:    C<String, double> expressed as
//             cls = C, arguments = [null, null, String, double],
//             i.e. cls_args = [String, double], offset = 2, length = 2.
//   Output:   arguments = [int, double, String, double]
void ClassFinalizer::FinalizeTypeArguments(
    const Class& cls,
    const AbstractTypeArguments& arguments,
    FinalizationKind finalization) {
  ASSERT(arguments.Length() >= cls.NumTypeArguments());
  if (!cls.is_finalized()) {
    GrowableArray<intptr_t> visited_interfaces;
    ResolveInterfaces(cls, &visited_interfaces);
    FinalizeTypeParameters(cls);
  }
  Type& super_type = Type::Handle(cls.super_type());
  if (!super_type.IsNull()) {
    const Class& super_class = Class::Handle(super_type.type_class());
    AbstractTypeArguments& super_type_args = AbstractTypeArguments::Handle();
    if (super_type.IsBeingFinalized()) {
      // This type references itself via its type arguments. This is legal, but
      // we must avoid endless recursion. We therefore map the innermost
      // super type to Dynamic.
      // Note that a direct self-reference via the super class chain is illegal
      // and reported as an error earlier.
      // Such legal self-references occur with F-bounded quantification.
      // Example 1: class Derived extends Base<Derived>.
      // The type 'Derived' forms a cycle by pointing to itself via its
      // flattened type argument vector: Derived[Base[Derived[Base[...]]]]
      // We break the cycle as follows: Derived[Base[Derived[Dynamic]]]
      // Example 2: class Derived extends Base<Middle<Derived>> results in
      // Derived[Base[Middle[Derived[Dynamic]]]]
      // Example 3: class Derived<T> extends Base<Derived<T>> results in
      // Derived[Base[Derived[Dynamic]], T].
      ASSERT(super_type_args.IsNull());  // Same as a vector of Dynamic.
    } else {
      super_type ^= FinalizeType(cls, super_type, finalization);
      cls.set_super_type(super_type);
      super_type_args = super_type.arguments();
    }
    const intptr_t num_super_type_params = super_class.NumTypeParameters();
    const intptr_t offset = super_class.NumTypeArguments();
    const intptr_t super_offset = offset - num_super_type_params;
    ASSERT(offset == (cls.NumTypeArguments() - cls.NumTypeParameters()));
    AbstractType& super_type_arg = AbstractType::Handle(Type::DynamicType());
    for (intptr_t i = 0; i < num_super_type_params; i++) {
      if (!super_type_args.IsNull()) {
        super_type_arg = super_type_args.TypeAt(super_offset + i);
        if (!super_type_arg.IsInstantiated()) {
          super_type_arg = super_type_arg.InstantiateFrom(arguments);
        }
        super_type_arg = super_type_arg.Canonicalize();
      }
      arguments.SetTypeAt(super_offset + i, super_type_arg);
    }
    FinalizeTypeArguments(super_class, arguments, finalization);
  }
}


RawAbstractType* ClassFinalizer::FinalizeType(const Class& cls,
                                              const AbstractType& type,
                                              FinalizationKind finalization) {
  if (type.IsFinalized()) {
    return type.raw();
  }
  ASSERT(type.IsResolved());
  ASSERT((finalization == kFinalize) || (finalization == kFinalizeWellFormed));

  if (FLAG_trace_type_finalization) {
    OS::Print("Finalize type '%s'\n", String::Handle(type.Name()).ToCString());
  }

  if (type.IsTypeParameter()) {
    TypeParameter& type_parameter = TypeParameter::Handle();
    type_parameter ^= type.raw();
    const Class& parameterized_class =
        Class::Handle(type_parameter.parameterized_class());
    ASSERT(!parameterized_class.IsNull());
    // The index must reflect the position of this type parameter in the type
    // arguments vector of its parameterized class. The offset to add is the
    // number of type arguments in the super type, which is equal to the
    // difference in number of type arguments and type parameters of the
    // parameterized class.
    const intptr_t offset = parameterized_class.NumTypeArguments() -
                            parameterized_class.NumTypeParameters();
    type_parameter.set_index(type_parameter.Index() + offset);
    type_parameter.set_is_finalized();
    // We do not canonicalize type parameters.
    return type_parameter.raw();
  }

  // At this point, we can only have a parameterized_type.
  Type& parameterized_type = Type::Handle();
  parameterized_type ^= type.raw();

  if (parameterized_type.IsBeingFinalized()) {
    // Self reference detected. The type is malformed.
    FinalizeMalformedType(
        Error::Handle(),  // No previous error.
        cls, parameterized_type, finalization,
        "type '%s' illegally refers to itself",
        String::Handle(parameterized_type.Name()).ToCString());
    return parameterized_type.raw();
  }

  // Mark type as being finalized in order to detect illegal self reference.
  parameterized_type.set_is_being_finalized();

  // The type class does not need to be finalized in order to finalize the type,
  // however, it must at least be resolved (this was done as part of resolving
  // the type itself, a precondition to calling FinalizeType).
  // Also, the interfaces of the type class must be resolved and the type
  // parameters of the type class must be finalized.
  Class& type_class = Class::Handle(parameterized_type.type_class());
  if (!type_class.is_finalized()) {
    GrowableArray<intptr_t> visited_interfaces;
    ResolveInterfaces(type_class, &visited_interfaces);
    FinalizeTypeParameters(type_class);
  }

  // Finalize the current type arguments of the type, which are still the
  // parsed type arguments.
  AbstractTypeArguments& arguments =
      AbstractTypeArguments::Handle(parameterized_type.arguments());
  if (!arguments.IsNull()) {
    intptr_t num_arguments = arguments.Length();
    AbstractType& type_argument = AbstractType::Handle();
    for (intptr_t i = 0; i < num_arguments; i++) {
      type_argument = arguments.TypeAt(i);
      type_argument = FinalizeType(cls, type_argument, finalization);
      if (type_argument.IsMalformed()) {
        // In production mode, malformed type arguments are mapped to Dynamic.
        // In checked mode, a type with malformed type arguments is malformed.
        if (FLAG_enable_type_checks) {
          const Error& error = Error::Handle(type_argument.malformed_error());
          const String& type_name = String::Handle(parameterized_type.Name());
          FinalizeMalformedType(error, cls, parameterized_type, finalization,
                                "type '%s' has malformed type argument",
                                type_name.ToCString());
          return parameterized_type.raw();
        } else {
          type_argument = Type::DynamicType();
        }
      }
      arguments.SetTypeAt(i, type_argument);
    }
  }

  // The finalized type argument vector needs num_type_arguments types.
  const intptr_t num_type_arguments = type_class.NumTypeArguments();
  // The type class has num_type_parameters type parameters.
  const intptr_t num_type_parameters = type_class.NumTypeParameters();

  // Initialize the type argument vector.
  // Check the number of parsed type arguments, if any.
  // Specifying no type arguments indicates a raw type, which is not an error.
  // However, type parameter bounds are checked below, even for a raw type.
  if (!arguments.IsNull() && (arguments.Length() != num_type_parameters)) {
    // Wrong number of type arguments. The type is malformed.
    FinalizeMalformedType(
        Error::Handle(),  // No previous error.
        cls, parameterized_type, finalization,
        "wrong number of type arguments in type '%s'",
        String::Handle(parameterized_type.Name()).ToCString());
    return parameterized_type.raw();
  }
  // The full type argument vector consists of the type arguments of the
  // super types of type_class, which may be initialized from the parsed
  // type arguments, followed by the parsed type arguments.
  TypeArguments& full_arguments = TypeArguments::Handle();
  if (num_type_arguments > 0) {
    // If no type arguments were parsed and if the super types do not prepend
    // type arguments to the vector, we can leave the vector as null.
    if (!arguments.IsNull() || (num_type_arguments > num_type_parameters)) {
      full_arguments = TypeArguments::New(num_type_arguments);
      // Copy the parsed type arguments at the correct offset in the full type
      // argument vector.
      const intptr_t offset = num_type_arguments - num_type_parameters;
      AbstractType& type_arg = AbstractType::Handle(Type::DynamicType());
      for (intptr_t i = 0; i < num_type_parameters; i++) {
        // If no type parameters were provided, a raw type is desired, so we
        // create a vector of DynamicType.
        if (!arguments.IsNull()) {
          type_arg = arguments.TypeAt(i);
        }
        ASSERT(type_arg.IsFinalized());  // Index of type parameter is adjusted.
        full_arguments.SetTypeAt(offset + i, type_arg);
      }
      if (type_class.IsSignatureClass()) {
        const Function& signature_fun =
            Function::Handle(type_class.signature_function());
        ASSERT(!signature_fun.is_static());
        const Class& sig_fun_owner = Class::Handle(signature_fun.owner());
        FinalizeTypeArguments(sig_fun_owner, full_arguments, finalization);
      } else {
        FinalizeTypeArguments(type_class, full_arguments, finalization);
      }
      if (full_arguments.IsRaw(num_type_arguments)) {
        // The parameterized_type is raw. Set its argument vector to null, which
        // is more efficient in type tests.
        full_arguments = TypeArguments::null();
      } else {
        // FinalizeTypeArguments can modify 'full_arguments',
        // canonicalize afterwards.
        full_arguments ^= full_arguments.Canonicalize();
      }
      parameterized_type.set_arguments(full_arguments);
    } else {
      ASSERT(full_arguments.IsNull());  // Use null vector for raw type.
    }
  }

  // Self referencing types may get finalized indirectly.
  if (!parameterized_type.IsFinalized()) {
    // Mark the type as finalized.
    if (parameterized_type.IsInstantiated()) {
      parameterized_type.set_is_finalized_instantiated();
    } else {
      parameterized_type.set_is_finalized_uninstantiated();
    }
  }

  // Upper bounds of the finalized type arguments are only verified in checked
  // mode, since bound errors are never reported by the vm in production mode.
  if (FLAG_enable_type_checks &&
      !full_arguments.IsNull() &&
      full_arguments.IsInstantiated()) {
    ResolveAndFinalizeUpperBounds(type_class);
    Error& malformed_error = Error::Handle();
    // Pass the full type argument vector as the bounds instantiator.
    if (!full_arguments.IsWithinBoundsOf(type_class,
                                         full_arguments,
                                         &malformed_error)) {
      ASSERT(!malformed_error.IsNull());
      // The type argument vector of the type is not within bounds. The type
      // is malformed. Prepend malformed_error to new malformed type error in
      // order to report both locations.
      // Note that malformed bounds never result in a compile time error, even
      // in checked mode. Therefore, overwrite finalization with kFinalize
      // when finalizing the malformed type.
      FinalizeMalformedType(
          malformed_error,
          cls, parameterized_type, kFinalize,
          "type arguments of type '%s' are not within bounds",
          String::Handle(parameterized_type.Name()).ToCString());
      return parameterized_type.raw();
    }
  }

  // If the type class is a signature class, we are currently finalizing a
  // signature type, i.e. finalizing the result type and parameter types of the
  // signature function of this signature type.
  // We do this after marking this type as finalized in order to allow a
  // function type to refer to itself via its parameter types and result type.
  if (type_class.IsSignatureClass()) {
    // Signature classes are finalized upon creation, except function type
    // aliases.
    if (type_class.IsCanonicalSignatureClass()) {
      ASSERT(type_class.is_finalized());
      // Resolve and finalize the result and parameter types of the signature
      // function of this signature class.
      ASSERT(type_class.SignatureType() == type.raw());
      ResolveAndFinalizeSignature(
          type_class, Function::Handle(type_class.signature_function()));
    } else {
      // This type is a function type alias. Its class may need to be finalized
      // and checked for illegal self reference.
      FinalizeClass(type_class, false);
      // Finalizing the signature function here (as in the canonical case above)
      // would not mark the canonical signature type as finalized.
      const Type& signature_type = Type::Handle(type_class.SignatureType());
      FinalizeType(cls, signature_type, finalization);
    }
  }

  return parameterized_type.Canonicalize();
}


void ClassFinalizer::ResolveAndFinalizeSignature(const Class& cls,
                                                 const Function& function) {
  // Resolve result type.
  AbstractType& type = AbstractType::Handle(function.result_type());
  // In case of a factory, the parser sets the factory result type to a type
  // with an unresolved class whose name matches the factory name.
  // It is not a compile time error if this name does not resolve to a class or
  // interface.
  ResolveType(cls, type, kFinalize);
  type = FinalizeType(cls, type, kFinalize);
  // In production mode, a malformed result type is mapped to Dynamic.
  if (!FLAG_enable_type_checks && type.IsMalformed()) {
    type = Type::DynamicType();
  }
  function.set_result_type(type);
  // Resolve formal parameter types.
  const intptr_t num_parameters = function.NumberOfParameters();
  for (intptr_t i = 0; i < num_parameters; i++) {
    type = function.ParameterTypeAt(i);
    ResolveType(cls, type, kFinalize);
    type = FinalizeType(cls, type, kFinalize);
    // In production mode, a malformed parameter type is mapped to Dynamic.
    if (!FLAG_enable_type_checks && type.IsMalformed()) {
      type = Type::DynamicType();
    }
    function.SetParameterTypeAt(i, type);
  }
}


static RawClass* FindSuperOwnerOfInstanceMember(const Class& cls,
                                                const String& name) {
  Class& super_class = Class::Handle();
  Function& function = Function::Handle();
  Field& field = Field::Handle();
  super_class = cls.SuperClass();
  while (!super_class.IsNull()) {
    // Check if an instance member of same name exists in any super class.
    function = super_class.LookupFunction(name);
    if (!function.IsNull() && !function.is_static()) {
      return super_class.raw();
    }
    field = super_class.LookupField(name);
    if (!field.IsNull() && !field.is_static()) {
      return super_class.raw();
    }
    super_class = super_class.SuperClass();
  }
  return Class::null();
}


static RawClass* FindSuperOwnerOfFunction(const Class& cls,
                                          const String& name) {
  Class& super_class = Class::Handle();
  Function& function = Function::Handle();
  super_class = cls.SuperClass();
  while (!super_class.IsNull()) {
    // Check if a function of same name exists in any super class.
    function = super_class.LookupFunction(name);
    if (!function.IsNull()) {
      return super_class.raw();
    }
    super_class = super_class.SuperClass();
  }
  return Class::null();
}


// Resolve and finalize the upper bounds of the type parameters of class cls.
void ClassFinalizer::ResolveAndFinalizeUpperBounds(const Class& cls) {
  const intptr_t num_type_params = cls.NumTypeParameters();
  AbstractType& bound = AbstractType::Handle();
  const AbstractTypeArguments& bounds =
      AbstractTypeArguments::Handle(cls.type_parameter_bounds());
  ASSERT((bounds.IsNull() && (num_type_params == 0)) ||
         (bounds.Length() == num_type_params));
  for (intptr_t i = 0; i < num_type_params; i++) {
    bound = bounds.TypeAt(i);
    if (bound.IsFinalized()) {
      continue;
    }
    ResolveType(cls, bound, kFinalize);
    bound = FinalizeType(cls, bound, kFinalize);
    bounds.SetTypeAt(i, bound);
  }
}


void ClassFinalizer::ResolveAndFinalizeMemberTypes(const Class& cls) {
  // Note that getters and setters are explicitly listed as such in the list of
  // functions of a class, so we do not need to consider fields as implicitly
  // generating getters and setters.
  // The only compile errors we report are therefore:
  // - a getter having the same name as a method (but not a getter) in a super
  //   class or in a subclass.
  // - a setter having the same name as a method (but not a setter) in a super
  //   class or in a subclass.
  // - a static field, instance field, or static method (but not an instance
  //   method) having the same name as an instance member in a super class.

  // Resolve type of fields and check for conflicts in super classes.
  Array& array = Array::Handle(cls.fields());
  Field& field = Field::Handle();
  AbstractType& type = AbstractType::Handle();
  String& name = String::Handle();
  Class& super_class = Class::Handle();
  intptr_t num_fields = array.Length();
  for (intptr_t i = 0; i < num_fields; i++) {
    field ^= array.At(i);
    type = field.type();
    ResolveType(cls, type, kFinalize);
    type = FinalizeType(cls, type, kFinalize);
    field.set_type(type);
    name = field.name();
    super_class = FindSuperOwnerOfInstanceMember(cls, name);
    if (!super_class.IsNull()) {
      const String& class_name = String::Handle(cls.Name());
      const String& super_class_name = String::Handle(super_class.Name());
      const Script& script = Script::Handle(cls.script());
      ReportError(script, field.token_index(),
                  "field '%s' of class '%s' conflicts with instance "
                  "member '%s' of super class '%s'",
                  name.ToCString(),
                  class_name.ToCString(),
                  name.ToCString(),
                  super_class_name.ToCString());
    }
  }
  // Collect interfaces, super interfaces, and super classes of this class.
  const GrowableObjectArray& interfaces =
      GrowableObjectArray::Handle(GrowableObjectArray::New());
  CollectInterfaces(cls, interfaces);
  // Include superclasses in list of interfaces and super interfaces.
  super_class = cls.SuperClass();
  while (!super_class.IsNull()) {
    interfaces.Add(super_class);
    super_class = super_class.SuperClass();
  }
  // Resolve function signatures and check for conflicts in super classes and
  // interfaces.
  array = cls.functions();
  Function& function = Function::Handle();
  Function& overridden_function = Function::Handle();
  intptr_t num_functions = array.Length();
  String& function_name = String::Handle();
  for (intptr_t i = 0; i < num_functions; i++) {
    function ^= array.At(i);
    ResolveAndFinalizeSignature(cls, function);
    function_name = function.name();
    if (function.is_static()) {
      super_class = FindSuperOwnerOfInstanceMember(cls, function_name);
      if (!super_class.IsNull()) {
        const String& class_name = String::Handle(cls.Name());
        const String& super_class_name = String::Handle(super_class.Name());
        const Script& script = Script::Handle(cls.script());
        ReportError(script, function.token_index(),
                    "static function '%s' of class '%s' conflicts with "
                    "instance member '%s' of super class '%s'",
                    function_name.ToCString(),
                    class_name.ToCString(),
                    function_name.ToCString(),
                    super_class_name.ToCString());
      }
    } else {
      for (int i = 0; i < interfaces.Length(); i++) {
        super_class ^= interfaces.At(i);
        overridden_function = super_class.LookupDynamicFunction(function_name);
        if (!overridden_function.IsNull() &&
            !function.HasCompatibleParametersWith(overridden_function)) {
          // Function types are purposely not checked for subtyping.
          const String& class_name = String::Handle(cls.Name());
          const String& super_class_name = String::Handle(super_class.Name());
          const Script& script = Script::Handle(cls.script());
          ReportError(script, function.token_index(),
                      "class '%s' overrides function '%s' of %s '%s' "
                      "with incompatible parameters",
                      class_name.ToCString(),
                      function_name.ToCString(),
                      super_class.is_interface() ? "interface" : "super class",
                      super_class_name.ToCString());
        }
      }
    }
    if (function.kind() == RawFunction::kGetterFunction) {
      name = Field::NameFromGetter(function_name);
      super_class = FindSuperOwnerOfFunction(cls, name);
      if (!super_class.IsNull()) {
        const String& class_name = String::Handle(cls.Name());
        const String& super_class_name = String::Handle(super_class.Name());
        const Script& script = Script::Handle(cls.script());
        ReportError(script, function.token_index(),
                    "getter '%s' of class '%s' conflicts with "
                    "function '%s' of super class '%s'",
                    name.ToCString(),
                    class_name.ToCString(),
                    name.ToCString(),
                    super_class_name.ToCString());
      }
    } else if (function.kind() == RawFunction::kSetterFunction) {
      name = Field::NameFromSetter(function_name);
      super_class = FindSuperOwnerOfFunction(cls, name);
      if (!super_class.IsNull()) {
        const String& class_name = String::Handle(cls.Name());
        const String& super_class_name = String::Handle(super_class.Name());
        const Script& script = Script::Handle(cls.script());
        ReportError(script, function.token_index(),
                    "setter '%s' of class '%s' conflicts with "
                    "function '%s' of super class '%s'",
                    name.ToCString(),
                    class_name.ToCString(),
                    name.ToCString(),
                    super_class_name.ToCString());
      }
    } else {
      name = Field::GetterName(function_name);
      super_class = FindSuperOwnerOfFunction(cls, name);
      if (!super_class.IsNull()) {
        const String& class_name = String::Handle(cls.Name());
        const String& super_class_name = String::Handle(super_class.Name());
        const Script& script = Script::Handle(cls.script());
        ReportError(script, function.token_index(),
                    "function '%s' of class '%s' conflicts with "
                    "getter '%s' of super class '%s'",
                    function_name.ToCString(),
                    class_name.ToCString(),
                    function_name.ToCString(),
                    super_class_name.ToCString());
      }
      name = Field::SetterName(function_name);
      super_class = FindSuperOwnerOfFunction(cls, name);
      if (!super_class.IsNull()) {
        const String& class_name = String::Handle(cls.Name());
        const String& super_class_name = String::Handle(super_class.Name());
        const Script& script = Script::Handle(cls.script());
        ReportError(script, function.token_index(),
                    "function '%s' of class '%s' conflicts with "
                    "setter '%s' of super class '%s'",
                    function_name.ToCString(),
                    class_name.ToCString(),
                    function_name.ToCString(),
                    super_class_name.ToCString());
      }
    }
  }
}


void ClassFinalizer::FinalizeClass(const Class& cls, bool generating_snapshot) {
  if (cls.is_finalized()) {
    return;
  }
  if (FLAG_trace_class_finalization) {
    OS::Print("Finalize %s\n", cls.ToCString());
  }
  if (!IsSuperCycleFree(cls)) {
    const String& name = String::Handle(cls.Name());
    const Script& script = Script::Handle(cls.script());
    ReportError(script, cls.token_index(),
                "class '%s' has a cycle in its superclass relationship",
                name.ToCString());
  }
  GrowableArray<intptr_t> visited_interfaces;
  ResolveInterfaces(cls, &visited_interfaces);
  // Finalize super class.
  const Class& super_class = Class::Handle(cls.SuperClass());
  if (!super_class.IsNull()) {
    FinalizeClass(super_class, generating_snapshot);
  }
  // Finalize type parameters before finalizing the super type.
  FinalizeTypeParameters(cls);
  // Finalize super type.
  Type& super_type = Type::Handle(cls.super_type());
  if (!super_type.IsNull()) {
    super_type ^= FinalizeType(cls, super_type, kFinalizeWellFormed);
    cls.set_super_type(super_type);
  }
  // Signature classes are finalized upon creation, except function type
  // aliases.
  if (cls.IsSignatureClass()) {
    ASSERT(!cls.IsCanonicalSignatureClass());
    // Check for illegal self references.
    GrowableArray<intptr_t> visited_aliases;
    if (!IsAliasCycleFree(cls, &visited_aliases)) {
      const String& name = String::Handle(cls.Name());
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "typedef '%s' illegally refers to itself",
                  name.ToCString());
    }
    cls.Finalize();
    return;
  }
  // Finalize factory class, if any.
  if (cls.is_interface()) {
    if (cls.HasFactoryClass()) {
      const Class& factory_class = Class::Handle(cls.FactoryClass());
      if (!factory_class.is_finalized()) {
        FinalizeClass(factory_class, generating_snapshot);
        // Finalizing the factory class may indirectly finalize this interface.
        if (cls.is_finalized()) {
          return;
        }
      }
    }
  }
  // Finalize interface types (but not necessarily interface classes).
  Array& interface_types = Array::Handle(cls.interfaces());
  AbstractType& interface_type = AbstractType::Handle();
  for (intptr_t i = 0; i < interface_types.Length(); i++) {
    interface_type ^= interface_types.At(i);
    interface_type = FinalizeType(cls, interface_type, kFinalizeWellFormed);
    interface_types.SetAt(i, interface_type);
  }
  // Mark as finalized before resolving type parameter upper bounds and member
  // types in order to break cycles.
  cls.Finalize();
  ResolveAndFinalizeUpperBounds(cls);
  ResolveAndFinalizeMemberTypes(cls);
  // Run additional checks after all types are finalized.
  if (cls.is_const()) {
    CheckForLegalConstClass(cls);
  }
  // Check to ensure we don't have classes with native fields in libraries
  // which do not have a native resolver.
  if (!generating_snapshot && cls.num_native_fields() != 0) {
    const Library& lib = Library::Handle(cls.library());
    if (lib.native_entry_resolver() == NULL) {
      const String& cls_name = String::Handle(cls.Name());
      const String& lib_name = String::Handle(lib.url());
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "class '%s' is trying to extend a native fields class, "
                  "but library '%s' has no native resolvers",
                  cls_name.ToCString(), lib_name.ToCString());
    }
  }
}


bool ClassFinalizer::IsSuperCycleFree(const Class& cls) {
  Class& test1 = Class::Handle(cls.raw());
  Class& test2 = Class::Handle(cls.SuperClass());
  // A finalized class has been checked for cycles.
  // Using the hare and tortoise algorithm for locating cycles.
  while (!test1.is_finalized() &&
         !test2.IsNull() && !test2.is_finalized()) {
    if (test1.raw() == test2.raw()) {
      // Found a cycle.
      return false;
    }
    test1 = test1.SuperClass();
    test2 = test2.SuperClass();
    if (!test2.IsNull()) {
      test2 = test2.SuperClass();
    }
  }
  // No cycles.
  return true;
}


// Returns false if the function type alias illegally refers to itself.
bool ClassFinalizer::IsAliasCycleFree(const Class& cls,
                                      GrowableArray<intptr_t>* visited) {
  ASSERT(cls.IsSignatureClass());
  ASSERT(!cls.IsCanonicalSignatureClass());
  ASSERT(!cls.is_finalized());
  ASSERT(visited != NULL);
  const intptr_t cls_index = cls.id();
  for (int i = 0; i < visited->length(); i++) {
    if ((*visited)[i] == cls_index) {
      // We have already visited alias 'cls'. We found a cycle.
      return false;
    }
  }

  // Visit the result type and parameter types of this signature type.
  visited->Add(cls.id());
  const Function& function = Function::Handle(cls.signature_function());
  // Check class of result type.
  AbstractType& type = AbstractType::Handle(function.result_type());
  ResolveType(cls, type, kFinalize);
  if (type.IsType() && !type.IsMalformed()) {
    const Class& type_class = Class::Handle(type.type_class());
    if (!type_class.is_finalized() &&
        type_class.IsSignatureClass() &&
        !type_class.IsCanonicalSignatureClass()) {
      if (!IsAliasCycleFree(type_class, visited)) {
        return false;
      }
    }
  }
  // Check classes of formal parameter types.
  const intptr_t num_parameters = function.NumberOfParameters();
  for (intptr_t i = 0; i < num_parameters; i++) {
    type = function.ParameterTypeAt(i);
    ResolveType(cls, type, kFinalize);
    if (type.IsType() && !type.IsMalformed()) {
      const Class& type_class = Class::Handle(type.type_class());
      if (!type_class.is_finalized() &&
          type_class.IsSignatureClass() &&
          !type_class.IsCanonicalSignatureClass()) {
        if (!IsAliasCycleFree(type_class, visited)) {
          return false;
        }
      }
    }
  }
  visited->RemoveLast();
  return true;
}


bool ClassFinalizer::AddInterfaceIfUnique(
    const GrowableObjectArray& interface_list,
    const AbstractType& interface,
    AbstractType* conflicting) {
  String& interface_class_name = String::Handle(interface.ClassName());
  String& existing_interface_class_name = String::Handle();
  String& interface_name = String::Handle();
  String& existing_interface_name = String::Handle();
  AbstractType& other_interface = AbstractType::Handle();
  for (intptr_t i = 0; i < interface_list.Length(); i++) {
    other_interface ^= interface_list.At(i);
    existing_interface_class_name = other_interface.ClassName();
    if (interface_class_name.Equals(existing_interface_class_name)) {
      // Same interface class name, now check names of type arguments.
      interface_name = interface.Name();
      existing_interface_name = other_interface.Name();
      // TODO(regis): Revisit depending on the outcome of issue 4905685.
      if (!interface_name.Equals(existing_interface_name)) {
        *conflicting = other_interface.raw();
        return false;
      } else {
        return true;
      }
    }
  }
  interface_list.Add(interface);
  return true;
}


// Walks the graph of explicitly declared interfaces of classes and
// interfaces recursively. Resolves unresolved interfaces.
// Returns false if there is an interface reference that cannot be
// resolved, or if there is a cycle in the graph. We detect cycles by
// remembering interfaces we've visited in each path through the
// graph. If we visit an interface a second time on a given path,
// we found a loop.
void ClassFinalizer::ResolveInterfaces(const Class& cls,
                                       GrowableArray<intptr_t>* visited) {
  ASSERT(visited != NULL);
  const intptr_t cls_index = cls.id();
  for (int i = 0; i < visited->length(); i++) {
    if ((*visited)[i] == cls_index) {
      // We have already visited interface class 'cls'. We found a cycle.
      const String& interface_name = String::Handle(cls.Name());
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "cyclic reference found for interface '%s'",
                  interface_name.ToCString());
    }
  }

  // If the class/interface has no explicit interfaces, we are done.
  Array& super_interfaces = Array::Handle(cls.interfaces());
  if (super_interfaces.Length() == 0) {
    return;
  }

  // If cls belongs to core lib or to core lib's implementation, restrictions
  // about allowed interfaces are lifted.
  const bool cls_belongs_to_core_lib =
      (cls.library() == Library::CoreLibrary()) ||
      (cls.library() == Library::CoreImplLibrary());

  // Resolve and check the interfaces of cls.
  visited->Add(cls_index);
  AbstractType& interface = AbstractType::Handle();
  Class& interface_class = Class::Handle();
  for (intptr_t i = 0; i < super_interfaces.Length(); i++) {
    interface ^= super_interfaces.At(i);
    ResolveType(cls, interface, kFinalizeWellFormed);
    if (interface.IsTypeParameter()) {
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "type parameter '%s' cannot be used as interface",
                  String::Handle(interface.Name()).ToCString());
    }
    interface_class = interface.type_class();
    if (interface_class.IsSignatureClass()) {
      const Script& script = Script::Handle(cls.script());
      ReportError(script, cls.token_index(),
                  "'%s' is used where an interface or class name is expected",
                  String::Handle(interface_class.Name()).ToCString());
    }
    // Verify that unless cls belongs to core lib, it cannot extend or implement
    // any of bool, num, int, double, String, Function, Dynamic.
    // The exception is signature classes, which are compiler generated and
    // represent a function type, therefore implementing the Function interface.
    if (!cls_belongs_to_core_lib) {
      if (interface.IsBoolInterface() ||
          interface.IsNumberInterface() ||
          interface.IsIntInterface() ||
          interface.IsDoubleInterface() ||
          interface.IsStringInterface() ||
          (interface.IsFunctionInterface() && !cls.IsSignatureClass()) ||
          interface.IsDynamicType()) {
        const Script& script = Script::Handle(cls.script());
        ReportError(script, cls.token_index(),
                    "'%s' is not allowed to extend or implement '%s'",
                    String::Handle(cls.Name()).ToCString(),
                    String::Handle(interface_class.Name()).ToCString());
      }
    }
    // Now resolve the super interfaces.
    ResolveInterfaces(interface_class, visited);
  }
  visited->RemoveLast();
}


// A class is marked as constant if it has one constant constructor.
// A constant class:
// - may extend only const classes.
// - has only const instance fields.
// Note: we must check for cycles before checking for const properties.
void ClassFinalizer::CheckForLegalConstClass(const Class& cls) {
  ASSERT(cls.is_const());
  const Class& super = Class::Handle(cls.SuperClass());
  if (!super.IsNull() && !super.is_const()) {
    String& name = String::Handle(super.Name());
    const Script& script = Script::Handle(cls.script());
    ReportError(script, cls.token_index(),
                "superclass '%s' must be const", name.ToCString());
  }
  const Array& fields_array = Array::Handle(cls.fields());
  intptr_t len = fields_array.Length();
  Field& field = Field::Handle();
  for (intptr_t i = 0; i < len; i++) {
    field ^= fields_array.At(i);
    if (!field.is_static() && !field.is_final()) {
      const String& class_name = String::Handle(cls.Name());
      const String& field_name = String::Handle(field.name());
      const Script& script = Script::Handle(cls.script());
      ReportError(script, field.token_index(),
                  "const class '%s' has non-final field '%s'",
                  class_name.ToCString(), field_name.ToCString());
    }
  }
}


void ClassFinalizer::PrintClassInformation(const Class& cls) {
  HANDLESCOPE(Isolate::Current());
  const String& class_name = String::Handle(cls.Name());
  OS::Print("%s '%s'",
            cls.is_interface() ? "interface" : "class",
            class_name.ToCString());
  const Library& library = Library::Handle(cls.library());
  if (!library.IsNull()) {
    OS::Print(" library '%s%s':\n",
              String::Handle(library.url()).ToCString(),
              String::Handle(library.private_key()).ToCString());
  } else {
    OS::Print(" (null library):\n");
  }
  const Type& super_type = Type::Handle(cls.super_type());
  if (super_type.IsNull()) {
    OS::Print("  Super: NULL");
  } else {
    const String& super_name = String::Handle(super_type.Name());
    OS::Print("  Super: %s", super_name.ToCString());
  }
  const Array& interfaces_array = Array::Handle(cls.interfaces());
  if (interfaces_array.Length() > 0) {
    OS::Print("; interfaces: ");
    AbstractType& interface = AbstractType::Handle();
    intptr_t len = interfaces_array.Length();
    for (intptr_t i = 0; i < len; i++) {
      interface ^= interfaces_array.At(i);
      OS::Print("  %s ", interface.ToCString());
    }
  }
  OS::Print("\n");
  const Array& functions_array = Array::Handle(cls.functions());
  Function& function = Function::Handle();
  intptr_t len = functions_array.Length();
  for (intptr_t i = 0; i < len; i++) {
    function ^= functions_array.At(i);
    OS::Print("  %s\n", function.ToCString());
  }
  const Array& fields_array = Array::Handle(cls.fields());
  Field& field = Field::Handle();
  len = fields_array.Length();
  for (intptr_t i = 0; i < len; i++) {
    field ^= fields_array.At(i);
    OS::Print("  %s\n", field.ToCString());
  }
}


void ClassFinalizer::FinalizeMalformedType(const Error& prev_error,
                                           const Class& cls,
                                           const Type& type,
                                           FinalizationKind finalization,
                                           const char* format, ...) {
  va_list args;
  va_start(args, format);
  LanguageError& error = LanguageError::Handle();
  if (FLAG_enable_type_checks ||
      !type.HasResolvedTypeClass() ||
      (finalization == kFinalizeWellFormed)) {
    const Script& script = Script::Handle(cls.script());
    if (prev_error.IsNull()) {
      error ^= Parser::FormatError(
          script, type.token_index(), "Error", format, args);
    } else {
      error ^= Parser::FormatErrorWithAppend(
          prev_error, script, type.token_index(), "Error", format, args);
    }
    if (finalization == kFinalizeWellFormed) {
      ReportError(error);
    }
  }
  if (FLAG_enable_type_checks || !type.HasResolvedTypeClass()) {
    // In check mode, always mark the type as malformed.
    // In production mode, mark the type as malformed only if its type class is
    // not resolved.
    type.set_malformed_error(error);
  } else {
     // In production mode, do not mark the type with a resolved type class as
     // malformed, but make it raw.
    ASSERT(type.HasResolvedTypeClass());
    type.set_arguments(AbstractTypeArguments::Handle());
  }
  if (!type.IsFinalized()) {
    type.set_is_finalized_instantiated();
    // Do not canonicalize malformed types, since they may not be resolved.
  } else {
    // The only case where the malformed type was already finalized is when its
    // type arguments are not within bounds. In that case, we have a prev_error.
    ASSERT(!prev_error.IsNull());
  }
}


void ClassFinalizer::ReportError(const Error& error) {
  Isolate::Current()->long_jump_base()->Jump(1, error);
  UNREACHABLE();
}


void ClassFinalizer::ReportError(const Script& script,
                                 intptr_t token_index,
                                 const char* format, ...) {
  va_list args;
  va_start(args, format);
  const Error& error = Error::Handle(
      Parser::FormatError(script, token_index, "Error", format, args));
  ReportError(error);
}


void ClassFinalizer::ReportError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const Error& error = Error::Handle(
      Parser::FormatError(Script::Handle(), -1, "Error", format, args));
  va_end(args);
  ReportError(error);
}

}  // namespace dart
