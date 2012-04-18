// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#library('LibraryPrivateInConstructor');
#import("LibraryPrivateInConstructorA.dart");
#import("LibraryPrivateInConstructorB.dart");

main() {
  var b = new B();
  Expect.equals(499, b.x);
  Expect.equals(42, b.y);
}
