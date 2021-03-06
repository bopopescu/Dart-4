// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

interface Window {
  final int document;
}

// Defining this global object makes Frog eager on optimizing
// call sites where the receiver is typed 'Window'.
class _DOMWindowJs implements Window native "@*DOMWindow" {
  final int document;
}

class Win implements Window {
}

main() {
  // By typing this variable to 'Window', Frog will optimize calls on
  // it.
  Window win = new Win();
  Expect.throws(() => win.document,
                (e) => e is NoSuchMethodException);
}
