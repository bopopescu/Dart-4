/*
 * Copyright (c) 2011, the Dart project authors.
 * 
 * Licensed under the Eclipse Public License v1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 * 
 * http://www.eclipse.org/legal/epl-v10.html
 * 
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
package com.google.dart.tools.core.utilities.bindings;

import com.google.dart.compiler.ast.DartClass;
import com.google.dart.compiler.ast.DartField;
import com.google.dart.compiler.ast.DartFieldDefinition;
import com.google.dart.compiler.ast.DartFunction;
import com.google.dart.compiler.ast.DartIdentifier;
import com.google.dart.compiler.ast.DartMethodDefinition;
import com.google.dart.compiler.ast.DartNode;
import com.google.dart.compiler.ast.DartUnit;
import com.google.dart.compiler.resolver.FieldElement;
import com.google.dart.compiler.resolver.MethodElement;
import com.google.dart.tools.core.model.CompilationUnit;
import com.google.dart.tools.core.model.CompilationUnitElement;
import com.google.dart.tools.core.model.DartElement;
import com.google.dart.tools.core.model.DartLibrary;
import com.google.dart.tools.core.model.DartModelException;
import com.google.dart.tools.core.model.DartProject;
import com.google.dart.tools.core.model.DartVariableDeclaration;
import com.google.dart.tools.core.model.Field;
import com.google.dart.tools.core.model.Method;
import com.google.dart.tools.core.model.Type;
import com.google.dart.tools.core.test.util.TestUtilities;
import com.google.dart.tools.core.utilities.compiler.DartCompilerUtilities;

import junit.framework.TestCase;

public class BindingUtilsTest extends TestCase {
  public void test_BindingUtils_getDartElement_field_global() throws Exception {
    CompilationUnit compilationUnit = getSampleCompilationUnit("sampler.dart");
    DartVariableDeclaration expectedField = getGlobalVariable(compilationUnit);
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartField fieldNode = getGlobalVariable(ast, expectedField.getElementName());
    CompilationUnitElement actualField = BindingUtils.getDartElement(compilationUnit.getLibrary(),
        fieldNode.getSymbol());
    assertEquals(expectedField, actualField);
  }

  public void test_BindingUtils_getDartElement_field_notNull() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    Type type = compilationUnit.getTypes()[0];
    Field expectedField = type.getFields()[0];
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartClass classNode = getType(ast, type.getElementName());
    DartField fieldNode = getField(classNode, expectedField.getElementName());
    CompilationUnitElement actualField = BindingUtils.getDartElement(compilationUnit.getLibrary(),
        fieldNode.getSymbol());
    assertEquals(expectedField, actualField);
  }

  public void test_BindingUtils_getDartElement_field_null() throws Exception {
    DartLibrary library = getMoneyLibrary();
    CompilationUnitElement field = BindingUtils.getDartElement(library, (FieldElement) null);
    assertNull(field);
  }

  public void test_BindingUtils_getDartElement_method_constructor() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    Type type = compilationUnit.getTypes()[0];
    Method expectedMethod = type.getMethods()[0];
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartClass classNode = getType(ast, type.getElementName());
    DartMethodDefinition methodNode = getMethod(classNode, expectedMethod.getElementName());
    Method actualMethod = BindingUtils.getDartElement(compilationUnit.getLibrary(),
        methodNode.getSymbol());
    assertEquals(expectedMethod, actualMethod);
  }

  public void test_BindingUtils_getDartElement_method_nonConstructor() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    Type type = compilationUnit.getTypes()[0];
    Method expectedMethod = type.getMethods()[1];
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartClass classNode = getType(ast, type.getElementName());
    DartMethodDefinition methodNode = getMethod(classNode, expectedMethod.getElementName());
    Method actualMethod = BindingUtils.getDartElement(compilationUnit.getLibrary(),
        methodNode.getSymbol());
    assertEquals(expectedMethod, actualMethod);
  }

  public void test_BindingUtils_getDartElement_method_null() throws Exception {
    DartLibrary library = getMoneyLibrary();
    Method method = BindingUtils.getDartElement(library, (MethodElement) null);
    assertNull(method);
  }

  public void test_BindingUtils_getEnclosingFunction_nonNull() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartClass classNode = (DartClass) ast.getTopLevelNodes().get(0);
    DartFunction function = getMethod(classNode).getFunction();
    assertEquals(function, BindingUtils.getEnclosingFunction(function.getBody()));
  }

  public void test_BindingUtils_getEnclosingFunction_null() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    assertNull(BindingUtils.getEnclosingFunction(ast));
  }

  public void test_BindingUtils_getEnclosingType_nonNull() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    DartClass classNode = (DartClass) ast.getTopLevelNodes().get(0);
    DartNode node = classNode.getMembers().get(0);
    assertEquals(classNode, BindingUtils.getEnclosingType(node));
  }

  public void test_BindingUtils_getEnclosingType_null() throws Exception {
    CompilationUnit compilationUnit = getMoneyCompilationUnit("simple_money.dart");
    DartUnit ast = DartCompilerUtilities.resolveUnit(compilationUnit);
    assertNull(BindingUtils.getEnclosingType(ast));
  }

  private CompilationUnit getCompilationUnit(DartLibrary library, String unitName) throws Exception {
    if (library == null) {
      return null;
    }
    CompilationUnit[] units = library.getCompilationUnits();
    for (CompilationUnit unit : units) {
      if (unit.getElementName().equals(unitName)) {
        return unit;
      }
    }
    return null;
  }

  private DartField getField(DartClass classNode, String fieldName) {
    for (DartNode node : classNode.getMembers()) {
      if (node instanceof DartFieldDefinition) {
        for (DartField field : ((DartFieldDefinition) node).getFields()) {
          if (field.getName().getTargetName().equals(fieldName)) {
            return field;
          }
        }
      }
    }
    return null;
  }

  private DartVariableDeclaration getGlobalVariable(CompilationUnit compilationUnit)
      throws DartModelException {
    for (DartElement element : compilationUnit.getChildren()) {
      if (element instanceof DartVariableDeclaration) {
        return (DartVariableDeclaration) element;
      }
    }
    return null;
  }

  private DartField getGlobalVariable(DartUnit unit, String variableName) {
    for (DartNode node : unit.getTopLevelNodes()) {
      if (node instanceof DartFieldDefinition) {
        for (DartField field : ((DartFieldDefinition) node).getFields()) {
          if (field.getName().getTargetName().equals(variableName)) {
            return field;
          }
        }
      }
    }
    return null;
  }

  private DartLibrary getLibrary(String projectName) throws Exception {
    DartProject project = TestUtilities.loadPluginRelativeProject(
        "com.google.dart.tools.core_test", projectName);
    if (project == null) {
      return null;
    }
    for (DartLibrary library : project.getDartLibraries()) {
      return library;
    }
    return null;
  }

  private DartMethodDefinition getMethod(DartClass classNode) {
    for (DartNode node : classNode.getMembers()) {
      if (node instanceof DartMethodDefinition) {
        return (DartMethodDefinition) node;
      }
    }
    return null;
  }

  private DartMethodDefinition getMethod(DartClass classNode, String methodName) {
    for (DartNode node : classNode.getMembers()) {
      if (node instanceof DartMethodDefinition) {
        DartMethodDefinition method = (DartMethodDefinition) node;
        if (((DartIdentifier) method.getName()).getTargetName().equals(methodName)) {
          return method;
        }
      }
    }
    return null;
  }

  private CompilationUnit getMoneyCompilationUnit(String unitName) throws Exception {
    return getCompilationUnit(getMoneyLibrary(), unitName);
  }

  private DartLibrary getMoneyLibrary() throws Exception {
    return getLibrary("Money");
  }

  private CompilationUnit getSampleCompilationUnit(String unitName) throws Exception {
    return getCompilationUnit(getSampleLibrary(), unitName);
  }

  private DartLibrary getSampleLibrary() throws Exception {
    return getLibrary("SampleCode");
  }

  private DartClass getType(DartUnit ast, String typeName) {
    for (DartNode node : ast.getTopLevelNodes()) {
      if (node instanceof DartClass) {
        DartClass classNode = (DartClass) node;
        if (classNode.getClassName().equals(typeName)) {
          return classNode;
        }
      }
    }
    return null;
  }
}
