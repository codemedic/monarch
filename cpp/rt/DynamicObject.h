/*
 * Copyright (c) 2007-2011 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef monarch_rt_DynamicObject_H
#define monarch_rt_DynamicObject_H

#include "monarch/rt/Collectable.h"
#include "monarch/rt/DynamicObjectImpl.h"

#include <inttypes.h>
#include <algorithm>

namespace monarch
{
namespace rt
{

// forward declare DynamicObjectIterator
class DynamicObjectIterator;

/**
 * A DynamicObject is a reference counted object with a collection of
 * unordered name/value member pairs. Members can be dynamically added to
 * a DynamicObject.
 *
 * @author Dave Longley
 */
class DynamicObject : public Collectable<DynamicObjectImpl>
{
public:
   /**
    * Comparison function for sort.
    */
   typedef bool (*CompareLessDyno)(DynamicObject, DynamicObject);

   /**
    * Functor for sort to allow non-const sort functors.
    */
   struct SortFunctor
   {
      virtual bool operator()(DynamicObject& a, DynamicObject& b) = 0;
   };

   /**
    * Functor for filter.
    */
   typedef bool (*FilterDyno)(DynamicObject&);

   /**
    * Struct for filter.
    */
   struct FilterFunctor
   {
      virtual bool operator()(DynamicObject& d) const = 0;
   };

   /**
    * DynamicObject differencing flags.
    */
   enum
   {
      /**
       * Compare all objects for exact equality.
       */
      DiffEqual = 0,

      /**
       * Compare all 32-bit and 64-bit integers as 64-bit integers. UInt32
       * types will be compared against UInt64 and Int32 will be compared
       * against Int64.
       */
      DiffIntegersAsInt64s = 1 << 0,

      /**
       * Compare doubles as strings.
       */
      DiffDoublesAsStrings = 1 << 1,

      /**
       * Default diff flags (DiffCompareAsInt64).
       */
      DiffDefault = DiffIntegersAsInt64s
   };

   /**
    * Creates a new DynamicObject with a new, empty DynamicObjectImpl.
    */
   DynamicObject();

   /**
    * Creates a new DynamicObject with a new, empty DynamicObjectImpl of a
    * certain type.
    *
    * @param type the type to create.
    */
   DynamicObject(DynamicObjectType type);

   /**
    * Creates a DynamicObject that will reference count and then destroy
    * the passed DynamicObjectImpl.
    *
    * @param impl a pointer to a DynamicObjectImpl.
    */
   DynamicObject(DynamicObjectImpl* impl);

   /**
    * Creates a new DynamicObject by copying another one.
    *
    * @param copy the DynamicObject to copy.
    */
   DynamicObject(const DynamicObject& rhs);

   /**
    * Destructs this DynamicObject.
    */
   virtual ~DynamicObject();

   /**
    * Compares this DynamicObject to another one for equality. If the
    * DynamicObjects are the same type and have the same value or contain
    * all the same values (for Maps/Arrays), then they are equal. If the
    * DynamicObjects have different types but their string values are
    * equivalent, then they are equal.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if the passed DynamicObject is equal to this one, false
    *         if not.
    */
   virtual bool operator==(const DynamicObject& rhs) const;

   /**
    * Compares this DynamicObject to another one for non-equality. If the
    * DynamicObjects are the same type, and have the same value or contain
    * all the same values (for Maps/Arrays), then they are equal -- otherwise
    * they are not.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if the passed DynamicObject is not equal to this one, false
    *         if not.
    */
   virtual bool operator!=(const DynamicObject& rhs) const;

   /**
    * Compares this DynamicObject to a string for equality. If the
    * DynamicObject is not a string then the comparison will return false.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if the passed string is equal to this one, false if not.
    */
   virtual bool operator==(const char* rhs) const;

   /**
    * Compares this DynamicObject to a string for non-equality. If the
    * DynamicObject is not a string then the comparison will return true.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if the passed string is not equal to this one, false if not.
    */
   virtual bool operator!=(const char* rhs) const;

