// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// WARNING: Do not edit - generated code.

interface CSSRule {

  static final int CHARSET_RULE = 2;

  static final int FONT_FACE_RULE = 5;

  static final int IMPORT_RULE = 3;

  static final int MEDIA_RULE = 4;

  static final int PAGE_RULE = 6;

  static final int STYLE_RULE = 1;

  static final int UNKNOWN_RULE = 0;

  static final int WEBKIT_KEYFRAMES_RULE = 7;

  static final int WEBKIT_KEYFRAME_RULE = 8;

  static final int WEBKIT_REGION_RULE = 10;

  String cssText;

  final CSSRule parentRule;

  final CSSStyleSheet parentStyleSheet;

  final int type;
}
