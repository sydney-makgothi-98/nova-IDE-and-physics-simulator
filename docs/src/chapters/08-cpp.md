# C++: C With Superpowers

## What C++ adds

C++ began as "C with classes" in 1979 and has grown into a huge language. It keeps C's speed and closeness to the machine, and adds tools to organise big programs: **classes** (your own types), **templates** (code that works for any type), a rich **standard library** (lists, maps, strings, algorithms), and **automatic resource handling**.

Nearly all C code is also valid C++. That is why you learn C first: everything in Chapter 10 still works. Now we build on top of it.

::: story In plain words
C gives you bricks. C++ gives you bricks **plus** ready-made walls, doors and windows, and a foreman (the destructor) who tidies up after you.
:::

NOVA's own sample project, the one in the screenshots, is C++. Open `hello_nova` in NOVA while you read: you can run, step through and change every example.

## Hello, C++

```cpp run
#include <iostream>

int main() {
    std::cout << "Hello, NOVA!" << std::endl;
    int a = 6, b = 7;
    std::cout << a << " times " << b << " is " << a * b << '\n';
    return 0;
}
```

* `std::cout` is the **console output stream**. The `<<` operator sends things into it, one after another.
* `std::` means "this name lives in the **standard** namespace". A **namespace** is a labelled box of names so that two libraries can both have a `sort` without clashing.
* `std::endl` ends the line and flushes the output. A plain `'\n'` ends the line and is faster.
* `main` does not need `return 0;` in C++ (it is assumed), but it is clear to write it.

::: why Why std:: and not using namespace std?
Writing `using namespace std;` saves typing, but it dumps thousands of names into your file. If you also define something called `count` or `list` you get baffling errors. Professional code uses the `std::` prefix (or imports one name at a time). NOVA's own sample code does the same.
:::

## Everyday tools

### References: a nickname for a variable

```cpp run
#include <iostream>

void addOne(int& n) { n += 1; }               // & means: work on the ORIGINAL
void show(const int& n) { std::cout << n << '\n'; }   // const&: read-only, no copy

int main() {
    int x = 5;
    addOne(x);
    int& alias = x;                           // alias is another name for x
    alias = 100;
    show(x);
    return 0;
}
```

A **reference** is a pointer wearing a friendlier face: it is automatically dereferenced and can never be null. Use `const T&` to pass big things (such as strings) without copying them.

### auto and range-for

```cpp run
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::vector<std::string> parts = {"editor", "tree", "terminal"};
    for (const auto& p : parts) std::cout << p << ' ' << p.size() << '\n';
    auto total = 0;
    for (auto& p : parts) { p += "!"; total += static_cast<int>(p.size()); }
    std::cout << parts.back() << ' ' << total << '\n';
    return 0;
}
```

* `auto` lets the compiler work out the type.
* `for (const auto& p : parts)` visits every element. With `&` there is no copy; with `const` you promise not to change it. NOVA's sample project uses exactly this loop to add up circle areas.

### Strings

`std::string` replaces C's fragile character arrays. It grows by itself and knows its own length.

```cpp run
#include <iostream>
#include <string>

int main() {
    std::string s = "NOVA";
    s += " IDE";
    std::cout << s << ' ' << s.size() << ' ' << s.substr(0, 4) << '\n';
    std::cout << (s.find("IDE") != std::string::npos) << ' ' << s.find('V') << '\n';
    s.replace(0, 4, "Nova");
    std::cout << s << ' ' << (s == "Nova IDE") << ' ' << std::to_string(42) + "!" << '\n';
    std::cout << std::stoi("123") + 1 << '\n';
    return 0;
}
```

::: why Why is std::string safer than char arrays?
There is no null terminator to forget and no buffer to overflow: when you append, the string allocates more memory itself. The whole class of bugs warned about in Chapter 10 mostly vanishes. It is a perfect example of C++'s motto: *make the safe thing the easy thing*.
:::

## The standard library containers

