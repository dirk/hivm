
$LOAD_PATH.push File.expand_path(__dir__)
require 'lib/rake/util'
require 'lib/rake/env'

shared_library = "libhivm-#{VERSION}.so"

desc "Build"
task "build" => ['libhivm.a', 'libhivm-db.a']#, "libhivm.so"]
task "default" => ["build", "build:include"]

headers = {
  "vm"        => "hvm",
  "object"    => "hvm_object",
  "symbol"    => "hvm_symbol",
  "chunk"     => "hvm_chunk",
  "generator" => "hvm_generator",
  "bootstrap" => "hvm_bootstrap",
  "exception" => "hvm_exception",
  "debug"     => "hvm_debug"
}
headers.each do |src, dst|
  file "include/#{dst}.h" => "src/#{src}.h" do |t|
    sh "cp -p #{t.prerequisites[0]} #{t.name}"
  end
end

namespace "build" do
  desc "Build include directory (header files)"
  task "include" => headers.values.map {|dst| "include/#{dst}.h" }

  desc "Build the shared library"
  task "shared" => shared_library
end

objects = [
  # Source
  'src/vm.o', 'src/object.o', 'src/symbol.o', 'src/frame.o', 'src/chunk.o',
  'src/generator.o', 'src/bootstrap.o', 'src/exception.o', 'src/gc1.o',
  'src/jit-tracer.o', 'src/jit-compiler-llvm.o',
  # Generated source
  'src/chunk.pb-c.o'
]
debug_objects = objects.map do |file|
  (file == 'src/vm.o') ? 'src/vm-db.o' : file
end
debug_objects << 'src/debug.o'

static_archiver = lambda do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "#{AR} rcs #{t.name} #{t.prerequisites.join ' '}"
end

# Generating the static libraries
file 'libhivm.a' => objects, &static_archiver
file 'libhivm-db.a' => debug_objects, &static_archiver


# Don't use the giant LLVM-bundled JIT compiler object in the dynamic library
shared_objects = objects.map do |file|
  (file == 'src/jit-compiler-llvm.o') ? 'src/jit-compiler.o' : file
end

file shared_library => shared_objects do |t|
  objects = t.prerequisites.join ' '
  # Static libraries
  ldflags = '-liconv -lz -lpthread -ledit -lcurses -lm -lc++ '
  ldflags << `pkg-config --libs glib-2.0 lua5.1 libprotobuf-c`.strip+' '
  # Add the LLVM dynamic library
  llvm_version = `#{LLVM_CONFIG} --version`.strip
  ldflags << "-L#{LLVM_LIBDIR} -lLLVM-#{llvm_version} "
  if `uname -s`.strip == 'Darwin'
    ldflags += " -macosx_version_min 10.10"
  end
  sh "#{LD} #{objects} #{ldflags} -dylib -o #{t.name}"
end


file 'src/jit-compiler-llvm.o' => 'src/jit-compiler.o' do |t|
  llvm_libs    = `#{LLVM_CONFIG} --libs #{LLVM_MODULES}`.gsub("\n", '').strip
  llvm_ldflags = "-L#{LLVM_LIBDIR} #{llvm_libs}"
  # -r remerges into new file
  sh "#{LD} #{t.prerequisites.first} #{llvm_ldflags} -r -o #{t.name}"
end

# file 'src/object-jemalloc.o' => 'src/object.o' do |t|
#   # Remerge to pull in jemalloc
#   jemalloc = find_jemalloc
#   sh "#{LD} #{t.prerequisites.first} #{jemalloc} -r -o #{t.name}"
# end

file "src/chunk.pb-c.c" => ["src/chunk.proto"] do |t|
  sh "protoc-c --c_out=. #{t.prerequisites.first}"
end

# Let Rake know that debug.o depends on debug-lua.include.c
file 'src/debug.o' => 'src/debug-lua.include.c'
# Ditto for vm-dispatch
file 'src/vm.o' => ['src/vm.c', 'src/vm-dispatch.include.c']

# Generic compilation of object files
rule '.o' => ['.c'] do |t|
  sh "#{CC} #{t.source} -c #{cflags_for t.name} -o #{t.name}"
  # if File.basename(t.name) == "object.o"
  #   # -r merges object files into a new object file.
  #   # TODO: Make this platform-independent (fiddle with pkg-config?)
  #   libs = [
  #     "/usr/local/Cellar/glib/2.38.2/lib/libglib-2.0.a",
  #     "/usr/local/Cellar/gettext/0.18.3.2/lib/libintl.a"
  #   ]
  #   sh "#{ld} -r #{t.name} #{libs.join ' '} -o #{t.name}"
  # end
  jemalloc = find_jemalloc
  if File.basename(t.name) == 'object.o'
    # a.o -> a.tmp.o
    tmp = t.name.sub /\.o$/, '.tmp.o'
    sh "mv #{t.name} #{tmp}"
    sh "#{LD} #{tmp} #{jemalloc} -r -o #{t.name}"
  end
end

# Compiling the debug version of vm.c (include the dispatcher as a dependency)
file 'src/vm-db.o' => ['src/vm.c', 'src/vm-dispatch.include.c'] do |t|
  sh "#{CC} #{t.prerequisites.first} -c #{cflags_for t.name} -o #{t.name}"
end

desc "Alias for clean:objects"
task 'clean' => ['clean:objects']

namespace 'clean' do
  desc 'Clean up objects'
  task 'objects' do
    sh "rm -f src/*.o"
    sh "rm -f lib*.*"
    # sh "rm test/*.o"
  end

  desc 'Clean up copied headers'
  task 'headers' do
    sh "rm -f include/*.h"
  end

  desc 'Clean up generated source files'
  task 'generated' do
    sh 'rm -f src/chunk.pb-c.*'
  end

  desc 'Clean up everything (objects, docs)'
  task 'all' => ['clean:objects', 'clean:headers', 'clean:generated', 'doc:clean'] do
    sh 'rm include/*.h'
  end
end

desc "Build documentation"
task "doc" do
  sh "doxygen Doxyfile"
end

namespace "doc" do
  desc "Clean up documentation"
  task "clean" do
    sh "rm -rf doc/*"
  end
end
