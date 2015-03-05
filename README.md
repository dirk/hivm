# Hivm

[![Build Status](https://travis-ci.org/dirk/hivm.png?branch=master)](https://travis-ci.org/dirk/hivm)

For now you'll probably just want to read over the [notes](manual/notes.md) and [instruction set](manual/instructions.md).

## Getting started

Hivm uses the [Rake build tool](https://github.com/ruby/rake) which should be available with most Ruby installations or through Rubygems (via `[sudo] gem install rake`). Hivm also depends on a few other open-source libraries:

* [LLVM] around version 3.5: Compiler infrastructure used in the JIT compiler
* [GNOME GLib] version 2.*: Common C application library
* [Lua] version 5.1: Used by the debugger

[LLVM]: http://llvm.org/
[GNOME GLIB]: https://wiki.gnome.org/Projects/GLib
[Lua]: http://www.lua.org/

On Mac OS X these should be easily-installed via Homebrew:

```sh
brew install glib llvm lua51
```

You can then build the project by invoking Rake:

```sh
git clone https://github.com/dirk/hivm.git
cd hivm
rake # Will build the library, headers, and so forth
rake -T # Will show a list of all available tasks
```

## The Manifesto

Virtual machines have become a new layer of abstraction between the programmer and the machine their code runs on. The ecosystem of virtual machines is growing and the machines themselves are becoming increasingly more complex. Furthermore, virtual machines have almost always been closely bound to their "native tongue": the language they were originally designed to execute. Running "non-native" languages on these machines is cumbersome and often incurs a penalty in performance and/or functionality.

The Hivm project aims to overcome these and provide a new, better virtual machine for the execution of static, dynamic, and hybrid languages:

1. **No native tongue**: Hivm instead provides a solid, dependable, performant base for implementing languages.
2. **Minimize complexity**: bootstrapping a language, interacting with the machine, writing platform-native extensions, and the like should not be hard.
3. **Maximize usable performance**: the machine will not only perform well but also provide easy-to-use and powerful tools and APIs for understanding and optimizing how it performs.

## License

Licensed under the [Mozilla Public License Version 2.0](http://www.mozilla.org/MPL/2.0/). See [LICENSE](./LICENSE) for details.