| Container | What it is | Good at | Slow at |
|---|---|---|---|
| `std::vector<T>` | Growable array | Index access, adding at the end | Inserting in the middle |
| `std::array<T,N>` | Fixed-size array | Speed, no allocation | Cannot grow |
| `std::list<T>` | Linked list | Insert/remove anywhere | Index access |
| `std::map<K,V>` | Sorted key to value | Lookups in order | Slower than hash |
| `std::unordered_map<K,V>` | Hash table | Very fast lookups | No ordering |
| `std::set<T>` | Sorted unique items | "Have I seen this?" | |
| `std::queue`, `std::stack` | First in/first out, last in/first out | | |

```cpp run
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>

int main() {
    std::vector<int> v = {5, 2, 9, 1, 7};
    v.push_back(3);
    std::sort(v.begin(), v.end());
    for (int n : v) std::cout << n << ' ';
    std::cout << '\n';

    std::cout << std::accumulate(v.begin(), v.end(), 0) << ' '
              << *std::max_element(v.begin(), v.end()) << ' '
              << std::count_if(v.begin(), v.end(), [](int n) { return n % 2 == 1; }) << '\n';

    std::map<std::string, int> lines = {{"main.cpp", 120}, {"util.cpp", 45}};
    lines["app.cpp"] = 300;
    for (const auto& [file, n] : lines) std::cout << file << "=" << n << ' ';
    std::cout << '\n';

    std::set<int> unique(v.begin(), v.end());
    std::cout << unique.size() << ' ' << unique.count(7) << ' ' << unique.count(8) << '\n';
    return 0;
}
```

### The vector growth you built in C

Remember the `IntList` from the C chapter? `std::vector` is that, perfected. It doubles its capacity when full (see the growth figure in Chapter 10). You can watch it:

```cpp run
#include <iostream>
#include <vector>

int main() {
    std::vector<int> v;
    size_t last = v.capacity();
    std::cout << "cap " << last << '\n';
    for (int i = 0; i < 20; i++) {
        v.push_back(i);
        if (v.capacity() != last) { last = v.capacity(); std::cout << "size " << v.size() << " -> cap " << last << '\n'; }
    }
    return 0;
}
```

### Lambdas

A **lambda** is a small unnamed function you write on the spot. `[](int n) { return n % 2 == 1; }` above is one. The square brackets choose what the lambda may use from outside:

```cpp run
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

int main() {
    int threshold = 3;
    std::vector<std::string> words = {"a", "abcd", "ab", "abcde", "abc"};
    std::sort(words.begin(), words.end(), [](const auto& x, const auto& y) { return x.size() < y.size(); });
    for (auto& w : words) std::cout << w << ' ';
    std::cout << '\n';
    auto longer = std::count_if(words.begin(), words.end(), [threshold](const std::string& w) { return (int)w.size() > threshold; });
    std::cout << longer << '\n';
    return 0;
}
```

`[threshold]` **captures** the variable by copy; `[&threshold]` would capture a reference; `[=]` and `[&]` capture everything.

## Classes: your own types

A **class** bundles data (member variables) with the functions that work on it (member functions). It is the most important feature of C++. This is the very class from NOVA's sample project, now complete.

```cpp run
#include <iostream>

class Circle {
public:
    explicit Circle(double r) : radius_(r) {}          // constructor
    double area() const { return 3.14159265 * radius_ * radius_; }
    void grow(double by) { radius_ += by; }
    double radius() const { return radius_; }

private:
    double radius_;                                    // hidden from outside
};

int main() {
    Circle c(2.0);
    std::cout << c.area() << '\n';
    c.grow(1.0);
    std::cout << c.radius() << ' ' << c.area() << '\n';
    // c.radius_ = -5;      // ERROR: radius_ is private
    return 0;
}
```

Reading it:

* `public:` members can be used by anyone; `private:` members only by the class itself.
* The **constructor** `Circle(double r)` runs when an object is created. `: radius_(r)` is the **initializer list**: it sets the member directly.
* `const` after `area()` is a promise: *this function will not change the object*. A `const Circle` can only call `const` functions.
* `explicit` stops accidental conversions such as `Circle c = 5;`.

