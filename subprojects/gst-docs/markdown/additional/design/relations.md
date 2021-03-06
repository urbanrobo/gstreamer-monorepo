# Object relation types

This document describes the relations between objects that exist in
GStreamer. It will also describe the way of handling the relation wrt
locking and refcounting.

## parent-child relation

```
     +---------+    +-------+
     | parent  |    | child |
*--->|       *----->|       |
     |       F1|<-----*    1|
     +---------+    +-------+
```

### properties
    - parent has references to multiple children
    - child has reference to parent
    - reference fields protected with LOCK
    - the reference held by each child to the parent is NOT reflected in
    the refcount of the parent.
    - the parent removes the floating flag of the child when taking
    ownership.
    - the application has valid reference to parent
    - creation/destruction requires two unnested locks and 1 refcount.

### usage in GStreamer

    * `GstBin` -> `GstElement`
    * `GstElement` -> `GstRealPad`

### lifecycle

#### object creation

The application creates two objects and holds a pointer
to them. The objects are initially FLOATING with a refcount of 1.

```
     +---------+              +-------+
*--->| parent  |         *--->| child |
     |       * |              |       |
     |       F1|              | *   F1|
     +---------+              +-------+
```

#### establishing the parent-child relationship

The application then calls a method on the parent object to take ownership of
the child object. The parent performs the following actions:

```
result = _set_parent (child, parent);
if (result) {
  lock (parent);
  ref_pointer = child;

  1.  update other data structures .. unlock (parent);
} else {

  2.  child had parent ..
}
```

the `_set_parent()` method performs the following actions:

```
lock (child);
if (child->parent != null) {
  unlock (child);
  return false;
}
if (is_floating (child)) {
  unset (child, floating);
}
else {
  _ref (child);
}
child->parent = parent;
unlock (child);
_signal (parent_set, child, parent);
return true;
```

The function atomically checks if the child has no parent yet
and will set the parent if not. It will also sink the child, meaning
all floating references to the child are invalid now as it takes
over the refcount of the object.

Visually:

after `_set_parent()` returns TRUE:

```
      +---------+            +-------+
*---->| parent  |      *-//->| child |
      |       * |            |       |
      |       F1|<-------------*    1|
      +---------+            +-------+
```

after parent updates `ref_pointer` to child.

```
      +---------+        +-------+
*---->| parent  |  *-//->| child |
      |       *--------->|       |
      |       F1|<---------*    1|
      +---------+        +-------+
```

- only one parent is able to `_sink` the same object because the
`_set_parent()` method is atomic.

- since only one parent is able to `_set_parent()` the object, only
one will add a reference to the object.

- since the parent can hold multiple references to children, we don???t
need to lock the parent when locking the child. Many threads can
call `_set_parent()` on the children with the same parent, the
parent can then add all those to its lists.

> Note: that the signal is emitted before the parent has added the
> element to its internal data structures. This is not a problem
> since the parent usually has his own signal to inform the app that
> the child was reffed. One possible solution would be to update the
> internal structure first and then perform a rollback if the `_set_parent()`
> failed. This is not a good solution as iterators might grab the
> 'half-added' child too soon.

#### using the parent-child relationship

  - since the initial floating reference to the child object became
    invalid after giving it to the parent, any reference to a child has
    at least a refcount \> 1.

  - this means that unreffing a child object cannot decrease the
    refcount to 0. In fact, only the parent can destroy and dispose the
    child object.

  - given a reference to the child object, the parent pointer is only
    valid when holding the child LOCK. Indeed, after unlocking the child
    LOCK, the parent can unparent the child or the parent could even
    become disposed. To avoid the parent dispose problem, when obtaining
    the parent pointer, if should be reffed before releasing the child
    LOCK.

  * getting a reference to the parent.
      - a referece is held to the child, so it cannot be disposed.

