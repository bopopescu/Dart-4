// Copyright (c) 2011, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

class WorkElement {
  final Element element;
  TreeElements resolutionTree;
  Function run;

  WorkElement.toCompile(this.element) : resolutionTree = null {
    run = this.compile;
  }

  WorkElement.toCodegen(this.element, this.resolutionTree) {
    run = this.codegen;
  }

  bool isAnalyzed() => resolutionTree != null;
  int hashCode() => element.hashCode();

  String compile(Compiler compiler) {
    return compiler.compileMethod(this);
  }

  String codegen(Compiler compiler) {
    return compiler.codegenMethod(this);
  }
}

class Compiler implements Canceler, Logger {
  Queue<WorkElement> worklist;
  Universe universe;
  String assembledCode;
  Namer namer;
  Types types;

  CompilerTask measuredTask;
  Element currentElement;
  Element currentLibrary = null; // TODO(ngeoffray): initialize this.

  List<CompilerTask> tasks;
  ScannerTask scanner;
  ParserTask parser;
  TreeValidatorTask validator;
  ResolverTask resolver;
  TypeCheckerTask checker;
  SsaBuilderTask builder;
  SsaOptimizerTask optimizer;
  SsaCodeGeneratorTask generator;
  CodeEmitterTask emitter;

  static final SourceString MAIN = const SourceString('main');

  Compiler() {
    types = new Types();
    universe = new Universe();
    worklist = new Queue<WorkElement>();
    namer = new Namer();
    scanner = new ScannerTask(this);
    parser = new ParserTask(this);
    validator = new TreeValidatorTask(this);
    resolver = new ResolverTask(this);
    checker = new TypeCheckerTask(this);
    builder = new SsaBuilderTask(this);
    optimizer = new SsaOptimizerTask(this);
    generator = new SsaCodeGeneratorTask(this);
    emitter = new CodeEmitterTask(this);
    tasks = [scanner, parser, resolver, checker, builder, optimizer, generator,
             emitter];
  }

  void ensure(bool condition) {
    if (!condition) cancel('failed assertion in leg');
  }

  void unimplemented(String methodName,
                     [Node node, Token token, HInstruction instruction]) {
    cancel("$methodName not implemented", node, token, instruction);
  }

  void internalError(String message,
                     [Node node, Token token, HInstruction instruction]) {
    cancel("Internal Error: $message", node, token, instruction);
  }

  void cancel([String reason, Node node, Token token,
               HInstruction instruction]) {
    throw new CompilerCancelledException(reason);
  }

  void log(message) {
    // Do nothing.
  }

  bool run(Script script) {
    try {
      runCompiler(script);
    } catch (CompilerCancelledException exception) {
      log(exception.toString());
      log('compilation failed');
      return false;
    }
    // TODO(floitsch): the following code can be removed once the HTracer
    // writes directly into a file.
    if (GENERATE_SSA_TRACE) {
      print("------------------");
      print(new HTracer.singleton());
      print("------------------");
    }
    log('compilation succeeded');
    return true;
  }

  void scanCoreLibrary() {
    String fileName = io.join([legDirectory, 'lib', 'core.dart']);
    currentElement = new CompilationUnitElement(
        readScript(fileName), currentLibrary);
    scanner.scan(currentElement);
    // Make our special function a foreign kind.
    Element element = new ForeignElement(const SourceString('JS'));
    universe.define(element);
  }

  void enqueueInvokedInstanceMethods() {
    // TODO(floitsch): find a more efficient way of doing this.
    // Run through the classes and see if we need to compile methods.
    for (ClassElement classElement in universe.instantiatedClasses) {
      for (ClassElement currentClass = classElement;
           currentClass !== null;
           currentClass = currentClass.superClass) {
        // TODO(floitsch): we don't need to add members that have been
        // overwritten by subclasses.
        for (Element member in currentClass.members) {
          SourceString name = member.name;
          if (Elements.isInstanceMethod(member) &&
              universe.generatedCode[member] === null &&
              universe.invokedNames.contains(namer.instanceName(name))) {
            addToWorklist(member);
          }
        }
      }
    }
  }

  void runCompiler(Script script) {
    scanCoreLibrary();
    currentElement = new CompilationUnitElement(script, currentLibrary);
    scanner.scan(currentElement);
    Element element = universe.find(MAIN);
    if (element === null) cancel('Could not find $MAIN');
    worklist.add(new WorkElement.toCompile(element));
    do {
      while (!worklist.isEmpty()) {
        WorkElement work = worklist.removeLast();
        currentElement = work.element;
        work.run(this);
      }
      enqueueInvokedInstanceMethods();
    } while (!worklist.isEmpty());
    emitter.assembleProgram();
  }

  TreeElements analyzeMethod(WorkElement work) {
    Node tree = parser.parse(work.element);
    validator.validate(tree);
    TreeElements elements = resolver.resolve(work.element);
    work.resolutionTree = elements;
    checker.check(tree, elements);
    return elements;
  }

  String codegenMethod(WorkElement work) {
    HGraph graph = builder.build(work.element, work.resolutionTree);
    optimizer.optimize(graph);
    String code = generator.generate(work.element, graph);
    universe.addGeneratedCode(work.element, code);
    return code;
  }

  String compileMethod(WorkElement work) {
    String code = universe.generatedCode[work.element];
    if (code !== null) return code;
    analyzeMethod(work);
    return codegenMethod(work);
  }

  void addToWorklist(Element element) {
    if (element.kind === ElementKind.GENERATIVE_CONSTRUCTOR) {
      registerInstantiatedClass(element.enclosingElement);
    }
    worklist.add(new WorkElement.toCompile(element));
  }

  void registerStaticInvocation(Element element) {
    addToWorklist(element);
  }

  void registerDynamicInvocation(String methodName) {
    universe.invokedNames.add(methodName);
  }

  void registerInstantiatedClass(ClassElement element) {
    universe.instantiatedClasses.add(element);
  }

  Element resolveType(ClassElement element) {
    parser.parse(element);
    resolver.resolveType(element);
    return element;
  }

  Element resolveSignature(FunctionElement element) {
    parser.parse(element);
    resolver.resolveSignature(element);
    return element;
  }

  reportWarning(Node node, var message) {}
  reportError(Node node, var message) {}

  Script readScript(String filename) {
    unimplemented('Compiler.readScript');
  }

  String get legDirectory() {
    unimplemented('Compiler.legDirectory');
  }
}

class CompilerTask {
  final Compiler compiler;
  final Stopwatch watch;

  CompilerTask(this.compiler) : watch = new Stopwatch();

  String get name() => 'Unknown task';
  int get timing() => watch.elapsedInMs();

  measure(Function action) {
    // TODO(kasperl): Do we have to worry about exceptions here?
    CompilerTask previous = compiler.measuredTask;
    compiler.measuredTask = this;
    if (previous !== null) previous.watch.stop();
    watch.start();
    var result = action();
    watch.stop();
    if (previous !== null) previous.watch.start();
    compiler.measuredTask = previous;
    return result;
  }
}

class CompilerCancelledException implements Exception {
  final String reason;
  CompilerCancelledException(this.reason);

  String toString() {
    String banner = 'compiler cancelled';
    return (reason !== null) ? '$banner: $reason' : '$banner';
  }
}
