[Ler esse documento em português](README-pt.md)

# SiNUCA3

Third iteration of the Simulator of Non-Uniform Cache Architectures.

## Main authors

SiNUCA3 is developed by the following students of
[Dr. Marco Zanata](https://github.com/mazalves):

- [Gabriel G. de Brito](https://github.com/gboncoffee)
- [Vinícius Santos](https://github.com/Vini72SH)
- [Fernando de Barros Castro](https://github.com/ferbcastro)
- [Bruno Krügel](https://github.com/bk304)
- [Daniel Siqueira](https://github.com/Dalien-S)
- [Pedro Endrigo dos Santos](https://github.com/Endrigopedro)
- [Giuliano Tavares](https://github.com/GiuTP)

## Dependencies

SiNUCA3 depends on libyaml (`sudo apt install libyaml-dev` on Debian, Ubuntu and
derivatives).

## Building

Just `make` works.

We use a Makefile that automatically detects every source file in `src/` and
builds it to it's own object to `build/`, linking everything together as
`sinuca3`. Also, it'll use `clang` if you have it installed, but will fallback
to `gcc` or `cc` otherwise.

That said, you can build an optimized binary by just running "make" and a debug
binary by running "make debug". Both can coexist as the object names and the
binaries are different (debug ones have -debug appended to the name).

If you play with any header file, you'll unfortunatelly have to `make -B`
because our build system does not automatically rebuilds sources when their
headers have changed.

## Project structure

As said in building, you can just throw a .cpp or .c file inside `src/` and
it'll be picked up by the build system. For creating your own components, it may
be a good idea to keep things organized and put them into the
`src/custom_components/` directory.

You'll need to add your custom components to the
`src/custom_components/custom.cpp` file also.

Most of the API is accessible via `sinuca3.hpp`.

## Modularization and configuration schema

Simulation components are loosely defined as a stage of the pipeline. They
communicate via a message passing interface. Each cycle, a component can read
messages send to it by other components in the previous cycle. To create a
component, one's need to inherit from `Component<T>`, whereas T is the type of
message it receives.

To simulate a system, define it in a yaml configuration file.

When needing to actually reuse code, prefer to compose, I.e, add the specific
class as a private attribute and call it's methods.

## Developing, styleguide, guidelines, etc

We like to use modern tools like `clangd` and `lldb` and old standards: our C++
is C++98  and our C is C99. When adding new files, please be kind and run `bear
--append -- make debug` so `clangd` properly detects them. After editing, please
format everything with `clang-format`. A common trick is to run `find . -type f
-name "*.[cpp,hpp,c,h]" -exec clang-format -i {} \;` as this will format all
files.

Also, the project is documented with Doxygen, so remember of the documentation
comments.

We mostly follow [Google's C++ Style
Guide](https://google.github.io/styleguide/cppguide.html) with some extensions:

- As of code styling, we use the following `.clang-format`:  
  ```
  BasedOnStyle: Google
  IndentWidth: 4
  AccessModifierOffset: -2
  PointerAlignment: Left
  ```
- We use `.cpp` for C++ and `.hpp` for C++ headers. Similarly, the include
  guards ends with `HPP_`.
- Static storage duration still should be avoided but we're more permissive
  towards it then Google due to the nature of our project.
- *Usually* you should only inherit from virtual classes.
- Operator overloading is strictly forbidden.
- We do not use references, only raw pointers. Of course we do not use smart
  pointers and move semantics as we're stuck in C++98.
- We do not have `cpplint.py`.
- Don't use friend classes.
- Use `cstdio` instead of `iostream`.
- Use `NULL` for null-pointers as we're stuck in C++98.
- Don't use type deduction.
- No boost library is aprooved.
- Variables, attributes and namespaces are `camelCase` and without any prefix or
  suffix.
- Constants are `ALL_CAPS`, just like macros (that of course should be avoided).
- Enum names are `PascalCase` just like functions and types.
- In the overall, avoid complicating stuff.

HiPES is an inclusive place. Remember that.