::: why Why make radius_ private?
So nobody can set it to an impossible value like -5 behind the class's back. If the only way to change the radius is through `grow`, then `grow` can check the rules. This is **encapsulation**: the class guards its own data. Later, if you store the radius differently, no outside code needs to change.
:::

::: whatif What if area() were not marked const?
Then you could not call it on a `const Circle&`, which is what you get when you write `for (const auto& c : circles)`. The compiler would refuse with an error about discarding qualifiers. Marking everything that does not modify state as `const` is a habit worth forming from day one.
:::

### Constructors and destructors: birth and death

```cpp run
#include <iostream>
#include <string>

class Noisy {
public:
    explicit Noisy(std::string n) : name_(std::move(n)) { std::cout << "  build " << name_ << '\n'; }
    ~Noisy() { std::cout << "  destroy " << name_ << '\n'; }
private:
    std::string name_;
};

int main() {
    std::cout << "start\n";
    Noisy a("A");
    {
        Noisy b("B");
        std::cout << "inside block\n";
    }
    Noisy c("C");
    std::cout << "end\n";
    return 0;
}
```

Run it and study the order. Objects are destroyed **in reverse order** of creation, and **automatically** at the closing brace of their scope. Nobody has to remember to call anything.

## RAII: the idea that defines C++

**RAII** stands for *Resource Acquisition Is Initialization*. It is a clumsy name for a simple, powerful idea: **tie the life of a resource (a file, memory, a lock) to the life of an object**. The constructor takes the resource and the destructor gives it back.

{{fig:raii-timeline|With RAII the cleanup is guaranteed, however the block ends.}}

```cpp run
#include <cstdio>
#include <iostream>
#include <stdexcept>

class File {
public:
    File(const char* path, const char* mode) : f_(std::fopen(path, mode)) {
        if (!f_) throw std::runtime_error("cannot open file");
    }
    ~File() { if (f_) { std::fclose(f_); std::cout << "[file closed]\n"; } }
    File(const File&) = delete;                  // forbid copying (two owners would double-close)
    File& operator=(const File&) = delete;
    void write(const char* text) { std::fputs(text, f_); }
private:
    std::FILE* f_;
};

int main() {
    try {
        File f("raii_demo.txt", "w");
        f.write("hello\n");
        throw std::runtime_error("something went wrong halfway");
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }
    std::remove("raii_demo.txt");
    return 0;
}
```

Notice that although an error was thrown, the line `[file closed]` is printed: the destructor still ran, because leaving the block, even by exception, destroys its objects.

::: whatif What if you used fopen and fclose by hand?
You would need an `fclose` on every possible path out of the function: normal return, early return, and every exception. Forget one and you leak the file handle. In big programs this is unmanageable. RAII reduces it to *nothing to remember*.
:::

### Smart pointers

Raw `new` and `delete` are RAII's enemy: forgetting `delete` leaks. **Smart pointers** wrap them in objects that delete automatically.

```cpp run
#include <iostream>
#include <memory>
#include <vector>

struct Tab {
    explicit Tab(int id) : id(id) { std::cout << "open tab " << id << '\n'; }
    ~Tab() { std::cout << "close tab " << id << '\n'; }
    int id;
};

int main() {
    auto a = std::make_unique<Tab>(1);          // exactly ONE owner
    std::cout << a->id << '\n';
    auto b = std::move(a);                      // ownership moves; a is now empty
    std::cout << (a == nullptr) << ' ' << b->id << '\n';

    auto s1 = std::make_shared<Tab>(2);         // SHARED ownership, counted
    {
        auto s2 = s1;
        std::cout << "owners: " << s1.use_count() << '\n';
    }
    std::cout << "owners: " << s1.use_count() << '\n';

    std::vector<std::unique_ptr<Tab>> tabs;
    tabs.push_back(std::make_unique<Tab>(3));
    std::cout << "leaving main\n";
    return 0;
}
```

