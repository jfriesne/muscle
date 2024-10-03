# MUSCLE client-side API support for other languages

* MUSCLE expects most server-side code will be written in C++ (or possibly in C)
* However, it's a fact that client-side software is often written in many different languages
* Therefore, MUSCLE tries to provide explicit client-side support APIs for as many languages as possible.
* MUSCLE-speaking programs in any language should be able to communicate seamlessly with each other (since they all use the same flattened-Message binary-communication-protocol)
* Currently, MUSCLE has explicit client-side support for the following languages:
    - C++03 and higher (with full features enabled under C++11 or later)
    - C (via the [MicroMessage](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/site/micromessage/) and [MiniMessage](https://public.msli.com/lcs/muscle/muscle/lang/c/minimessage/MiniMessage.h) APIs)
    - C# (see the `muscle/lang/csharp` folder)
    - Delphi (see the `muscle/lang/delphi` folder)
    - Python (via the .py files in the `muscle/lang/python2` folder or the `muscle/lang/python3` folder)
    - Java (via the files in the `muscle/lang/java` folder)
    - Java 2 Micro Edition (via the files in the `muscle/lang/java_j2me` folder)
* Support for other languages is straightforward to add if/when necessary.
