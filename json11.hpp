/* json11
 *
 * json11 is a tiny JSON library for C++11, providing JSON parsing and serialization.
 *
 * The core object provided by the library is json11::Json. A Json object represents any JSON
 * value: null, bool, number (int or double), string (std::string), array (std::vector), or
 * object (std::map).
 *
 * Json objects act like values: they can be assigned, copied, moved, compared for equality or
 * order, etc. There are also helper methods Json::dump, to serialize a Json to a string, and
 * Json::parse (static) to parse a std::string as a Json object.
 *
 * Internally, the various types of Json object are represented by the JsonValue class
 * hierarchy.
 *
 * A note on numbers - JSON specifies the syntax of number formatting but not its semantics,
 * so some JSON implementations distinguish between integers and floating-point numbers, while
 * some don't. In json11, we choose the latter. Because some JSON implementations (namely
 * Javascript itself) treat all numbers as the same type, distinguishing the two leads
 * to JSON that will be *silently* changed by a round-trip through those implementations.
 * Dangerous! To avoid that risk, json11 stores all numbers as double internally, but also
 * provides integer helpers.
 *
 * Fortunately, double-precision IEEE754 ('double') can precisely store any integer in the
 * range +/-2^53, which includes every 'int' on most systems. (Timestamps often use int64
 * or long long to avoid the Y2038K problem; a double storing microseconds since some epoch
 * will be exact for +/- 275 years.)
 */

/* Copyright (c) 2013 Dropbox, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>

namespace json11 {

class Json;
class JsonValue;

namespace detail
{

/* A helper class for implementing the traits below. */
struct TraitsHelpers
{
    static std::false_type test_member_encode(...);
    static std::false_type test_free_encode(...);
    static std::false_type test_member_decode(...);
    static std::false_type test_free_decode(...);

    template<class S>
    static std::true_type test_member_encode(
        const S&,
        decltype(std::declval<S>().to_json())* = 0
    );

    template<class S>
    static std::true_type test_free_encode(
        const S&,
        decltype(to_json(std::declval<S>()))* = 0
    );

    template<class S>
    static std::true_type test_member_decode(
        const S&,
        decltype(S::from_json(std::declval<Json>()))* = 0
    );

    template<class S>
    static std::true_type test_free_decode(
        const S&,
        decltype(from_json(std::declval<Json>(), std::declval<S&>()))* = 0
    );
    
};

/* Inherits from std::true_type if t.to_json() is a valid expression. */
template<class T>
struct has_member_to_json : 
    decltype(TraitsHelpers::test_member_encode(std::declval<T>()))
{
};

/* Inherits from std::true_type if to_json(t) is a valid expression. */
template<class T>
struct has_free_to_json : 
    decltype(TraitsHelpers::test_free_encode(std::declval<T>()))
{
};

/*
 * Inherits from std::true_type if to_json(t) is valid but t.to_json()
 * is not.
 */
template<class T>
struct has_only_free_to_json : 
    std::integral_constant<
        bool, 
        !has_member_to_json<T>::value && has_free_to_json<T>::value
    >
{ };

template<class T>
struct has_to_json : 
    std::integral_constant<
        bool,
        has_member_to_json<T>::value || has_free_to_json<T>::value
    >
{ };

/* 
 * Inherits from std::true_type if T::from_json(Json()) is 
 * a valid expression. 
 */
template<class T>
struct has_member_from_json :
    decltype(TraitsHelpers::test_member_decode(std::declval<T>()))
{
};

/* 
 * Inherits from std::true_type if from_json(Json(), t) is 
 * a valid expression. 
 */
template<class T>
struct has_free_from_json :
    decltype(TraitsHelpers::test_free_decode(std::declval<T>()))
{
};

/* 
 * Inherits from std::true_type if from_json(Json(), t) is valid
 * but t.from_json(Json()) is not.
 */
template<class T>
struct has_only_free_from_json : 
    std::integral_constant<
        bool,
        !has_member_from_json<T>::value && has_free_from_json<T>::value
    >
{ };

template<class T>
struct has_from_json : 
    std::integral_constant<
        bool,
        has_member_from_json<T>::value || has_free_from_json<T>::value
    >
{ };

}

class Json final {
public:
    // Types
    enum Type {
        NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
    };

    // Array and object typedefs
    typedef std::vector<Json> array;
    typedef std::map<std::string, Json> object;

    // Constructors for the various types of JSON value.
    Json() noexcept;                // NUL
    Json(std::nullptr_t) noexcept;  // NUL
    Json(double value);             // NUMBER
    Json(int value);                // NUMBER
    Json(bool value);               // BOOL
    Json(const std::string &value); // STRING
    Json(std::string &&value);      // STRING
    Json(const char * value);       // STRING
    Json(const array &values);      // ARRAY
    Json(array &&values);           // ARRAY
    Json(const object &values);     // OBJECT
    Json(object &&values);          // OBJECT
   
    // Implicit constructor: anything with a to_json() member function.
    template<class T, typename std::enable_if<
        detail::has_member_to_json<T>::value, int>::type = 0>
    Json(const T& t) : Json(t.to_json()) { }
    
