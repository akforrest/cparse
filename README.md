# Qt-CParser

[license-image]: http://img.shields.io/badge/license-MIT-blue.svg?style=flat
[license]: LICENSE.mit

This is a fork of https://github.com/cparse/cparse, with a Qt interface. CParse is an extendable expression parser based on Dijkstra's Shunting-yard algorithm.

A lot of has changed. In no particular order, and by no means an exhaustive list, the following has changed:

1. Massive code reoder and name changes. Class names are now properly camel cased, fixed a number of issues with forward declarations and over exposing the core API. Other types have been refactored to be better named. The .inc files have been renamed to be .h or .cpp files as appropriate. This means an IDE has half a chance to run a language parser on this project now, because there are no files with undeclared dependencies. This maybe now requires a compiler which supports C++17, but I am not sure (thats what I worked against).
2. The whole public facing API is now Qt-ified, escpecially with regards to QString. 
3. All exceptions have been removed, because exceptions are just the worst (along with all the usual actual technical reasons). There is now a TokenType::Error, PackTokens with this error code are returned when there is a compilation or evaluation error. Calculator::compiled() can also be used to check status when the compiling constructor is used. Empty expressions or any cases which result in the RPN engine returning an empty or erronous stack set compiled to false.
4. Added cparse.h, which has an initialize() function, rather than needing to possibly include a .inc file. This is possibly the only file you need to include to use this entire library now.
5. Added cparse.pri (in place of Makefile). Sources are compiled in, if you want this as a lib, youll have to write your own!
6. I got rid of catch for the tests, because it was too much hassle to keep it working with all the other changes. The unit tests still exist, there is a CONFIG option to include them, and then you have to call a function to run them. At some point I may fully convert them over to a proper QtTest.
7. Calculators can be created with a set config which they will use for any subsequent compilation and evaluation steps.

TODO: 
1. I want to be able to create calculator configs that have only a subset of operations defined, by some sort of logical grouping like number operators, boolean operators, set/list/map operators, math functions, dev functions (etc). 
2. Reorder and create a proper public API and private API (/src/ and /include/) to reduce dependencies and make it a bit clearer and easier to extend with custom types.
3. Start using smart pointers internally. exceptions were hiding a few memory leaks which I caught already but using unique_ptr et al would be much better.