| Type | Meaning | Use it when |
|---|---|---|
| `std::unique_ptr<T>` | One owner, deleted when it goes away | **Default choice** |
| `std::shared_ptr<T>` | Many owners, deleted when the last one goes away | You truly need shared ownership |
| `T&` or `T*` (raw) | "Just looking, not owning" | Passing things around without ownership |

::: why Why is unique_ptr the default?
Single ownership is simple to reason about: the owner decides when the object dies. `shared_ptr` costs a counter and makes lifetimes harder to see. Modern C++ code almost never contains a bare `new` or `delete`.
:::

## Inheritance and polymorphism

A class can **inherit** from another: it gets all its members and adds more. With **virtual functions** a base-class pointer can run the right version for the real object. This is **polymorphism**.

{{fig:vtable|A virtual call looks inside the object for a hidden table of functions, so p->area() runs the Circle version even through a Shape pointer.}}

```cpp run
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

class Shape {
public:
    virtual ~Shape() = default;                 // always make base destructors virtual
    virtual double area() const = 0;            // "= 0": pure virtual, must be overridden
    virtual const char* name() const = 0;
};

class Circle : public Shape {
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
    const char* name() const override { return "circle"; }
private:
    double r_;
};

class Rect : public Shape {
public:
    Rect(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    const char* name() const override { return "rectangle"; }
private:
    double w_, h_;
};

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(1.0));
    shapes.push_back(std::make_unique<Rect>(3.0, 4.0));
    shapes.push_back(std::make_unique<Circle>(2.0));

    double total = 0;
    for (const auto& s : shapes) {
        std::cout << s->name() << ": " << s->area() << '\n';
        total += s->area();
    }
    std::cout << "total: " << total << '\n';
    return 0;
}
```

::: why Why virtual ~Shape()?
When a `unique_ptr<Shape>` deletes a `Circle` through a `Shape` pointer, only a **virtual** destructor makes C++ run the `Circle` destructor. Without it the Circle part is never cleaned up properly: undefined behaviour. Rule: *a class meant to be a base needs a virtual destructor.*
:::

::: whatif What if you write override and misspell the name?
`override` tells the compiler "this must replace a base function". If you write `aera()` or change a `const`, the compiler stops with an error. Without `override` you would silently create a *new* function and the base one would never be replaced. It is a tiny keyword that prevents a very sneaky bug.
:::

## Operator overloading

You can teach your own types to use `+`, `==`, `<<` and so on.

```cpp run
#include <iostream>

struct Vec2 {
    double x = 0, y = 0;
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator*(double k) const { return {x * k, y * k}; }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};
std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << '(' << v.x << ", " << v.y << ')'; }

int main() {
    Vec2 a{1, 2}, b{3, 4};
    std::cout << a + b << ' ' << a * 3 << ' ' << (a == b) << '\n';
    return 0;
}
```

## Templates: code for any type

A **template** is a recipe the compiler uses to write code for each type you need.

```cpp run
#include <iostream>
#include <string>
#include <vector>

template <typename T>
T biggest(const T& a, const T& b) { return a < b ? b : a; }

template <typename T>
class Stack {
public:
    void push(const T& v) { items_.push_back(v); }
    T pop() { T v = items_.back(); items_.pop_back(); return v; }
    bool empty() const { return items_.empty(); }
    size_t size() const { return items_.size(); }
private:
    std::vector<T> items_;
};

int main() {
    std::cout << biggest(3, 9) << ' ' << biggest(2.5, 1.5) << ' ' << biggest(std::string("pear"), std::string("apple")) << '\n';
    Stack<std::string> history;
    history.push("main.cpp");
    history.push("util.cpp");
    std::cout << history.pop() << ' ' << history.size() << '\n';
    return 0;
}
```

