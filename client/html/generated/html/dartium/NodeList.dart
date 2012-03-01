// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// TODO(nweiz): when all implementations we target have the same name for the
// coreimpl implementation of List<E>, extend that rather than wrapping.
class _ListWrapper<E> implements List<E> {
  List _list;

  _ListWrapper(List this._list);

  Iterator<E> iterator() => _list.iterator();

  void forEach(void f(E element)) => _list.forEach(f);

  Collection map(f(E element)) => _list.map(f);

  List<E> filter(bool f(E element)) => _list.filter(f);

  bool every(bool f(E element)) => _list.every(f);

  bool some(bool f(E element)) => _list.some(f);

  bool isEmpty() => _list.isEmpty();

  int get length() => _list.length;

  E operator [](int index) => _list[index];

  void operator []=(int index, E value) { _list[index] = value; }

  void set length(int newLength) { _list.length = newLength; }

  void add(E value) => _list.add(value);

  void addLast(E value) => _list.addLast(value);

  void addAll(Collection<E> collection) => _list.addAll(collection);

  void sort(int compare(E a, E b)) => _list.sort(compare);

  int indexOf(E element, [int start = 0]) => _list.indexOf(element, start);

  int lastIndexOf(E element, [int start = 0]) =>
    _list.lastIndexOf(element, start);

  void clear() => _list.clear();

  E removeLast() => _list.removeLast();

  E last() => _list.last();

  List<E> getRange(int start, int length) => _list.getRange(start, length);

  void setRange(int start, int length, List<E> from, [int startFrom = 0]) =>
    _list.setRange(start, length, from, startFrom);

  void removeRange(int start, int length) => _list.removeRange(start, length);

  void insertRange(int start, int length, [E initialValue = null]) =>
    _list.insertRange(start, length, initialValue);

  E get first() => _list[0];
}

/**
 * This class is used to insure the results of list operations are NodeLists
 * instead of lists.
 */
class _NodeListWrapper extends _ListWrapper<Node> implements NodeList {
  _NodeListWrapper(List list) : super(list);

  NodeList filter(bool f(Node element)) =>
    new _NodeListWrapper(_list.filter(f));

  NodeList getRange(int start, int length) =>
    new _NodeListWrapper(_list.getRange(start, length));
}

class _NodeListImpl extends _DOMTypeBase implements NodeList {
  _NodeImpl _parent;

  // -- start List<Node> mixins.
  // Node is the element type.

  // From Iterable<Node>:

  Iterator<Node> iterator() {
    // Note: NodeLists are not fixed size. And most probably length shouldn't
    // be cached in both iterator _and_ forEach method. For now caching it
    // for consistency.
    return new _FixedSizeListIterator<Node>(this);
  }

  // From Collection<Node>:

  void add(_NodeImpl value) {
    _parent._appendChild(value);
  }

  void addLast(_NodeImpl value) {
    _parent._appendChild(value);
  }

  void addAll(Collection<_NodeImpl> collection) {
    for (_NodeImpl node in collection) {
      _parent._appendChild(node);      
    }
  }

  _NodeImpl removeLast() {
    final last = this.last();
    if (last != null) {
      _parent._removeChild(last);
    }
    return last;
  }

  void clear() {
    _parent.text = '';
  }

  void operator []=(int index, _NodeImpl value) {
    _parent._replaceChild(value, this[index]);
  }

  void forEach(void f(Node element)) => _Collections.forEach(this, f);

  Collection map(f(Node element)) => _Collections.map(this, [], f);

  Collection<Node> filter(bool f(Node element)) =>
     new _NodeListWrapper(_Collections.filter(this, <Node>[], f));

  bool every(bool f(Node element)) => _Collections.every(this, f);

  bool some(bool f(Node element)) => _Collections.some(this, f);

  bool isEmpty() => this.length == 0;

  // From List<Node>:

  void sort(int compare(Node a, Node b)) {
    throw new UnsupportedOperationException("Cannot sort immutable List.");
  }

  int indexOf(Node element, [int start = 0]) =>
      _Lists.indexOf(this, element, start, this.length);

  int lastIndexOf(Node element, [int start = 0]) =>
      _Lists.lastIndexOf(this, element, start);

  Node last() => this[length - 1];
  Node get first() => this[0];

  // FIXME: implement thesee.
  void setRange(int start, int length, List<Node> from, [int startFrom]) {
    throw new UnsupportedOperationException("Cannot setRange on immutable List.");
  }
  void removeRange(int start, int length) {
    throw new UnsupportedOperationException("Cannot removeRange on immutable List.");
  }
  void insertRange(int start, int length, [Node initialValue]) {
    throw new UnsupportedOperationException("Cannot insertRange on immutable List.");
  }
  NodeList getRange(int start, int length) =>
    new _NodeListWrapper(_Lists.getRange(this, start, length, <Node>[]));

  // -- end List<Node> mixins.

  _NodeListImpl._wrap(ptr) : super._wrap(ptr);

  int get length() => _wrap(_ptr.length);

  Node operator[](int index) => _wrap(_ptr[index]);


}