   /**
    * Compares this DynamicObject to a string. If the DynamicObject is a string
    * that is lexically less than the given string, then the method will
    * return true, otherwise it will return false.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if this DynamicObject is a string and the passed string is
    *         less than this DynamicObject, false otherwise.
    */
   virtual bool operator<(const char* rhs) const;

   /**
    * Compares this DynamicObject to a string. If the DynamicObject is a string
    * that is lexically less than or equal to the given string, then the method
    * will return true, otherwise it will return false.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if this DynamicObject is a string and the passed string is
    *         less than or equal to this DynamicObject, false otherwise.
    */
   virtual bool operator<=(const char* rhs) const;

   /**
    * Compares this DynamicObject to a string. If the DynamicObject is a string
    * that is lexically greater than the given string, then the method will
    * return true, otherwise it will return false.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if this DynamicObject is a string and the passed string is
    *         greater than this DynamicObject, false otherwise.
    */
   virtual bool operator>(const char* rhs) const;

   /**
    * Compares this DynamicObject to a string. If the DynamicObject is a string
    * that is lexically greater than or equal to the given string, then the
    * method will return true, otherwise it will return false.
    *
    * @param rhs the string to compare to this one.
    *
    * @return true if this DynamicObject is a string and the passed string is
    *         greater than or equal to this DynamicObject, false otherwise.
    */
   virtual bool operator>=(const char* rhs) const;

   /**
    * Compares this DynamicObject to another one to see if it is less than it.
    *
    * If one DynamicObject is NULL, it is less.
    *
    * If the DynamicObjects are both numbers, their operator< will be used.
    *
    * If they are both booleans, false will be less than true.
    *
    * If they are both strings, strcmp will be used.
    *
    * If they are both maps, then the smaller map will be less. If the maps are
    * the same size, then their keys will be compared lexigraphically using
    * operator<. If their keys are the same, the values of their keys will be
    * compared using operator<.
    *
    * If they are both arrays, then operator< will be used.
    *
    * If they are different types, then their string value will be compared.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if this DynamicObject is less than the passed one, false if
    *         not.
    */
   virtual bool operator<(const DynamicObject& rhs) const;

   /**
    * Compares this DynamicObject to another one to see if it is less than
    * or equal it.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if this DynamicObject is less than or equal to the passed
    *         one, false if not.
    */
   virtual bool operator<=(const DynamicObject& rhs) const;

   /**
    * Compares this DynamicObject to another one to see if it is greater than
    * it. If the first DynamicObject is not equal and not greater than the
    * second, then it is less.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if this DynamicObject is greater than the passed one, false
    *         if not.
    */
   virtual bool operator>(const DynamicObject& rhs) const;

   /**
    * Compares this DynamicObject to another one to see if it is greater than
    * or equal it.
    *
    * @param rhs the DynamicObject to compare to this one.
    *
    * @return true if this DynamicObject is greater than or equal to the passed
    *         one, false if not.
    */
   virtual bool operator>=(const DynamicObject& rhs) const;

   /**
    * Sets this object's value to a string.
    *
    * @param value the value for this object.
    */
   virtual void operator=(const char* value);

   /**
    * Sets this object's value to a string.
    *
    * @param value the value for this object.
    */
   virtual void operator=(const unsigned char* value);

   /**
    * Sets this object's value to a boolean.
    *
    * @param value the value for this object.
    */
   virtual void operator=(bool value);

   /**
    * Sets this object's value to a 32-bit integer.
    *
    * @param value the value for this object.
    */
   virtual void operator=(int32_t value);

   /**
    * Sets this object's value to a 32-bit unsigned integer.
    *
    * @param value the value for this object.
    */
   virtual void operator=(uint32_t value);

   /**
    * Sets this object's value to a 64-bit integer.
    *
    * @param value the value for this object.
    */
   virtual void operator=(int64_t value);

