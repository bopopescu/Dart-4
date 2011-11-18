// Copyright (c) 2011, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// WARNING: Do not edit - generated code.

class _NotificationWrappingImplementation extends DOMWrapperBase implements Notification {
  _NotificationWrappingImplementation() : super() {}

  static create__NotificationWrappingImplementation() native {
    return new _NotificationWrappingImplementation();
  }

  String get dir() { return _get_dir(this); }
  static String _get_dir(var _this) native;

  void set dir(String value) { _set_dir(this, value); }
  static void _set_dir(var _this, String value) native;

  EventListener get onclick() { return _get_onclick(this); }
  static EventListener _get_onclick(var _this) native;

  void set onclick(EventListener value) { _set_onclick(this, value); }
  static void _set_onclick(var _this, EventListener value) native;

  EventListener get onclose() { return _get_onclose(this); }
  static EventListener _get_onclose(var _this) native;

  void set onclose(EventListener value) { _set_onclose(this, value); }
  static void _set_onclose(var _this, EventListener value) native;

  EventListener get ondisplay() { return _get_ondisplay(this); }
  static EventListener _get_ondisplay(var _this) native;

  void set ondisplay(EventListener value) { _set_ondisplay(this, value); }
  static void _set_ondisplay(var _this, EventListener value) native;

  EventListener get onerror() { return _get_onerror(this); }
  static EventListener _get_onerror(var _this) native;

  void set onerror(EventListener value) { _set_onerror(this, value); }
  static void _set_onerror(var _this, EventListener value) native;

  String get replaceId() { return _get_replaceId(this); }
  static String _get_replaceId(var _this) native;

  void set replaceId(String value) { _set_replaceId(this, value); }
  static void _set_replaceId(var _this, String value) native;

  void addEventListener(String type, EventListener listener, [bool useCapture = null]) {
    if (useCapture === null) {
      _addEventListener_Notification(this, type, listener);
      return;
    } else {
      _addEventListener_Notification_2(this, type, listener, useCapture);
      return;
    }
  }
  static void _addEventListener_Notification(receiver, type, listener) native;
  static void _addEventListener_Notification_2(receiver, type, listener, useCapture) native;

  void cancel() {
    _cancel(this);
    return;
  }
  static void _cancel(receiver) native;

  bool dispatchEvent(Event evt) {
    return _dispatchEvent_Notification(this, evt);
  }
  static bool _dispatchEvent_Notification(receiver, evt) native;

  void removeEventListener(String type, EventListener listener, [bool useCapture = null]) {
    if (useCapture === null) {
      _removeEventListener_Notification(this, type, listener);
      return;
    } else {
      _removeEventListener_Notification_2(this, type, listener, useCapture);
      return;
    }
  }
  static void _removeEventListener_Notification(receiver, type, listener) native;
  static void _removeEventListener_Notification_2(receiver, type, listener, useCapture) native;

  void show() {
    _show(this);
    return;
  }
  static void _show(receiver) native;

  String get typeName() { return "Notification"; }
}