``` c
    LOCK (child);
    parent = _ref (child->parent);
    UNLOCK (child);

   .. use parent ..

   _unref (parent);
```

   * getting a reference to a child

      - a reference to a child can be obtained by reffing it before adding
        it to the parent or by querying the parent.

      - when requesting a child from the parent, a reference is held to the
        parent so it cannot be disposed. The parent will use its internal
        data structures to locate the child element and will return a
        reference to it with an incremented refcount. The requester should
        `_unref()` the child after usage.

   * destroying the parent-child relationship

      - only the parent can actively destroy the parent-child relationship
        this typically happens when a method is called on the parent to
        release ownership of the child.

      - a child shall never remove itself from the parent.

      - since calling a method on the parent with the child as an argument
        requires the caller to obtain a valid reference to the child, the
        child refcount is at least \> 1.

      - the parent will perform the folowing actions:

``` c
    LOCK (parent);
    if (ref_pointer == child) {
      ref_pointer = NULL;

      ..update other data structures ..
      UNLOCK (parent);

      _unparent (child);
    } else {
      UNLOCK (parent);
      .. not our child ..
    }
```

The `_unparent()` method performs the following actions:

``` c
LOCK (child);
if (child->parent != NULL) {
  child->parent = NULL;
  UNLOCK (child);
  _signal (PARENT_UNSET, child, parent);

  _unref (child);
} else {
  UNLOCK (child);
}
```

Since the `_unparent()` method unrefs the child object, it is possible that
the child pointer is invalid after this function. If the parent wants to
perform other actions on the child (such as signal emission) it should
`_ref()` the child first.

## single-reffed relation

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |       *--------->|         |
     |        1|        |        2|
     +---------+        +---------+
```

### properties
      - one object has a reference to another
      - reference field protected with LOCK
      - the reference held by the object is reflected in the refcount of the
        other object.
      - typically the other object can be shared among multiple other
        objects where each ref is counted for in the refcount.
      - no object has ownership of the other.
      - either shared state or copy-on-write.
      - creation/destruction requires one lock and one refcount.

### usage

```
        GstRealPad -> GstCaps
        GstBuffer -> GstCaps
        GstEvent -> GstCaps
        GstEvent -> GstObject
        GstMessage -> GstCaps
        GstMessage -> GstObject
```

### lifecycle

#### Two objects exist unlinked.

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |      *  |        |         |
     |        1|        |        1|
     +---------+        +---------+
```

#### establishing the single-reffed relationship

The second object is attached to the first one using a method
on the first object. The second object is reffed and a pointer
is updated in the first object using the following algorithm:

``` c
LOCK (object1);
if (object1->pointer)
  _unref (object1->pointer);
object1->pointer = _ref (object2);
UNLOCK (object1);
```

After releasing the lock on the first object is is not sure that
object2 is still reffed from object1.

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |       *--------->|         |
     |        1|        |        2|
     +---------+        +---------+
```

#### using the single-reffed relationship

The only way to access object2 is by holding a ref to it or by
getting the reference from object1.
Reading the object pointed to by object1 can be done like this:

``` c
LOCK (object1);
object2 = object1->pointer;
_ref (object2);
UNLOCK (object1);

??? use object2 ???
_unref (object2);
```

Depending on the type of the object, modifications can be done either with
copy-on-write or directly into the object.

Copy on write can practically only be done like this:

``` c
LOCK (object1);
object2 = object1->pointer;
object2 = _copy_on_write (object2);
... make modifications to object2 ...
UNLOCK (object1);

Releasing the lock has only a very small window where the copy_on_write
actually does not perform a copy:

LOCK (object1);
object2 = object1->pointer;
_ref (object2);
UNLOCK (object1);

/* object2 now has at least 2 refcounts making the next
copy-on-write make a real copy, unless some other thread writes
another object2 to object1 here ??? */

object2 = _copy_on_write (object2);

/* make modifications to object2 ??? */