    // Implicit constructor: anything with only a to_json() free function.
    template<class T, typename std::enable_if<
        detail::has_only_free_to_json<T>::value, int>::type = 0>
    Json(const T& t) : Json(to_json(t)) { }
    
    // Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
    template <class M, typename std::enable_if<
        std::is_constructible<std::string, typename M::key_type>::value
        && std::is_constructible<Json, typename M::mapped_type>::value,
            int>::type = 0>
    Json(const M & m) : Json(object(m.begin(), m.end())) {}

    // Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
    template <class V, typename std::enable_if<
        std::is_constructible<Json, typename V::value_type>::value &&
        !detail::has_to_json<V>::value,
            int>::type = 0>
    Json(const V & v) : Json(array(v.begin(), v.end())) {}

    // This prevents Json(some_pointer) from accidentally producing a bool. Use
    // Json(bool(some_pointer)) if that behavior is desired.
    Json(void *) = delete;

    // Accessors
    Type type() const;

    bool is_null()   const { return type() == NUL; }
    bool is_number() const { return type() == NUMBER; }
    bool is_bool()   const { return type() == BOOL; }
    bool is_string() const { return type() == STRING; }
    bool is_array()  const { return type() == ARRAY; }
    bool is_object() const { return type() == OBJECT; }

    // Return the enclosed value if this is a number, 0 otherwise. Note that json11 does not
    // distinguish between integer and non-integer numbers - number_value() and int_value()
    // can both be applied to a NUMBER-typed object.
    double number_value() const;
    int int_value() const;

    // Return the enclosed value if this is a boolean, false otherwise.
    bool bool_value() const;
    // Return the enclosed string if this is a string, "" otherwise.
    const std::string &string_value() const;
    // Return the enclosed std::vector if this is an array, or an empty vector otherwise.
    const array &array_items() const;
    // Return the enclosed std::map if this is an object, or an empty map otherwise.
    const object &object_items() const;

    // Return a reference to arr[i] if this is an array, Json() otherwise.
    const Json & operator[](size_t i) const;
    // Return a reference to obj[key] if this is an object, Json() otherwise.
    const Json & operator[](const std::string &key) const;

    // Serialize.
    void dump(std::string &out) const;
    std::string dump() const {
        std::string out;
        dump(out);
        return out;
    }

    // Parse. If parse fails, return Json() and assign an error message to err.
    static Json parse(const std::string & in, std::string & err);
    static Json parse(const char * in, std::string & err) {
        if (in) {
            return parse(std::string(in), err);
        } else {
            err = "null input";
            return nullptr;
        }
    }
    // Parse multiple objects, concatenated or separated by whitespace
    static std::vector<Json> parse_multi(const std::string & in, std::string & err);

    /* Converts a Json object to type T if T has a from_json method. */
    template<class T>
    T as(typename std::enable_if<
            detail::has_member_from_json<T>::value
        >::type* = 0) const
    {
        return T::from_json(*this);
    }

    /* 
     * Converts a Json object to type T if from_json(Json(), some_t) exists and
     * T is default constructible.
     */
    template<class T>
    T as(typename std::enable_if<
            detail::has_only_free_from_json<T>::value
        >::type* = 0
    ) const
    {
        T result;
        from_json(*this, result);
        return result;
    }

    template<
        class T,
        class = typename std::enable_if<!detail::has_from_json<T>::value>::type
    >
    T as(decltype(std::declval<T>().begin())* = 0) const
    {
        typedef decltype(std::declval<T>().begin()) It;
        typedef typename std::iterator_traits<It>::value_type S;
        T result;
        for (auto js : array_items())
        {
            result.push_back(js.as<S>());
        }
        return result;
    }
    
    bool operator== (const Json &rhs) const;
    bool operator<  (const Json &rhs) const;
    bool operator!= (const Json &rhs) const { return !(*this == rhs); }
    bool operator<= (const Json &rhs) const { return !(rhs < *this); }
    bool operator>  (const Json &rhs) const { return  (rhs < *this); }
    bool operator>= (const Json &rhs) const { return !(*this < rhs); }

    /* has_shape(types, err)
     *
     * Return true if this is a JSON object and, for each item in types, has a field of
     * the given type. If not, return false and set err to a descriptive message.
     */
    typedef std::initializer_list<std::pair<std::string, Type>> shape;
    bool has_shape(const shape & types, std::string & err) const;

private:
    std::shared_ptr<JsonValue> m_ptr;
};

// Internal class hierarchy - JsonValue objects are not exposed to users of this API.
class JsonValue {
protected:
    friend class Json;
    friend class JsonInt;
    friend class JsonDouble;
    virtual Json::Type type() const = 0;
    virtual bool equals(const JsonValue * other) const = 0;
    virtual bool less(const JsonValue * other) const = 0;
    virtual void dump(std::string &out) const = 0;
    virtual double number_value() const;
    virtual int int_value() const;
    virtual bool bool_value() const;
    virtual const std::string &string_value() const;
    virtual const Json::array &array_items() const;
    virtual const Json &operator[](size_t i) const;
    virtual const Json::object &object_items() const;
    virtual const Json &operator[](const std::string &key) const;
    virtual ~JsonValue() {}
};

} // namespace json11