   /**
    * Sets this object's value to a 64-bit unsigned integer.
    *
    * @param value the value for this object.
    */
   virtual void operator=(uint64_t value);

   /**
    * Sets this object's value to a double.
    *
    * @param value the value for this object.
    */
   virtual void operator=(double value);

   /**
    * Gets this object's value as a string. If the type of this object
    * is not a string, then the returned pointer may be invalidated by
    * the next call to getString().
    *
    * @return the value of this object.
    */
   virtual operator const char*() const;

   /**
    * Gets this object's value as a boolean.
    *
    * @return the value of this object.
    */
   virtual operator bool() const;

   /**
    * Gets this object's value as a 32-bit integer.
    *
    * @return the value of this object.
    */
   virtual operator int32_t() const;

   /**
    * Gets this object's value as a 32-bit unsigned integer.
    *
    * @return the value of this object.
    */
   virtual operator uint32_t() const;

   /**
    * Gets this object's value as a 64-bit integer.
    *
    * @return the value of this object.
    */
   virtual operator int64_t() const;

   /**
    * Gets this object's value as a 64-bit unsigned integer.
    *
    * @return the value of this object.
    */
   virtual operator uint64_t() const;

   /**
    * Gets this object's value as a double.
    *
    * @return the value of this object.
    */
   virtual operator double() const;

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](char* name);

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](char* name) const;

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](const char* name);

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](const char* name) const;

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](unsigned char* name);

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](unsigned char* name) const;

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](const unsigned char* name);

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its member name.
    *
    * @param name the name of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](const unsigned char* name) const;

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its index. A
    * negative index will index in reverse, with -1 referring to the last
    * element.
    *
    * @param index the index of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](int index);

   /**
    * Gets a DynamicObject from a DynamicObjectImpl based on its index. A
    * negative index will index in reverse, with -1 referring to the last
    * element.
    *
    * @param index the index of the member.
    *
    * @return the DynamicObject.
    */
   virtual DynamicObject& operator[](int index) const;

   /**
    * Gets a reference-counted DynamicObjectIterator for iterating over
    * the members of this object or its array elements.
    *
    * @return a DynamicObjectIterator.
    */
   virtual DynamicObjectIterator getIterator() const;

   /**
    * Appends the passed DynamicObject to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the DynamicObject to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(DynamicObject value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(const char* value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(bool value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(int32_t value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(uint32_t value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(int64_t value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(uint64_t value);

   /**
    * Appends the passed value to this one.
    *
    * If this object is not an array, it will be converted to one and its
    * original value will be pushed onto the array before the given value.
    *
    * @param value the value to push.
    *
    * @return this DynamicObject for chaining.
    */
   virtual DynamicObject& push(double value);

   /**
    * Removes the last element of a non-empty array.
    *
    * @return the last element or NULL.
    */
   virtual DynamicObject pop();

   /**
    * Removes the first element of a non-empty array.
    *
    * @return the first element or NULL.
    */
   virtual DynamicObject shift();

   /**
    * Gets a reference-counted DynamicObject for the first member or
    * array element in this object. If this DynamicObject is not a map
    * or array, then a reference to this object will be returned.
    *
    * @return a DynamicObject.
    */
   virtual DynamicObject first() const;

   /**
    * Gets a reference-counted DynamicObject for the last member or
    * array element in this object. If this DynamicObject is not a map
    * or array, then a reference to this object will be returned.
    *
    * @return a DynamicObject.
    */
   virtual DynamicObject last() const;

   /**
    * Gets the keys of this DynamicObject, if it is a map, in an array.
    *
    * @retuen the keys of the DynamicObject in an array.
    */
   virtual DynamicObject keys();

   /**
    * Gets the values of this DynamicObject, if it is a map, in an array.
    *
    * @retuen the values of the DynamicObject in an array.
    */
   virtual DynamicObject values();

   /**
    * Sorts this DynamicObject if it is an array.
    *
    * @param func a function or an object with a function that compares two
    *           DynamicObjects and returns true if the first is less than
    *           the second, false otherwise.
    *
    * @return the sorted array.
    */
   virtual DynamicObject& sort(DynamicObject::CompareLessDyno func = NULL);
   virtual DynamicObject& sort(DynamicObject::SortFunctor& func);

   /**
    * Filters elements from this DynamicObject, if it is an array, into
    * another array.
    *
    * @param func a function or an object with a function that takes a
    *           DynamicObject and returns true if the DynamicObject should
    *           be included in the result.
    */
   virtual DynamicObject filter(DynamicObject::FilterDyno func);
   virtual DynamicObject filter(DynamicObject::FilterFunctor& func);

   /**
    * Rotates the elements in this DynamicObject, if it an array, in place.
    *
    * The default rotation direction is to the left. This means that a call
    * to rotate(1) will move the first element in the array onto the end of
    * the array in place.
    *
    * @param num the number of elements to rotate.
    * @param left the direction to move the elements.
    *
    * @return a reference to the array.
    */
   virtual DynamicObject& rotate(int num = 1, bool left = true);

   /**
    * Returns a shallow copy that is a slice of this DynamicObject, if this
    * DynamicObject is an array. An empty array will be returned if this
    * object is not an array or there are no elements in the slice bounds.
    *
    * @param start the starting index for the slice.
    * @param end the ending index for the slice -1 to go to the end.
    *
    * @return the array slice.
    */
   virtual DynamicObject slice(int start = 0, int end = -1);

   /**
    * If this DynamicObject is an array, it is returned. If it is not an array,
    * an array is created and this DynamicObject is appended to it.
    *
    * @return a DynamicObject array.
    */
   virtual DynamicObject arrayify();

   /**
    * Clones this DynamicObject and returns it.
    *
    * @return a clone of this DynamicObject.
    */
   virtual DynamicObject clone();

   /**
    * Merges the passed DynamicObject into this one.
    *
    * If "append" is false:
    *
    * If the passed DynamicObject is a map, then all of its key-value pairs
    * will be deep-merged into this DynamicObject (converting it to a map if
    * necessary), overwriting any overlapping pairs. If it is an array, then
    * its elements will overwrite the elements in this DynamicObject object
    * (converting it to an array if necessary). If it is not a map or an
    * array, then this DynamicObject will be set to the clone of the passed
    * DynamicObject.
    *
    * If "append" is true:
    *
    * The same as if "append" is false with the following changes: If the
    * passed DynamicObject is an array, its elements will be appended to the
    * end of this DynamicObject (after converting it to an array if necessary).
    * If the passed DynamicObject is not an array or a map but this
    * DynamicObject is an array, then the passed DynamicObject will be
    * appended to this one.
    *
    * @param rhs the DynamicObject to merge into this one.
    * @param append true to append arrays, false to overwrite them.
    */
   virtual void merge(DynamicObject& rhs, bool append);

   /**
    * Generates the differences between this object and the given target object
    * by doing a deep compare between both objects. The differences are written
    * to the given result object.
    *
    * If differences are found, the result object will be a difference
    * description.  For simple types the description is:
    * {
    *    "type": "valueChanged|typeChanged",
    *    "source": <source object>,
    *    "target": <target object>
    * }
    * Where the depends if the entire type changed or just the value. The
    * source and target values are provided for reference.
    *
    * For Maps and Arrays the description is:
    * [
    *    {
    *       "key|index": <key|index>,
    *       "<type>": <value>
    *    },
    *    ...
    * ]
    * Where <key> or <index> is the source or target map key or array index
    * <type> is either "added", "removed", or "changed", and <value> is the
    * added or removed value or a sub-diff result.
    *
    * @param target the object to generate differences against.
    * @param result the resulting differences if there are any.
    * @param flags the set of flags that enable or disable certain comparison
    *              mechanisms used internally in the Dynamic object.
    *
    * @return true if the objects are different, false if the objects are the
    *         same.
    */
   virtual bool diff(
      DynamicObject& target, DynamicObject& result,
      uint32_t flags = DiffDefault);

   /**
    * Determines if this DynamicObject is a subset of another. If this
    * DynamicObject does not reference the exact same DynamicObject as the
    * passed one, then a check will be made to determine if this DynamicObject
    * this is a subset of the passed one.
    *
    * This DynamicObject is only a subset of the passed one if both are
    * Maps and the passed DynamicObject contains at least all of the members
    * and values of this DynamicObject.
    *
    * @param rhs the DynamicObject to check as a superset.
    *
    * @return true if this DynamicObject is a subset of the passed one.
    */
   virtual bool isSubset(const DynamicObject& rhs) const;

   /**
    * Get a simple description for the DynamicObjectType enumerated value.
    * Useful in error messages to describe valid or invalid input.
    *
    * @param type the type to describe.
    *
    * @return a simple string to describe each DynamicObject type.
    */
   static const char* descriptionForType(DynamicObjectType type);

   /**
    * Gets the appropriate type for the passed string.
    *
    * @param str the string value to determine the type for.
    *
    * @return the appropriate type.
    */
   static DynamicObjectType determineType(const char* str);
};

} // end namespace rt
} // end namespace monarch