::: why Why templates instead of writing three versions?
`biggest` for `int`, `double` and `string` would be three copies of the same logic, and a bug fixed in one would stay in the others. A template is written once, checked once, and compiled into exactly the versions you use, with **no speed penalty**. `std::vector`, `std::map` and all the containers are templates.
:::

## Errors: exceptions

```cpp run
#include <iostream>
#include <stdexcept>
#include <string>

int parsePort(const std::string& s) {
    int n = std::stoi(s);                     // throws if it is not a number
    if (n < 1 || n > 65535) throw std::out_of_range("port out of range: " + s);
    return n;
}

int main() {
    for (std::string s : {"8080", "99999", "abc"}) {
        try {
            std::cout << "port " << parsePort(s) << '\n';
        } catch (const std::out_of_range& e) {
            std::cout << "range error: " << e.what() << '\n';
        } catch (const std::exception& e) {
            std::cout << "other error: " << e.what() << '\n';
        }
    }
    return 0;
}
```

## Move semantics, briefly

Copying a big object is slow. When the original is about to be thrown away anyway, C++ lets you **move**: steal its insides instead of copying them.

```cpp run
#include <iostream>
#include <string>
#include <utility>
#include <vector>

struct Buffer {
    std::vector<int> data;
    explicit Buffer(size_t n) : data(n, 7) { std::cout << "created " << n << '\n'; }
    Buffer(const Buffer& o) : data(o.data) { std::cout << "COPY\n"; }
    Buffer(Buffer&& o) noexcept : data(std::move(o.data)) { std::cout << "move\n"; }
};

Buffer make() { return Buffer(1000); }

int main() {
    Buffer a(1000);
    Buffer b = a;                    // copies: a is still needed
    Buffer c = std::move(a);         // moves: a gives up its insides
    std::cout << a.data.size() << ' ' << c.data.size() << '\n';
    Buffer d = make();               // returned objects are moved (or built in place)
    std::string s = "text";
    std::string t = std::move(s);
    std::cout << t << ' ' << s.size() << '\n';
    return 0;
}
```

After a move, the old object is left empty but valid. This is why `return` of big vectors and strings from functions is cheap in modern C++.

## Modern C++ goodies

```cpp run
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <map>

std::optional<int> findLine(const std::map<std::string, int>& m, const std::string& k) {
    auto it = m.find(k);
    if (it == m.end()) return std::nullopt;     // "no value"
    return it->second;
}

constexpr int square(int x) { return x * x; }   // can run at compile time
static_assert(square(5) == 25);

int main() {
    std::map<std::string, int> lines = {{"main.cpp", 120}};
    if (auto n = findLine(lines, "main.cpp")) std::cout << "found " << *n << '\n';
    std::cout << findLine(lines, "none").value_or(-1) << '\n';

    std::variant<int, std::string> v = 42;      // holds ONE of several types
    v = std::string("now text");
    std::visit([](const auto& x) { std::cout << "variant: " << x << '\n'; }, v);

    std::string_view sv = "a view, no copy";    // a cheap look at existing text
    std::cout << sv.substr(2, 4) << ' ' << square(9) << '\n';

    auto [lo, hi] = std::pair{3, 8};            // structured bindings
    std::cout << lo + hi << '\n';
    return 0;
}
```

## Threads, in one example

```cpp run
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    int counter = 0;
    std::mutex m;
    auto work = [&] {
        for (int i = 0; i < 1000; i++) {
            std::lock_guard<std::mutex> lock(m);     // RAII lock: released at the closing brace
            ++counter;
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < 4; t++) pool.emplace_back(work);
    for (auto& t : pool) t.join();
    std::cout << "counter = " << counter << '\n';
    return 0;
}
```

::: whatif What if there were no mutex?
Four threads would read, add and write `counter` at the same time and overwrite each other's work. The result would usually be less than 4000 and different every run: a **race condition**. The mutex lets only one thread in at a time. `lock_guard` is RAII again: the lock is released at the closing brace even if an exception is thrown.
:::

## Worked project: a C++ tokenizer

