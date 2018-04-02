# MUSCLE client-side API support for other languages

* MUSCLE expects most server-side code will be written in C++ (or possibly in C)
* However, it's a fact that client-side software is often written in many different languages
* Therefore, MUSCLE tries to provide explicit client-side support APIs for as many languages as possible.
* MUSCLE-speaking programs in any language should be able to communicate seamlessly with each other (since they all use the same flattened-Message binary-communication-protocol)
* Currently, MUSCLE contains explicit support for the following languages:
    - C++03 and higher (with additional features enabled under C++11 and higher)
    - C (via the MicroMessage and MiniMessage APIs)
    - C# (see the muscle/charp folder)
    - Delphi (see the muscle/delphi folder)
    - Python (via the py files in the muscle/python folder)
    - Java (via the files in the muscle/java folder)
    - Java 2 Micro Edition (via the files in the muscle/java_j2me folder)