/**
 * Compares a DynamicObject to a string for equality. If the
 * DynamicObject is not a string then the comparison will return false.
 *
 * @param lhs the string to compare to the DynamicObject.
 * @param rhs the DynamicObject to compare to the string.
 *
 * @return true if the passed string is equal to this one, false if not.
 */
bool operator==(const char* lhs, const monarch::rt::DynamicObject& rhs);

/**
 * Compares a DynamicObject to a string for non-equality. If the
 * DynamicObject is not a string then the comparison will return true.
 *
 * @param lhs the string to compare to the DynamicObject.
 * @param rhs the DynamicObject to compare to the string.
 *
 * @return true if the passed string is not equal to this one, false if not.
 */
bool operator!=(const char* lhs, const monarch::rt::DynamicObject& rhs);

/**
 * Compares a DynamicObject to a string. If the DynamicObject is a string
 * that is lexically less than the given string, then the method will
 * return true, otherwise it will return false.
 *
 * @param lhs the string to compare.
 * @param rhs the DynamicObject to compare.
 *
 * @return true if the DynamicObject is a string and the passed string is
 *         less than the DynamicObject, false otherwise.
 */
bool operator<(const char* lhs, const monarch::rt::DynamicObject& rhs);

/**
 * Compares a DynamicObject to a string. If the DynamicObject is a string
 * that is lexically less than or equal to the given string, then the method
 * will return true, otherwise it will return false.
 *
 * @param lhs the string to compare.
 * @param rhs the DynamicObject to compare.
 *
 * @return true if the DynamicObject is a string and the passed string is
 *         less than or equal to the DynamicObject, false otherwise.
 */
bool operator<=(const char* lhs, const monarch::rt::DynamicObject& rhs);

/**
 * Compares a DynamicObject to a string. If the DynamicObject is a string
 * that is lexically greater than the given string, then the method will
 * return true, otherwise it will return false.
 *
 * @param lhs the string to compare.
 * @param rhs the DynamicObject to compare.
 *
 * @return true if the DynamicObject is a string and the passed string is
 *         greater than the DynamicObject, false otherwise.
 */
bool operator>(const char* lhs, const monarch::rt::DynamicObject& rhs);

/**
 * Compares a DynamicObject to a string. If the DynamicObject is a string
 * that is lexically greater or equal to than the given string, then the
 * method will return true, otherwise it will return false.
 *
 * @param lhs the string to compare.
 * @param rhs the DynamicObject to compare.
 *
 * @return true if the DynamicObject is a string and the passed string is
 *         greater than or equal to the DynamicObject, false otherwise.
 */
bool operator>=(const char* lhs, const monarch::rt::DynamicObject& rhs);

#endif