NOVA colours your code with Monaco. But how would you do it yourself? The first step is a **tokenizer**: cut the text into pieces and label each one. This program does exactly that, using classes, enums, vectors, sets and strings from this chapter.

```cpp run
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

enum class Kind { Keyword, Identifier, Number, String, Comment, Symbol };

const char* name(Kind k) {
    switch (k) {
        case Kind::Keyword: return "keyword";
        case Kind::Identifier: return "identifier";
        case Kind::Number: return "number";
        case Kind::String: return "string";
        case Kind::Comment: return "comment";
        case Kind::Symbol: return "symbol";
    }
    return "?";
}

struct Token { Kind kind; std::string text; };

std::vector<Token> tokenize(const std::string& src) {
    static const std::set<std::string> keywords = {"int", "return", "if", "else", "for", "while", "void", "double", "class", "const", "auto"};
    std::vector<Token> out;
    size_t i = 0;
    while (i < src.size()) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (std::isspace(c)) { ++i; continue; }
        size_t start = i;
        if (std::isalpha(c) || c == '_') {
            while (i < src.size() && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) ++i;
            std::string word = src.substr(start, i - start);
            out.push_back({keywords.count(word) ? Kind::Keyword : Kind::Identifier, word});
        } else if (std::isdigit(c)) {
            while (i < src.size() && (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) ++i;
            out.push_back({Kind::Number, src.substr(start, i - start)});
        } else if (c == '"') {
            ++i;
            while (i < src.size() && src[i] != '"') { if (src[i] == '\\') ++i; ++i; }
            ++i;
            out.push_back({Kind::String, src.substr(start, i - start)});
        } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            out.push_back({Kind::Comment, src.substr(start, i - start)});
        } else {
            ++i;
            out.push_back({Kind::Symbol, std::string(1, static_cast<char>(c))});
        }
    }
    return out;
}

int main() {
    const std::string code = "int total = 42; // the answer\nreturn compute(total, \"hi\");";
    std::map<std::string, int> counts;
    for (const auto& t : tokenize(code)) {
        std::cout << name(t.kind) << "\t" << t.text << '\n';
        ++counts[name(t.kind)];
    }
    std::cout << "---\n";
    for (const auto& [k, n] : counts) std::cout << k << ": " << n << '\n';
    return 0;
}
```

::: why Why enum class and not a plain enum?
A plain `enum` leaks its names into the surrounding scope (`Keyword` would be visible everywhere) and converts silently to an `int`. An `enum class` keeps names inside (`Kind::Keyword`) and refuses silent conversion. Fewer clashes and fewer accidents.
:::

## Reading C++ error messages

C++ errors can be long, especially from templates. Use this recipe:

1. Read only the **first** error. Later ones are often fallout.
2. Find the file and line (NOVA underlines it and lists it in Problems).
3. Read the first sentence: it usually says exactly what is wrong (`'cnt' was not declared`, `expected ';'`).
4. For a template error, look for the line in **your** file inside the long chain, labelled `required from here`.
5. Fix one thing, rebuild, repeat.

## Mini assignments

::: assign Assignment 11.1: Word frequency
Read a sentence into a `std::string`, split it into words, count them with a `std::map<std::string,int>` (ignoring case), and print them sorted by count (highest first). *Hint: copy the map into a vector of pairs and `std::sort` with a lambda.*
:::

::: assign Assignment 11.2: A BankAccount class
Write `BankAccount` with a private balance, `deposit`, `withdraw` (throw `std::runtime_error` if the money is not there) and a `const` `balance()`. Test it with a try/catch.
:::

::: assign Assignment 11.3: Shapes
Add a `Triangle` to the shapes example. Add a `perimeter()` virtual function to all shapes. Print the shape with the largest area using `std::max_element` and a lambda.
:::

::: assign Assignment 11.4: Generic Queue
Write `template <typename T> class Queue` with `push`, `pop`, `front` and `size`. Use `std::deque` inside. Test it with `int` and `std::string`.
:::