LOCK (object1);
if (object1->pointer != object2) {
  if (object1->pointer)
    _unref (object1->pointer);
  object1->pointer = gst_object_ref (object2);
}
UNLOCK (object1);
```

#### destroying the single-reffed relationship

The folowing algorithm removes the single-reffed link between
object1 and object2.

``` c
LOCK (object1);
_unref (object1->pointer);
object1->pointer = NULL;
UNLOCK (object1);
```

Which yields the following initial state again:

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |      *  |        |         |
     |        1|        |        1|
     +---------+        +---------+
```

## unreffed relation

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |       *--------->|         |
     |        1|<---------*      1|
     +---------+        +---------+
```

### properties

- two objects have references to each other
- both objects can only have 1 reference to another object.
- reference fields protected with LOCK
- the references held by each object are NOT reflected in the refcount
of the other object.
- no object has ownership of the other.
- typically each object is owned by a different parent.
- creation/destruction requires two nested locks and no refcounts.

### usage

- This type of link is used when the link is less important than the
existance of the objects, If one of the objects is disposed, so is
the link.

    `GstRealPad` <-> `GstRealPad` (srcpad lock taken first)

### lifecycle

#### Two objects exist unlinked.

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |       * |        |         |
     |        1|        | *      1|
     +---------+        +---------+
```

#### establishing the unreffed relationship

Since we need to take two locks, the order in which these locks are
taken is very important or we might cause deadlocks. This lock order
must be defined for all unreffed relations. In these examples we always
lock object1 first and then object2.

``` c
LOCK (object1);
LOCK (object2);
object2->refpointer = object1;
object1->refpointer = object2;
UNLOCK (object2);
UNLOCK (object1);
```

#### using the unreffed relationship

Reading requires taking one of the locks and reading the corresponing
object. Again we need to ref the object before releasing the lock.

``` c
LOCK (object1);
object2 = _ref (object1->refpointer);
UNLOCK (object1);

.. use object2 ..
_unref (object2);
```

#### destroying the unreffed relationship

Because of the lock order we need to be careful when destroying this
relation.

When only a reference to object1 is held:

``` c
LOCK (object1);
LOCK (object2);
object1->refpointer->refpointer = NULL;
object1->refpointer = NULL;
UNLOCK (object2);
UNLOCK (object1);
```

When only a reference to object2 is held, we need to get a handle to the
other object fist so that we can lock it first. There is a window where
we need to release all locks and the relation could be invalid. To solve
this we check the relation after grabbing both locks and retry if the
relation changed.

``` c
retry:
  LOCK (object2);
  object1 = _ref (object2->refpointer);
  UNLOCK (object2);
  .. things can change here ..
  LOCK (object1);
  LOCK (object2);
  if (object1 == object2->refpointer) {
    /* relation unchanged */
    object1->refpointer->refpointer = NULL;
    object1->refpointer = NULL;
  }
  else {
    /* relation changed.. retry */
    UNLOCK (object2);
    UNLOCK (object1);
    _unref (object1);
    goto retry;
  }
  UNLOCK (object2);
  UNLOCK (object1);
  _unref (object1);

/* When references are held to both objects. Note that it is not possible to
get references to both objects with the locks released since when the
references are taken and the locks are released, a concurrent update might
have changed the link, making the references not point to linked objects. */

LOCK (object1);
LOCK (object2);
if (object1->refpointer == object2) {
  object2->refpointer = NULL;
  object1->refpointer = NULL;
}
else {
  .. objects are not linked ..
}
UNLOCK (object2);
UNLOCK (object1);
```

## double-reffed relation

```
     +---------+        +---------+
*--->| object1 |   *--->| object2 |
     |       *--------->|         |
     |        2|<---------*      2|
     +---------+        +---------+
```

### properties

  - two objects have references to each other
  - reference fields protected with LOCK
  - the references held by each object are reflected in the refcount of
    the other object.
  - no object has ownership of the other.
  - typically each object is owned by a different parent.
  - creation/destruction requires two locks and two refcounts.

#### usage

Not used in GStreamer.

### lifecycle