::: assign Assignment 11.5: Prove RAII
Write a `Timer` class that records the time in its constructor and prints the elapsed milliseconds in its destructor. Put one at the top of a function to time it automatically.
:::

## Big assignment: A source-code outline tool

::: big "Outline": a mini version of NOVA's Structure panel
**Goal.** Write a command-line program that reads a C++ source file and prints an outline: the classes and functions it contains with their line numbers, plus a summary table.

**Example output.**

```text
class Circle                     line 4
  function area()                line 7
  function grow(double)          line 8
function main()                  line 25
Summary: 1 class, 3 functions, 62 lines, 11 comments
```

**Requirements.**

1. Read a file name from `argv[1]`. Read the whole file with `std::ifstream` (RAII will close it).
2. Reuse the **tokenizer** from this chapter and extend it to track the **line number** of every token and to recognise multi-line `/* ... */` comments and `#` preprocessor lines.
3. Write a second stage, `outline(tokens)`, that finds `class`/`struct` names (the identifier after the keyword) and **functions** (an identifier followed by `(` at brace depth 0 or 1 where the matching `)` is followed by `{` or `const {`). Track brace depth by counting `{` and `}`.
4. Store results in a `struct Symbol { std::string kind, name; int line; int depth; }` held in a `std::vector`.
5. Print with `std::setw` for aligned columns, indented by depth.
6. Use at least: one class, one `enum class`, `std::map` for the summary counts, one lambda, `const&` parameters and range-for.
7. Handle errors (missing file, no file argument) with exceptions or clear messages and a non-zero exit code.
8. Split into `lexer.h/.cpp`, `outline.h/.cpp` and `main.cpp`, and build with a CMake project (Chapter 12). Run it in NOVA on **its own source files**.
9. No `new`/`delete` anywhere and no compiler warnings under `-Wall -Wextra`.

**Stretch goals.** Output JSON instead of plain text (the format NOVA's language server uses). Skip functions inside string literals and comments. Add `--sort name` to sort the outline. Compare your result to the Structure panel in NOVA for the same file and find where clangd, which really understands C++, differs from your simple version. That gap is why real tools use a proper parser.

**How to check yourself.** Run it on `hello_nova/src/main.cpp` and on `shapes.cpp`. The functions should match those in NOVA's Structure panel.
:::

## Quiz

::: quiz
1. Why prefer `std::cout` with the `std::` prefix over `using namespace std;`?
2. What does `const` after a member function's parameter list promise?
3. What is a constructor's initializer list for?
4. What is RAII in one sentence?
5. Why does `std::unique_ptr` make `delete` unnecessary?
6. When do you use `shared_ptr` instead of `unique_ptr`?
7. What does `virtual` do and why must a base class destructor be virtual?
8. What does `override` protect you from?
9. What is a template and why is it better than copy-pasting a function?
10. What is a race condition and how does a mutex help?
11. What does `std::move` actually do?
12. Why is `enum class` safer than plain `enum`?
:::

::: answers
1. It avoids dumping thousands of names into your file, which can clash with your own names.
2. That the function does not change the object, so it can be called on const objects.
3. To initialise member variables directly when the object is created.
4. Tie a resource's lifetime to an object so that the destructor releases it automatically, however the scope ends.
5. The unique_ptr's destructor deletes the object when the pointer goes out of scope.
6. When several owners truly must share one object and it should die when the last owner is gone.
7. It chooses the function by the real object type at run time. A virtual base destructor makes deleting through a base pointer run the derived destructor too.
8. Silently creating a new function instead of replacing the base version because of a typo or mismatched signature.
9. A recipe the compiler uses to generate code for each type; one tested version replaces many copies.
10. Several threads changing the same data at once give wrong, varying results; a mutex lets one thread in at a time.
11. It casts to an rvalue so that move constructors/assignments may steal the contents; it does not move anything by itself.
12. Its names stay inside the enum's scope and it does not convert silently to integers.
:::
